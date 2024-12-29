#include <assert.h>
#include <iomanip>
#include <string>
#include <unistd.h>

#include <libcamera/camera_manager.h>
#include <libcamera/version.h>
#include <libcamera/control_ids.h>

#include <QCoreApplication>

#include <QMutexLocker>
#include <QStandardPaths>
#include <QStringList>
#include <QTimer>
#include <QtDebug>

#include "common/image.h"
#include "qlibcamera.h"
#include "qlibcameraview.h"
#include "qlibcameraworker.h"

static const QMap<LibCamera::Format, libcamera::PixelFormat> formatMap
{
//    { LibCamera::Format_ABGR8888, libcamera::formats::ABGR8888 },
//    { LibCamera::Format_XBGR8888, libcamera::formats::XBGR8888 },
//    { LibCamera::Format_ARGB8888, libcamera::formats::ARGB8888 },
//    { LibCamera::Format_XRGB8888, libcamera::formats::XRGB8888 },

    { LibCamera::Format_BGR888, libcamera::formats::BGR888 },
    { LibCamera::Format_RGB888, libcamera::formats::RGB888 },
    { LibCamera::Format_RGB565, libcamera::formats::RGB565 },
    { LibCamera::Format_YUV420, libcamera::formats::YUV420 },
};

/**
 * \brief Custom QEvent to signal capture completion
 */
class CaptureEvent : public QEvent
{
public:
    CaptureEvent()
        : QEvent(type())
    {
    }

    static Type type()
    {
        static int type = QEvent::registerEventType();
        return static_cast<Type>(type);
    }
};

libcamera::CameraManager *LibCamera::cm_ = nullptr;

LibCamera::LibCamera(QObject *parent)
    : QObject{parent}, view_(nullptr), index_(0), enabled_(false), format_(Format_RGB565), fps_(15), width_(640), height_(480), allocator_(nullptr),
    isCapturing_(false), captureRaw_(false), isRecording_(false), framesRecorded_(0), recordBitRate_(300000)
{
    init();
}

void LibCamera::init()
{
    timerRestart_ = new QTimer(this);
    timerRestart_->setSingleShot(true);
    connect(timerRestart_, &QTimer::timeout, this, &LibCamera::restart);

    initProcessWorker();
    initSnapshotWorker();
    initRecordingWorker();
}

void LibCamera::initProcessWorker()
{
    LibCameraThread *processThread = new LibCameraThread();
    connect(this, &QObject::destroyed, processThread, [processThread]() {
        processThread->quit();
        processThread->wait();
        delete processThread;
    });
    processThread->start();

    LibCameraProcessWorker *processWorker = new LibCameraProcessWorker();
    processWorker->moveToThread(processThread);
    connect(processThread, &QThread::finished, processWorker, &QObject::deleteLater);
    connect(this, &LibCamera::processFormatChanged, processWorker, &LibCameraProcessWorker::onFormatChanged);
    connect(this, &LibCamera::processFrameReady, processWorker, &LibCameraProcessWorker::onFrameReady);
    connect(processWorker, &LibCameraProcessWorker::completed, this, &LibCamera::processCompleted);
}

void LibCamera::initSnapshotWorker()
{
    LibCameraThread *snapshotThread = new LibCameraThread();
    connect(this, &QObject::destroyed, snapshotThread, [snapshotThread]() {
        snapshotThread->quit();
        snapshotThread->wait();
        delete snapshotThread;
    });
    snapshotThread->start();

    LibCameraSnapshotWorker *snapshotWorker = new LibCameraSnapshotWorker();
    snapshotWorker->moveToThread(snapshotThread);
    connect(snapshotThread, &QThread::finished, snapshotWorker, &QObject::deleteLater);
    connect(this, &LibCamera::snapshotFrameReady, snapshotWorker, &LibCameraSnapshotWorker::onFrameReady);
    connect(snapshotWorker, &LibCameraSnapshotWorker::completed, this, &LibCamera::snapshotCompleted);
}

void LibCamera::initRecordingWorker()
{
    LibCameraThread *recordingThread = new LibCameraThread();
    connect(this, &QObject::destroyed, recordingThread, [recordingThread]() {
        recordingThread->quit();
        recordingThread->wait();
        delete recordingThread;
    });
    recordingThread->start();

    LibCameraRecordingWorker *recordingWorker = new LibCameraRecordingWorker();
    recordingWorker->moveToThread(recordingThread);
    connect(recordingThread, &QThread::finished, recordingWorker, &QObject::deleteLater);
    connect(this, &LibCamera::recordingStart, recordingWorker, &LibCameraRecordingWorker::onStart);
    connect(this, &LibCamera::recordingEnd, recordingWorker, &LibCameraRecordingWorker::onEnd);
    connect(this, &LibCamera::recordingFrameReady, recordingWorker, &LibCameraRecordingWorker::onFrameReady);
    connect(recordingWorker, &LibCameraRecordingWorker::frameRecorded, this, &LibCamera::onFrameRecorded);
    connect(recordingWorker, &LibCameraRecordingWorker::completed, this, &LibCamera::recordingCompleted);
}

LibCamera::~LibCamera()
{
    cleanup();
}

bool LibCamera::event(QEvent *e)
{
    if (e->type() == CaptureEvent::type()) {
        processCapture();
        return true;
    }

    return QObject::event(e);
}

void LibCamera::cleanup()
{
    if (camera_) {
        if(this->isRecording()) {
            this->endRecording();
        }

        stopCapture();
        camera_->release();
        camera_.reset();
    }
}

int LibCamera::openCamera()
{
    std::string cameraName;

    std::vector<std::shared_ptr<libcamera::Camera>> cameras = cameraManager()->cameras();
    if (cameras.size() > index_)
        cameraName = cameras[index_]->id();

    if (cameraName == "")
        return -EINVAL;

    /* Get and acquire the camera. */
    camera_ = cameraManager()->get(cameraName);
    if (!camera_) {
        qInfo() << "Camera" << cameraName.c_str() << "not found";
        return -ENODEV;
    }

    if (camera_->acquire()) {
        qInfo() << "Failed to acquire camera";
        camera_.reset();
        return -EBUSY;
    }

    return 0;
}

void LibCamera::restart()
{
    cleanup();
    int ret = openCamera();
    if (ret < 0) {
        QCoreApplication::instance()->quit();
        return;
    }

    if(enabled_) {
        startCapture();
    }
}

int LibCamera::startCapture()
{
    std::vector<libcamera::StreamRole> roles = { libcamera::StreamRole::Viewfinder };
    int ret;

    /* Configure the camera. */
    config_ = camera_->generateConfiguration(roles);
    if (!config_) {
        qWarning() << "Failed to generate configuration from roles";
        return -EINVAL;
    }

    libcamera::StreamConfiguration &vfConfig = config_->at(0);

    std::vector<libcamera::PixelFormat> formats = vfConfig.formats().pixelformats();
    qDebug() << "Supported formats:";
    for(auto format : formats) {
        qDebug() << format.toString();
    }

    vfConfig.pixelFormat = formatMap[format_];
    vfConfig.size = libcamera::Size(width_, height_);

    libcamera::CameraConfiguration::Status validation = config_->validate();
    if (validation == libcamera::CameraConfiguration::Invalid) {
        qWarning() << "Failed to create valid camera configuration";
        return -EINVAL;
    }

    if (validation == libcamera::CameraConfiguration::Adjusted)
        qInfo() << "Stream configuration adjusted to "
                << vfConfig.toString().c_str();

    ret = camera_->configure(config_.get());
    if (ret < 0) {
        qInfo() << "Failed to configure camera";
        return ret;
    }

    /* Store stream allocation. */
    vfStream_ = config_->at(0).stream();
    if (config_->size() == 2)
        rawStream_ = config_->at(1).stream();
    else
        rawStream_ = nullptr;

    Q_EMIT processFormatChanged(vfConfig.pixelFormat,
                                QSize(vfConfig.size.width, vfConfig.size.height),
                                vfConfig.stride);

    /* Allocate and map buffers. */
    allocator_ = new libcamera::FrameBufferAllocator(camera_);
    for (libcamera::StreamConfiguration &config : *config_) {
        libcamera::Stream *stream = config.stream();

        ret = allocator_->allocate(stream);
        if (ret < 0) {
            qWarning() << "Failed to allocate capture buffers";
            goto error;
        }

        for (const std::unique_ptr<libcamera::FrameBuffer> &buffer : allocator_->buffers(stream)) {
            /* Map memory buffers and cache the mappings. */
            std::unique_ptr<qlibcamera::Image> image =
                qlibcamera::Image::fromFrameBuffer(buffer.get(), qlibcamera::Image::MapMode::ReadOnly);
            assert(image != nullptr);
            mappedBuffers_[buffer.get()] = std::move(image);

            /* Store buffers on the free list. */
            freeBuffers_[stream].enqueue(buffer.get());
        }
    }

    /* Create requests and fill them with buffers from the viewfinder. */
    while (!freeBuffers_[vfStream_].isEmpty()) {
        libcamera::FrameBuffer *buffer = freeBuffers_[vfStream_].dequeue();

        std::unique_ptr<libcamera::Request> request = camera_->createRequest();
        if (!request) {
            qWarning() << "Can't create request";
            ret = -ENOMEM;
            goto error;
        }

        // specifiy fps
        std::int64_t value_pair[2] = {1000000 / fps_, 1000000 / fps_};
        request->controls().set(libcamera::controls::FrameDurationLimits, libcamera::Span<const std::int64_t, 2>(value_pair));
        ret = request->addBuffer(vfStream_, buffer);
        if (ret < 0) {
            qWarning() << "Can't set buffer for request";
            goto error;
        }

        requests_.push_back(std::move(request));
    }

    /* Start the title timer and the camera. */
    previousFrames_ = 0;
    framesCaptured_ = 0;
    lastBufferTime_ = 0;

//    struct timespec time;
//    clock_gettime(CLOCK_REALTIME, &time);
//    QThread::sleep(std::chrono::nanoseconds(1000000000 - time.tv_nsec));
//    clock_gettime(CLOCK_REALTIME, &time);
//    qDebug() << (time.tv_sec * 1000 + time.tv_nsec / 1000000);

    ret = camera_->start();
    if (ret) {
        qInfo() << "Failed to start capture";
        goto error;
    }

    camera_->requestCompleted.connect(this, &LibCamera::requestComplete);

    /* Queue all requests. */
    for (std::unique_ptr<libcamera::Request> &request : requests_) {
        ret = queueRequest(request.get());
        if (ret < 0) {
            qWarning() << "Can't queue request";
            goto error_disconnect;
        }
    }

    isCapturing_ = true;

    return 0;

error_disconnect:
    camera_->requestCompleted.disconnect(this);
    camera_->stop();

error:
    requests_.clear();

    mappedBuffers_.clear();

    freeBuffers_.clear();

    delete allocator_;
    allocator_ = nullptr;

    return ret;
}

void LibCamera::stopCapture()
{
    if (!isCapturing_)
        return;

    if(view_)
        view_->stop();
    captureRaw_ = false;

    int ret = camera_->stop();
    if (ret)
        qInfo() << "Failed to stop capture";

    camera_->requestCompleted.disconnect(this);

    mappedBuffers_.clear();

    requests_.clear();
    freeQueue_.clear();

    delete allocator_;

    isCapturing_ = false;

    config_.reset();

    /*
     * A CaptureEvent may have been posted before we stopped the camera,
     * but not processed yet. Clear the queue of done buffers to avoid
     * racing with the event handler.
     */
    freeBuffers_.clear();
    doneQueue_.clear();
}

int LibCamera::queueRequest(libcamera::Request *request)
{
    return camera_->queueRequest(request);
}

void LibCamera::requestComplete(libcamera::Request *request)
{
    if (request->status() == libcamera::Request::RequestCancelled)
        return;

    /*
     * We're running in the libcamera thread context, expensive operations
     * are not allowed. Add the buffer to the done queue and post a
     * CaptureEvent for the application thread to handle.
     */
    {
        QMutexLocker locker(&mutex_);
        doneQueue_.enqueue(request);
    }

    QCoreApplication::postEvent(this, new CaptureEvent);
}

void LibCamera::processCapture()
{
    /*
     * Retrieve the next buffer from the done queue. The queue may be empty
     * if stopCapture() has been called while a CaptureEvent was posted but
     * not processed yet. Return immediately in that case.
     */
    libcamera::Request *request;
    {
        QMutexLocker locker(&mutex_);
        if (doneQueue_.isEmpty())
            return;

        request = doneQueue_.dequeue();
    }

    /* Process buffers. */
    if (request->buffers().count(vfStream_)) {
        libcamera::FrameBuffer *buffer = request->buffers().at(vfStream_);

        // Make a deep copy of the frame buffer
        QList<QByteArray> list;
        for(int i = 0;i < buffer->planes().size(); i ++) {
            size_t size = buffer->metadata().planes()[i].bytesused;
            list.append(QByteArray((const char *)mappedBuffers_[buffer]->data(i).data(), size));
        }


        quint64 timestamp = request->metadata().get(libcamera::controls::SensorTimestamp).value_or(0) / 1000000;
        // TODO: YOU CAN REPLACE SENSOR TIMESTAMP WITH SYSTEM TIMESTAMP
//        quint64 timestamp = QDateTime::currentMSecsSinceEpoch();

        Q_EMIT recordingFrameReady(list, timestamp);
        Q_EMIT processFrameReady(list, timestamp);
        qDebug() << buffer->metadata().sequence << "-" << timestamp;

        processViewfinder(buffer);
    }

    if (request->buffers().count(rawStream_)) {
        processRaw(request->buffers().at(rawStream_), request->metadata());
    }

    request->reuse();
    QMutexLocker locker(&mutex_);
    freeQueue_.enqueue(request);
}

void LibCamera::processRaw(libcamera::FrameBuffer *buffer, const libcamera::ControlList &metadata)
{
#ifdef HAVE_TIFF
    QString defaultPath = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    QString filename = QFileDialog::getSaveFileName(this, "Save DNG", defaultPath,
                                                    "DNG Files (*.dng)");

    if (!filename.isEmpty()) {
        uint8_t *memory = mappedBuffers_[buffer]->data(0).data();
        DNGWriter::write(filename.toStdString().c_str(), camera_.get(),
                         rawStream_->configuration(), metadata, buffer,
                         memory);
    }
#endif

    {
        QMutexLocker locker(&mutex_);
        freeBuffers_[rawStream_].enqueue(buffer);
    }
}

void LibCamera::processViewfinder(libcamera::FrameBuffer *buffer)
{
    framesCaptured_++;

    const libcamera::FrameMetadata &metadata = buffer->metadata();

    curFps_ = metadata.timestamp - lastBufferTime_;
    curFps_ = lastBufferTime_ && curFps_ ? 1000000000.0 / curFps_ : 0.0;
    lastBufferTime_ = metadata.timestamp;

    renderComplete(buffer);
}

void LibCamera::renderComplete(libcamera::FrameBuffer *buffer)
{
    libcamera::Request *request;
    {
        QMutexLocker locker(&mutex_);
        if (freeQueue_.isEmpty())
            return;

        request = freeQueue_.dequeue();
    }

    request->addBuffer(vfStream_, buffer);

    if (captureRaw_) {
        libcamera::FrameBuffer *rawBuffer = nullptr;

        {
            QMutexLocker locker(&mutex_);
            if (!freeBuffers_[rawStream_].isEmpty())
                rawBuffer = freeBuffers_[rawStream_].dequeue();
        }

        if (rawBuffer) {
            request->addBuffer(rawStream_, rawBuffer);
            captureRaw_ = false;
        } else {
            qWarning() << "No free buffer available for RAW capture";
        }
    }

    queueRequest(request);
}

void LibCamera::onFrameRecorded(qint32 frameCount)
{
    framesRecorded_ = frameCount;
}

qint32 LibCamera::recordBitRate() const
{
    return recordBitRate_;
}

void LibCamera::setRecordBitRate(qint32 newRecordBitRate)
{
    if (recordBitRate_ == newRecordBitRate)
        return;
    recordBitRate_ = newRecordBitRate;
    Q_EMIT recordBitRateChanged();
}

qint32 LibCamera::framesRecorded() const
{
    return framesRecorded_;
}

uint32_t LibCamera::framesCaptured() const
{
    return framesCaptured_;
}

bool LibCamera::isRecording() const
{
    return isRecording_;
}

void LibCamera::setIsRecording(bool newIsRecording)
{
    if (isRecording_ == newIsRecording)
        return;
    isRecording_ = newIsRecording;
    Q_EMIT isRecordingChanged();
}

qint32 LibCamera::fps() const
{
    return fps_;
}

void LibCamera::setFps(qint32 newFps)
{
    if (fps_ == newFps)
        return;
    fps_ = newFps;
    Q_EMIT fpsChanged();

    timerRestart_->start(0);
}

qreal LibCamera::curFps() const
{
    return curFps_;
}

LibCamera::Format LibCamera::format() const
{
    return format_;
}

void LibCamera::setFormat(Format newFormat)
{
    if (format_ == newFormat)
        return;
    format_ = newFormat;
    Q_EMIT formatChanged();

    timerRestart_->start(0);
}

void LibCamera::snapshot()
{
    if(!isCapturing_) {
        return;
    }

    Q_EMIT snapshotFrameReady(view_->getCurrentImage(), view_->imageTimestamp());
}

void LibCamera::startRecording()
{
    if(!isCapturing_) {
        return;
    }

    Q_EMIT recordingStart(width_, height_, fps_, formatMap[format_], recordBitRate_);
    setIsRecording(true);
}

void LibCamera::endRecording()
{
    Q_EMIT recordingEnd();
    setIsRecording(false);
}

bool LibCamera::enabled() const
{
    return enabled_;
}

void LibCamera::setEnabled(bool newEnabled)
{
    if (enabled_ == newEnabled)
        return;
    enabled_ = newEnabled;
    Q_EMIT enabledChanged();

    timerRestart_->start(0);
}

qint32 LibCamera::index() const
{
    return index_;
}

void LibCamera::setIndex(qint32 newIndex)
{
    if (index_ == newIndex)
        return;
    index_ = newIndex;
    Q_EMIT indexChanged();

    timerRestart_->start(0);
}

qint32 LibCamera::height() const
{
    return height_;
}

void LibCamera::setHeight(qint32 newHeight)
{
    if (height_ == newHeight)
        return;
    height_ = newHeight;
    Q_EMIT heightChanged();

    timerRestart_->start(0);
}

qint32 LibCamera::width() const
{
    return width_;
}

void LibCamera::setWidth(qint32 newWidth)
{
    if (width_ == newWidth)
        return;
    width_ = newWidth;
    Q_EMIT widthChanged();

    timerRestart_->start(0);
}

LibCameraView *LibCamera::view() const
{
    return view_;
}

void LibCamera::setView(LibCameraView *newView)
{
    if (view_ == newView)
        return;

    if(view_) {
        disconnect(this, &LibCamera::processCompleted, view_, &LibCameraView::onProcessCompleted);
    }

    view_ = newView;
    Q_EMIT viewChanged();

    connect(this, &LibCamera::processCompleted, view_, &LibCameraView::onProcessCompleted);
    timerRestart_->start(0);
}


