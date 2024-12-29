#pragma once

#include <QObject>
#include <QThread>
#include <QImage>

#include <libcamera/formats.h>

extern "C" {
    #include <libavcodec/avcodec.h>

    #include <libavutil/opt.h>
    #include <libavutil/imgutils.h>
}

#include "format_converter.h"

class LibCameraThread: public QThread
{
    Q_OBJECT
public:
    explicit LibCameraThread(QObject *parent = nullptr);
    ~LibCameraThread();

    void run() override;
};

class LibCameraProcessWorker: public QObject
{
    Q_OBJECT
public:
    explicit LibCameraProcessWorker(QObject *parent = nullptr);

    virtual void process();

Q_SIGNALS:
    void completed(QImage image, quint64 timestamp);

public Q_SLOTS:
    void onFormatChanged(const libcamera::PixelFormat &format, const QSize &size, unsigned int stride);
    void onFrameReady(QList<QByteArray> dataList, quint64 timestamp);

private:
    qlibcamera::FormatConverter converter_;
    libcamera::PixelFormat format_;
    QSizeF size_;

    QImage image_;
    QByteArray imageBuffer_;
};

class LibCameraSnapshotWorker : public QObject
{
    Q_OBJECT
public:
    explicit LibCameraSnapshotWorker(QObject *parent = nullptr);

Q_SIGNALS:
    void completed(QString filename);

public Q_SLOTS:
    void onFrameReady(QImage image, quint64 timestamp);
};

class LibCameraRecordingWorker : public QObject
{
    Q_OBJECT
public:
    explicit LibCameraRecordingWorker(QObject *parent = nullptr);

Q_SIGNALS:
    void frameRecorded(qint32 frameCount);
    void completed(QString filename, qint32 frameCount);

public Q_SLOTS:
    void onStart(qint32 width, qint32 height, qint32 fps, libcamera::PixelFormat pixelFormat, qint32 bitRate);
    void onFrameReady(QList<QByteArray> dataList, quint64 timestamp);
    void onEnd();

private:
    void encode(AVFrame *frame);

private:
    QString filename_;
    const AVCodec *codec_;
    AVCodecContext *codecContext_;
    FILE *file_;
    AVFrame *frame_;
    AVPacket *packet_;
    libcamera::PixelFormat pixelFormat_;

    bool running_;
    qint32 frameCount_;
};
