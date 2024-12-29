#include "qlibcameraworker.h"
#include <QDebug>
#include <QImage>
#include <QImageWriter>

#include "format_converter_yuv.h"

static const QMap<libcamera::PixelFormat, QImage::Format> nativeFormats
{
//    { libcamera::formats::ABGR8888, QImage::Format_RGBX8888 },
//    { libcamera::formats::XBGR8888, QImage::Format_RGBX8888 },
//    { libcamera::formats::ARGB8888, QImage::Format_RGB32 },
//    { libcamera::formats::XRGB8888, QImage::Format_RGB32 },

    { libcamera::formats::BGR888, QImage::Format_RGB888 },
    { libcamera::formats::RGB888, QImage::Format_BGR888 },
    { libcamera::formats::RGB565, QImage::Format_RGB16 },
};


LibCameraThread::LibCameraThread(QObject *parent)
    : QThread{parent}
{
//    qDebug() << "Thread Id:" << QThread::currentThreadId() << "Func:" << __FUNCTION__;
}

LibCameraThread::~LibCameraThread()
{
//    qDebug() << "Thread Id:" << QThread::currentThreadId() << "Func:" << __FUNCTION__;
}

void LibCameraThread::run()
{
    exec();
}


LibCameraProcessWorker::LibCameraProcessWorker(QObject *parent)
    : QObject{parent}
{

}

void LibCameraProcessWorker::onFormatChanged(const libcamera::PixelFormat &format, const QSize &size, unsigned int stride)
{
    image_ = QImage();

    /*
     * If format conversion is needed, configure the converter and allocate
     * the destination image.
     */
    if (!::nativeFormats.contains(format)) {
        int ret = converter_.configure(format, size, stride);
        if (ret < 0)
            return;

        image_ = QImage(size, QImage::Format_RGB32);

        qDebug() << "Using software format conversion from"
                << format.toString().c_str();
    } else {
        qDebug() << "Zero-copy enabled";
    }

    format_ = format;
    size_ = size;
}

void LibCameraProcessWorker::onFrameReady(QList<QByteArray> dataList, quint64 timestamp)
{
    if (::nativeFormats.contains(format_)) {
        /*
         * If the frame format is identical to the display
         * format, create a QImage that references the frame
         * and store a reference to the frame buffer. The
         * previously stored frame buffer, if any, will be
         * released.
         *
         * \todo Get the stride from the buffer instead of
         * computing it naively
         */
        assert(dataList.size() == 1);
        image_ = QImage((const uchar *)dataList[0].constData(), size_.width(),
                        size_.height(), dataList[0].size() / size_.height(),
                        ::nativeFormats[format_]);
        imageBuffer_ = dataList[0];
    } else {
        // Make a deep copy
        converter_.convert(dataList, &image_);
    }

    process();

    Q_EMIT completed(image_.copy(), timestamp);
}

void LibCameraProcessWorker::process()
{
    // TODO: DO YOUR PROCESSINGS HERE
}

LibCameraSnapshotWorker::LibCameraSnapshotWorker(QObject *parent)
    : QObject{parent}
{

}

void LibCameraSnapshotWorker::onFrameReady(QImage image, quint64 timestamp)
{
    QString filename = QString("%1.jpg").arg(timestamp);
    QImageWriter writer(filename);
    writer.setQuality(95);
    writer.write(image);

    Q_EMIT completed(filename);
}

LibCameraRecordingWorker::LibCameraRecordingWorker(QObject *parent)
    : QObject{parent}, codec_(nullptr), codecContext_(nullptr), file_(nullptr), frame_(nullptr), packet_(nullptr),
    running_(false), frameCount_(0)
{

}

void LibCameraRecordingWorker::onStart(qint32 width, qint32 height, qint32 fps, libcamera::PixelFormat pixelFormat, qint32 bitRate)
{
//    qDebug() << "Thread Id:" << QThread::currentThreadId() << "Func:" << __FUNCTION__;

    int ret;
    filename_ = QString("%1.mp4").arg(QDateTime::currentMSecsSinceEpoch());
    const char* codexName = "h264_v4l2m2m";
    // TODO: YOU CAN USE libx264 FOR BETTER QUALITY
//    const char* codexName = "libx264";

    /* find the mpeg1video encoder */
    codec_ = avcodec_find_encoder_by_name(codexName);
    if (!codec_) {
        qDebug() << QString("Codec '%1' not found").arg(codexName);
        return;
    }

    codecContext_ = avcodec_alloc_context3(codec_);
    if (!codecContext_) {
        qDebug() << "Could not allocate video codec context";
        return;
    }

    packet_ = av_packet_alloc();
    if (!packet_)
        return;

    /* put sample parameters */
    codecContext_->bit_rate = bitRate;
    /* resolution must be a multiple of two */
    codecContext_->width = width;
    codecContext_->height = height;
    /* frames per second */
    codecContext_->time_base = (AVRational){1, fps};
    codecContext_->framerate = (AVRational){fps, 1};

    /* emit one intra frame every ten frames
     * check frame pict_type before passing frame
     * to encoder, if frame->pict_type is AV_PICTURE_TYPE_I
     * then gop_size is ignored and the output of encoder
     * will always be I frame irrespective to gop_size
     */
    codecContext_->gop_size = 10;
    codecContext_->max_b_frames = 1;
    codecContext_->pix_fmt = AV_PIX_FMT_YUV420P;

    if (codec_->id == AV_CODEC_ID_H264)
        av_opt_set(codecContext_->priv_data, "preset", "slow", 0);

    /* open it */
    ret = avcodec_open2(codecContext_, codec_, NULL);
    if (ret < 0) {
        qDebug() << QString("Could not open codec: %1").arg(ret);
        return;
    }

    file_ = fopen(filename_.toStdString().c_str(), "wb");
    if (!file_) {
        qDebug() << QString("Could not open %1").arg(filename_);
        return;
    }

    frame_ = av_frame_alloc();
    if (!frame_) {
        qDebug() << "Could not allocate video frame";
        return;
    }
    frame_->format = codecContext_->pix_fmt;
    frame_->width  = codecContext_->width;
    frame_->height = codecContext_->height;

    ret = av_frame_get_buffer(frame_, 0);
    if (ret < 0) {
        qDebug() << "Could not allocate the video frame data";
        return;
    }

    running_ = true;
    pixelFormat_ = pixelFormat;
}

void LibCameraRecordingWorker::onFrameReady(QList<QByteArray> dataList, quint64 timestamp)
{
    if(!running_) {
        return;
    }

    /* Make sure the frame data is writable.
       On the first round, the frame is fresh from av_frame_get_buffer()
       and therefore we know it is writable.
       But on the next rounds, encode() will have called
       avcodec_send_frame(), and the codec may have kept a reference to
       the frame in its internal structures, that makes the frame
       unwritable.
       av_frame_make_writable() checks that and allocates a new buffer
       for the frame only if necessary.
    */
    int ret = av_frame_make_writable(frame_);
    if (ret < 0)
        return;

    if(pixelFormat_ == libcamera::formats::RGB565) {
        rgb565_to_yuv420((quint16 *)dataList.at(0).data(), frame_->data[0], frame_->data[1], frame_->data[2], codecContext_->width, codecContext_->height);
    }
    else if(pixelFormat_ == libcamera::formats::BGR888) {
        rgb24_to_yuv420((quint8 *)dataList.at(0).data(), frame_->data[0], frame_->data[1], frame_->data[2], codecContext_->width, codecContext_->height);
    }
    else if(pixelFormat_ == libcamera::formats::RGB888) {
        bgr24_to_yuv420((quint8 *)dataList.at(0).data(), frame_->data[0], frame_->data[1], frame_->data[2], codecContext_->width, codecContext_->height);
    }
    else if(pixelFormat_ == libcamera::formats::YUV420) {
        memcpy(frame_->data[0], dataList.at(0).data(), frame_->linesize[0] * codecContext_->height);
        memcpy(frame_->data[1], dataList.at(1).data(), frame_->linesize[1] * codecContext_->height / 2);
        memcpy(frame_->data[2], dataList.at(2).data(), frame_->linesize[2] * codecContext_->height / 2);
    }

    frame_->pts = frameCount_;
    frameCount_ ++;

    /* encode the image */
    encode(frame_);

    Q_EMIT frameRecorded(frameCount_);
}

void LibCameraRecordingWorker::onEnd()
{
    if(running_) {
        uint8_t endcode[] = { 0, 0, 1, 0xb7 };

    //  qDebug() << "Thread Id:" << QThread::currentThreadId() << "Func:" << __FUNCTION__;

        /* flush the encoder */
        encode(NULL);

        /* Add sequence end code to have a real MPEG file.
           It makes only sense because this tiny examples writes packets
           directly. This is called "elementary stream" and only works for some
           codecs. To create a valid file, you usually need to write packets
           into a proper file format or protocol; see mux.c.
         */
        if (codec_->id == AV_CODEC_ID_MPEG1VIDEO || codec_->id == AV_CODEC_ID_MPEG2VIDEO)
            fwrite(endcode, 1, sizeof(endcode), file_);

        Q_EMIT completed(filename_, frameCount_);
    }
    running_ = false;

    if(file_) {
        fclose(file_);
        file_ = nullptr;
    }

    if(codecContext_) {
        avcodec_free_context(&codecContext_);
        codecContext_ = nullptr;
    }

    if(frame_) {
        av_frame_free(&frame_);
        frame_ = nullptr;
    }

    if(packet_) {
        av_packet_free(&packet_);
        packet_ = nullptr;
    }

    codec_ = nullptr;
}

void LibCameraRecordingWorker::encode(AVFrame *frame)
{
    int ret;

    /* send the frame to the encoder */
    if (frame)
        qDebug() << QString("Send frame %1").arg(frame->pts);

    ret = avcodec_send_frame(codecContext_, frame);
    if (ret < 0) {
        qDebug() << "Error sending a frame for encoding";
        return;
    }

    while (ret >= 0) {
        ret = avcodec_receive_packet(codecContext_, packet_);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            return;
        else if (ret < 0) {
            qDebug() << "Error during encoding";
            exit(1);
        }

        qDebug() << QString("Write packet %1 (size=%2)").arg(packet_->pts).arg(packet_->size);
        fwrite(packet_->data, 1, packet_->size, file_);
        av_packet_unref(packet_);
    }
}
