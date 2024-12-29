#pragma once

#include <QQuickPaintedItem>
#include <QPixmap>
#include <QImage>
#include <QList>
#include <QMutex>
#include <QSize>

#include <libcamera/formats.h>
#include <libcamera/framebuffer.h>
#include <libcamera/pixel_format.h>
#include <libcamera/color_space.h>

#include "format_converter.h"

class LibCameraView : public QQuickPaintedItem
{
    Q_OBJECT
    Q_PROPERTY(int refreshRateLimit READ refreshRateLimit WRITE setRefreshRateLimit NOTIFY refreshRateLimitChanged FINAL)
    QML_ELEMENT

public:
    LibCameraView(QQuickItem *parent = nullptr);

    void stop();

    QImage getCurrentImage();

    int refreshRateLimit() const;
    void setRefreshRateLimit(int newRefreshRateLimit);

    quint64 imageTimestamp() const;

public Q_SLOTS:
    void onProcessCompleted(QImage image, quint64 timestamp);

protected:
    void paint(QPainter *painter) override;
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;

Q_SIGNALS:
    void refreshRateLimitChanged();

private:
    int refreshRateLimit_;
    qint64 nextRenderTime_;
    QImage image_;
    quint64 imageTimestamp_;
    QRectF place_;
};

