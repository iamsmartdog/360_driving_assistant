#ifndef VIDEOPLAYBACKSERVICE_H
#define VIDEOPLAYBACKSERVICE_H

#include <QObject>
#include <QString>
#include <QTimer>
#include <QImage>
#include <QVariantList>

#include <opencv2/opencv.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/imgproc.hpp>

class VideoFrameProvider;
class DetectionEngine;

// 视频播放服务 - 使用OpenCV读取本地AVI文件，逐帧解码+检测+发送到QML
// 检测逻辑由 DetectionEngine（YOLOv8n）统一提供
class VideoPlaybackService : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool isPlaying READ isPlaying NOTIFY playingChanged)
    Q_PROPERTY(int currentFrame READ currentFrame NOTIFY frameChanged)
    Q_PROPERTY(int totalFrames READ totalFrames NOTIFY videoInfoChanged)
    Q_PROPERTY(double fps READ fps NOTIFY videoInfoChanged)
    Q_PROPERTY(int frameWidth READ frameWidth NOTIFY videoInfoChanged)
    Q_PROPERTY(int frameHeight READ frameHeight NOTIFY videoInfoChanged)
    Q_PROPERTY(bool videoLoaded READ videoLoaded NOTIFY videoInfoChanged)
    Q_PROPERTY(bool vehicleDetectEnabled READ vehicleDetectEnabled WRITE setVehicleDetectEnabled NOTIFY vehicleDetectEnabledChanged)
    Q_PROPERTY(bool trafficLightDetectEnabled READ trafficLightDetectEnabled WRITE setTrafficLightDetectEnabled NOTIFY trafficLightDetectEnabledChanged)
    Q_PROPERTY(int frameCounter READ frameCounter NOTIFY frameCounterChanged)
    // 毫秒级时间属性（比帧数/fps更可靠，避免MJPEG AVI帧数虚高问题）
    Q_PROPERTY(int currentPositionMs READ currentPositionMs NOTIFY positionMsChanged)
    Q_PROPERTY(int totalDurationMs READ totalDurationMs NOTIFY videoInfoChanged)

public:
    explicit VideoPlaybackService(VideoFrameProvider *provider, QObject *parent = nullptr);
    ~VideoPlaybackService();

    bool isPlaying() const;
    int currentFrame() const;
    int totalFrames() const;
    double fps() const;
    int frameWidth() const;
    int frameHeight() const;
    bool videoLoaded() const;
    bool vehicleDetectEnabled() const;
    void setVehicleDetectEnabled(bool enabled);
    bool trafficLightDetectEnabled() const;
    void setTrafficLightDetectEnabled(bool enabled);
    int frameCounter() const;
    int currentPositionMs() const;
    int totalDurationMs() const;

    Q_INVOKABLE bool openVideo(const QString &filePath);
    Q_INVOKABLE void closeVideo();
    Q_INVOKABLE void play();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void seekToFrame(int frame);
    Q_INVOKABLE void seekToSec(double sec);
    Q_INVOKABLE void setPlaybackSpeed(double speed);
    Q_INVOKABLE QImage getCurrentFrameImage();
    Q_INVOKABLE QString saveScreenshot(const QString &videoDir);

signals:
    void playingChanged();
    void frameChanged();
    void videoInfoChanged();
    void vehicleDetectEnabledChanged();
    void trafficLightDetectEnabledChanged();
    void frameCounterChanged();
    void positionMsChanged();
    void newFrameReady(const QImage &frame);
    void playbackFinished();
    void screenshotSaved(const QString &filePath);

private slots:
    void onPlayTimer();

private:
    void processAndSendFrame();
    void detectAndDrawFrame(cv::Mat &frame);

    cv::VideoCapture m_capture;
    QTimer *m_playTimer;
    double m_playbackSpeed;

    bool m_isPlaying;
    int m_currentFrame;
    int m_totalFrames;
    double m_fps;
    int m_frameWidth;
    int m_frameHeight;
    bool m_videoLoaded;

    // 毫秒级时间追踪（解决MJPEG AVI帧数虚高导致进度条不准的问题）
    int m_currentPositionMs;
    int m_totalDurationMs;

    bool m_vehicleDetectEnabled;
    bool m_trafficLightDetectEnabled;

    // 共享检测引擎（YOLOv8n）
    DetectionEngine *m_detectionEngine;

    cv::Mat m_lastFrame;  // 缓存最后一帧用于截图

    // 检测结果缓存（帧跳跃时复用）
    QVariantList m_lastVehicleResults;
    QVariantList m_lastTrafficLightRects;
    QString m_lastTrafficLightState;

    // ImageProvider帧传递
    VideoFrameProvider *m_frameProvider;
    int m_frameCounter;
};

#endif // VIDEOPLAYBACKSERVICE_H
