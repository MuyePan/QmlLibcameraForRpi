#include "qlibcameraview.h"
#include <QPainter>

LibCameraView::LibCameraView(QQuickItem *parent)
    : QQuickPaintedItem(parent), place_(boundingRect()), refreshRateLimit_(15), nextRenderTime_(0)
{
    setFillColor(QColor());
}

void LibCameraView::onProcessCompleted(QImage image, quint64 timestamp)
{
    if(QDateTime::currentMSecsSinceEpoch() >= nextRenderTime_) {
        update();
        nextRenderTime_ = QDateTime::currentMSecsSinceEpoch() + 1000 / refreshRateLimit() - 1;
    }
    image_ = image;
    imageTimestamp_ = timestamp;
}

void LibCameraView::stop()
{
    image_ = QImage();
    update();
}

QImage LibCameraView::getCurrentImage()
{
    return image_;
}

void LibCameraView::paint(QPainter *painter)
{
    /* If we have an image, draw it, with black letterbox rectangles. */
    if (!image_.isNull()) {
//        if (place_.width() < width()) {
//            QRectF rect{ 0, 0, (width() - place_.width()) / 2, height() };
//            painter->drawRect(rect);
//            rect.moveLeft(place_.right());
//            painter->drawRect(rect);
//        }
//        else {
//            QRectF rect{ 0, 0, width(), (height() - place_.height()) / 2 };
//            painter->drawRect(rect);
//            rect.moveTop(place_.bottom());
//            painter->drawRect(rect);
//        }

        painter->drawImage(place_, image_, image_.rect());
        return;
    }
}

void LibCameraView::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) {
    place_.setRect(0, 0, newGeometry.width(), newGeometry.height());
    QQuickPaintedItem::geometryChange(newGeometry, oldGeometry);
}

quint64 LibCameraView::imageTimestamp() const
{
    return imageTimestamp_;
}

int LibCameraView::refreshRateLimit() const
{
    return refreshRateLimit_;
}

void LibCameraView::setRefreshRateLimit(int newRefreshRateLimit)
{
    if (refreshRateLimit_ == newRefreshRateLimit)
        return;
    refreshRateLimit_ = newRefreshRateLimit;
    Q_EMIT refreshRateLimitChanged();
}
