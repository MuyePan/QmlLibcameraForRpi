#pragma once

#include <QObject>
#include <QQmlEngine>
#include <memory>
#include <vector>

#include <libcamera/camera.h>
#include <libcamera/camera_manager.h>
#include <libcamera/controls.h>
#include <libcamera/framebuffer.h>
#include <libcamera/framebuffer_allocator.h>
#include <libcamera/request.h>
#include <libcamera/stream.h>

#include <QMutex>
#include <QObject>
#include <QQueue>
#include <QTimer>
#include <QQuickItem>

#include "qlibcameraview.h"

class LibCamera : public QObject
{
    Q_OBJECT
    Q_PROPERTY(LibCameraView *view READ view WRITE setView NOTIFY viewChanged FINAL)
    Q_PROPERTY(qint32 width READ width WRITE setWidth NOTIFY widthChanged FINAL)
    Q_PROPERTY(qint32 height READ height WRITE setHeight NOTIFY heightChanged FINAL)
    Q_PROPERTY(qint32 index READ index WRITE setIndex NOTIFY indexChanged FINAL)
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged FINAL)
    Q_PROPERTY(Format format READ format WRITE setFormat NOTIFY formatChanged FINAL)
    Q_PROPERTY(qreal curFps READ curFps CONSTANT FINAL)
    Q_PROPERTY(qint32 fps READ fps WRITE setFps NOTIFY fpsChanged FINAL)
    Q_PROPERTY(bool isRecording READ isRecording WRITE setIsRecording NOTIFY isRecordingChanged FINAL)
    Q_PROPERTY(uint32_t framesCaptured READ framesCaptured CONSTANT FINAL)
    Q_PROPERTY(qint32 framesRecorded READ framesRecorded CONSTANT FINAL)
    Q_PROPERTY(int recordBitRate READ recordBitRate WRITE setRecordBitRate NOTIFY recordBitRateChanged FINAL)
    QML_ELEMENT

public:
    static libcamera::CameraManager *cameraManager() {
        if(cm_ == nullptr) {
            cm_ = new libcamera::CameraManager();

            int ret = cm_->start();
            if (ret) {
                qInfo() << "Failed to start camera manager:"
                        << strerror(-ret);
            }
        }

        return cm_;
    }

private:
    static libcamera::CameraManager *cm_;
public:
    enum Format {
//        Format_ABGR8888,
//        Format_XBGR8888,
//        Format_ARGB8888,
//        Format_XRGB8888,
        Format_BGR888,
        Format_RGB888,
        Format_RGB565,
        Format_YUV420,
    };
    Q_ENUM(Format)

    explicit LibCamera(QObject *parent = nullptr);
    virtual ~LibCamera();

    void init();
    virtual void initProcessWorker();
    virtual void initSnapshotWorker();
    virtual void initRecordingWorker();

    bool event(QEvent *e) override;

    LibCameraView *view() const;
    void setView(LibCameraView *newView);

    qint32 width() const;
    void setWidth(qint32 newWidth);

    qint32 height() const;
    void setHeight(qint32 newHeight);

    qint32 index() const;
    void setIndex(qint32 newIndex);

    bool enabled() const;
    void setEnabled(bool newEnabled);

    Format format() const;
    void setFormat(Format newFormat);

    qreal curFps() const;

    qint32 fps() const;
    void setFps(qint32 newFps);

    bool isRecording() const;
    void setIsRecording(bool newIsRecording);

    uint32_t framesCaptured() const;

    qint32 framesRecorded() const;

    qint32 recordBitRate() const;
    void setRecordBitRate(qint32 newRecordBitRate);

    Q_INVOKABLE void snapshot();
    Q_INVOKABLE void startRecording();
    Q_INVOKABLE void endRecording();

Q_SIGNALS:
    void viewChanged();

    void widthChanged();

    void heightChanged();

    void indexChanged();

    void enabledChanged();

    void formatChanged();

    void fpsChanged();

    void snapshotFrameReady(QImage image, quint64 timestamp);
    void snapshotCompleted(QString filename);

    void recordingStart(qint32 width, qint32 height, qint32 fps, libcamera::PixelFormat pixelFormat, qint32 bitRate);
    void recordingFrameReady(QList<QByteArray> dataList, quint64 timestamp);
    void recordingEnd();
    void recordingCompleted(QString filename, qint32 frameCount);

    void isRecordingChanged();

    void recordBitRateChanged();

    void processFormatChanged(const libcamera::PixelFormat &format, const QSize &size, unsigned int stride);
    void processFrameReady(QList<QByteArray> dataList, quint64 timestamp);
    void processCompleted(QImage image, quint64 timestamp);

private:
    void cleanup();
    int openCamera();
    void restart();

    int startCapture();
    void stopCapture();

    int queueRequest(libcamera::Request *request);
    void requestComplete(libcamera::Request *request);

    void processCapture();
    void processRaw(libcamera::FrameBuffer *buffer,
                    const libcamera::ControlList &metadata);
    void processViewfinder(libcamera::FrameBuffer *buffer);
    void renderComplete(libcamera::FrameBuffer *buffer);

private Q_SLOTS:
    void onFrameRecorded(int frameCount);

private:
    LibCameraView *view_;
    qint32 width_;
    qint32 height_;
    qint32 index_;
    bool enabled_;
    Format format_;
    qint32 fps_;
    bool isRecording_;
    qint32 recordBitRate_;

    QTimer *timerRestart_;
    qint32 framesRecorded_;

    /* Camera manager, camera, configuration and buffers */
    std::shared_ptr<libcamera::Camera> camera_;
    libcamera::FrameBufferAllocator *allocator_;

    std::unique_ptr<libcamera::CameraConfiguration> config_;
    std::map<libcamera::FrameBuffer *, std::unique_ptr<qlibcamera::Image>> mappedBuffers_;

    /* Capture state, buffers queue and statistics */
    bool isCapturing_;
    bool captureRaw_;
    libcamera::Stream *vfStream_;
    libcamera::Stream *rawStream_;
    std::map<const libcamera::Stream *, QQueue<libcamera::FrameBuffer *>> freeBuffers_;
    QQueue<libcamera::Request *> doneQueue_;
    QQueue<libcamera::Request *> freeQueue_;
    QMutex mutex_; /* Protects freeBuffers_, doneQueue_, and freeQueue_ */

    uint64_t lastBufferTime_;
    uint32_t previousFrames_;
    uint32_t framesCaptured_;
    qreal curFps_;

    std::vector<std::unique_ptr<libcamera::Request>> requests_;
};
