#ifndef PLAYBACKDETECTSERVICE_H
#define PLAYBACKDETECTSERVICE_H

#include <QObject>
#include <QString>
#include <QImage>
#include <QVariantList>
#include <QVariantMap>

#include <opencv2/opencv.hpp>
#include <opencv2/imgproc.hpp>

class DetectionEngine;

// 播放检测服务 - 对视频帧执行检测，返回带检测框的帧
// 检测逻辑由 DetectionEngine（YOLOv8n）统一提供
class PlaybackDetectService : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool vehicleDetectEnabled READ vehicleDetectEnabled WRITE setVehicleDetectEnabled NOTIFY vehicleDetectEnabledChanged)
    Q_PROPERTY(bool trafficLightDetectEnabled READ trafficLightDetectEnabled WRITE setTrafficLightDetectEnabled NOTIFY trafficLightDetectEnabledChanged)

public:
    explicit PlaybackDetectService(QObject *parent = nullptr);
    ~PlaybackDetectService();

    bool vehicleDetectEnabled() const;
    void setVehicleDetectEnabled(bool enabled);
    bool trafficLightDetectEnabled() const;
    void setTrafficLightDetectEnabled(bool enabled);

    // QML可调用：对帧执行检测并返回带框的QImage
    Q_INVOKABLE QImage detectAndDrawFrame(const QImage &frame);

    // QML可调用：截取当前帧保存为图片文件
    Q_INVOKABLE QString saveScreenshot(const QImage &frame, const QString &videoDir);

signals:
    void vehicleDetectEnabledChanged();
    void trafficLightDetectEnabledChanged();
    void detectionResultReady(const QImage &annotatedFrame, const QString &detectionInfo);

private:
    // 共享检测引擎（YOLOv8n）
    DetectionEngine *m_detectionEngine;

    bool m_vehicleDetectEnabled;
    bool m_trafficLightDetectEnabled;
};

#endif // PLAYBACKDETECTSERVICE_H
