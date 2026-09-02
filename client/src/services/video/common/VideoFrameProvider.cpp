#include "VideoFrameProvider.h"

VideoFrameProvider::VideoFrameProvider()
    : QQuickImageProvider(QQuickImageProvider::Image)
{
}

QImage VideoFrameProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    QMutexLocker locker(&m_mutex);

    // id可能包含查询参数（如"live?123"），提取实际的帧源id
    QString frameId = id.split('?').first();

    QImage result = m_frames.value(frameId);

    if (result.isNull()) {
        if (size) *size = QSize(0, 0);
        return QImage();
    }

    if (size) {
        *size = result.size();
    }

    if (requestedSize.isValid() && requestedSize.width() > 0 && requestedSize.height() > 0) {
        result = result.scaled(requestedSize, Qt::KeepAspectRatio, Qt::FastTransformation);
    }

    return result;
}

void VideoFrameProvider::updateFrame(const QString &id, const QImage &frame)
{
    QMutexLocker locker(&m_mutex);
    m_frames[id] = frame;
}
