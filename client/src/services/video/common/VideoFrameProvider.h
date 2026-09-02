#ifndef VIDEOFRAMEPROVIDER_H
#define VIDEOFRAMEPROVIDER_H

#include <QQuickImageProvider>
#include <QImage>
#include <QMutex>
#include <QMap>

// 视频帧提供者 - 用于QML Image组件高效显示OpenCV捕获的帧
// 替代base64 data URL方案，解决闪烁问题
// 支持多个帧源（通过id区分，如"live"、"playback"、"reverse"）
class VideoFrameProvider : public QQuickImageProvider
{
public:
    VideoFrameProvider();

    // QML Image通过 "image://videoframe/xxx" 请求图片时调用
    // id为帧源标识，如"live"、"playback"、"reverse"等
    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;

    // 由Service调用，更新指定id的最新帧
    void updateFrame(const QString &id, const QImage &frame);

private:
    QMap<QString, QImage> m_frames;
    QMutex m_mutex;
};

#endif // VIDEOFRAMEPROVIDER_H
