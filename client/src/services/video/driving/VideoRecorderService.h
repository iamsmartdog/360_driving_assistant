#ifndef VIDEORECORDERSERVICE_H
#define VIDEORECORDERSERVICE_H

#include <QObject>
#include <QString>
#include <QTimer>
#include <QDateTime>
#include <QImage>
#include <QVariantList>
#include <QVariantMap>

#include <opencv2/opencv.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/imgproc.hpp>

class VideoFrameProvider;
class DetectionEngine;

// 视频录制服务 - 使用OpenCV实现摄像头采集、录制和车辆识别
// 检测逻辑由 DetectionEngine（YOLOv8n）统一提供
class VideoRecorderService : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool isRecording READ isRecording NOTIFY recordingStateChanged)
    Q_PROPERTY(bool isDetecting READ isDetecting WRITE setDetecting NOTIFY detectingChanged)
    Q_PROPERTY(bool isCameraOpen READ isCameraOpen NOTIFY cameraStateChanged)
    Q_PROPERTY(QString recordFileName READ recordFileName NOTIFY recordFileNameChanged)
    Q_PROPERTY(int recordDuration READ recordDuration NOTIFY recordDurationChanged)
    Q_PROPERTY(QString videoDir READ videoDir WRITE setVideoDir NOTIFY videoDirChanged)
    Q_PROPERTY(QVariantList detectedVehicles READ detectedVehicles NOTIFY detectedVehiclesChanged)
    Q_PROPERTY(QString trafficLightState READ trafficLightState NOTIFY trafficLightStateChanged)
    Q_PROPERTY(bool isTrafficLightDetecting READ isTrafficLightDetecting WRITE setTrafficLightDetecting NOTIFY trafficLightDetectingChanged)
    Q_PROPERTY(int cameraFps READ cameraFps NOTIFY cameraInfoChanged)
    Q_PROPERTY(int cameraWidth READ cameraWidth NOTIFY cameraInfoChanged)
    Q_PROPERTY(int cameraHeight READ cameraHeight NOTIFY cameraInfoChanged)
    Q_PROPERTY(bool isAutoScreenshotEnabled READ isAutoScreenshotEnabled WRITE setAutoScreenshotEnabled NOTIFY autoScreenshotEnabledChanged)
    Q_PROPERTY(int autoScreenshotIntervalSec READ autoScreenshotIntervalSec WRITE setAutoScreenshotIntervalSec NOTIFY autoScreenshotIntervalChanged)
    Q_PROPERTY(int frameCounter READ frameCounter NOTIFY frameCounterChanged)

public:
    explicit VideoRecorderService(VideoFrameProvider *provider, QObject *parent = nullptr);
    ~VideoRecorderService();

    bool isRecording() const;
    bool isDetecting() const;
    void setDetecting(bool detect);
    bool isCameraOpen() const;
    QString recordFileName() const;
    int recordDuration() const;
    QString videoDir() const;
    void setVideoDir(const QString &dir);
    QVariantList detectedVehicles() const;
    QString trafficLightState() const;
    bool isTrafficLightDetecting() const;
    void setTrafficLightDetecting(bool detect);
    int cameraFps() const;
    int cameraWidth() const;
    int cameraHeight() const;
    bool isAutoScreenshotEnabled() const;
    void setAutoScreenshotEnabled(bool enabled);
    int autoScreenshotIntervalSec() const;
    void setAutoScreenshotIntervalSec(int sec);
    int frameCounter() const;

    // QML可调用方法
    Q_INVOKABLE bool startRecording();
    Q_INVOKABLE void stopRecording();
    Q_INVOKABLE bool openCamera(int deviceId = 0);
    Q_INVOKABLE void closeCamera();
    Q_INVOKABLE QString takeManualScreenshot();

signals:
    void recordingStateChanged();
    void detectingChanged();
    void cameraStateChanged();
    void recordFileNameChanged();
    void recordDurationChanged();
    void videoDirChanged();
    void detectedVehiclesChanged();
    void trafficLightStateChanged();
    void trafficLightDetectingChanged();
    void cameraInfoChanged();
    void autoScreenshotEnabledChanged();
    void autoScreenshotIntervalChanged();
    void frameCounterChanged();
    void newFrameReady(const QImage &frame);           // 新帧可用（供QML显示）
    void recordingSaved(const QString &filePath,       // 录制保存完成
                        int durationSec, int fileSizeMB,
                        int width, int height, int fps);
    void screenshotSaved(const QString &filePath,      // 自动截图保存完成
                         const QString &detectionInfo);

private slots:
    void onCaptureTimer();
    void onScreenshotTimer();

private:
    void processFrame();

    cv::VideoCapture m_capture;
    cv::VideoWriter m_writer;

    // 共享检测引擎（YOLOv8n）
    DetectionEngine *m_detectionEngine;

    bool m_isRecording;
    bool m_isDetecting;
    bool m_isTrafficLightDetecting;
    bool m_cameraOpened;
    QString m_recordFileName;
    QString m_videoDir;
    int m_recordDuration;
    int m_writtenFrames;     // 实际写入的帧数（用于计算真实录制时长）
    QTimer *m_captureTimer;
    QTimer *m_durationTimer;
    QVariantList m_detectedVehicles;
    QVariantList m_detectedTrafficLights;  // 红绿灯轮廓位置
    QString m_trafficLightState;  // "red" / "green" / "yellow" / "unknown"

    int m_cameraFps;
    int m_cameraWidth;
    int m_cameraHeight;

    // 自动截图相关
    bool m_isAutoScreenshotEnabled;
    int m_autoScreenshotIntervalSec;
    QTimer *m_screenshotTimer;
    cv::Mat m_lastFrame;  // 缓存最后一帧用于截图

    // ImageProvider帧传递（替代base64，解决闪烁）
    VideoFrameProvider *m_frameProvider;
    int m_frameCounter;  // 帧计数器，QML通过此属性变化触发Image刷新
};

#endif // VIDEORECORDERSERVICE_H
