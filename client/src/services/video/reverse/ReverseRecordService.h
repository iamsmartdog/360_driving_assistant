#ifndef REVERSERECORDSERVICE_H
#define REVERSERECORDSERVICE_H

#include <QObject>
#include <QString>
#include <QTimer>
#include <QDateTime>
#include <QImage>

#include <opencv2/opencv.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/imgproc.hpp>

// 倒车模式录制服务 - 单摄像头+辅助线+障碍物检测，每10秒自动录制300帧
class VideoFrameProvider;

class ReverseRecordService : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool isAutoRecording READ isAutoRecording NOTIFY autoRecordingChanged)
    Q_PROPERTY(bool isAutoRecordEnabled READ isAutoRecordEnabled WRITE setAutoRecordEnabled NOTIFY autoRecordEnabledChanged)
    Q_PROPERTY(int autoRecordIntervalSec READ autoRecordIntervalSec WRITE setAutoRecordIntervalSec NOTIFY autoRecordIntervalChanged)
    Q_PROPERTY(int targetFrames READ targetFrames WRITE setTargetFrames NOTIFY targetFramesChanged)
    Q_PROPERTY(int currentFrameCount READ currentFrameCount NOTIFY currentFrameCountChanged)
    Q_PROPERTY(QString videoDir READ videoDir WRITE setVideoDir NOTIFY videoDirChanged)
    Q_PROPERTY(bool isRunning READ isRunning NOTIFY runningChanged)
    Q_PROPERTY(QString warningLevel READ warningLevel NOTIFY warningLevelChanged)
    Q_PROPERTY(qreal steeringAngle READ steeringAngle WRITE setSteeringAngle NOTIFY steeringAngleChanged)
    Q_PROPERTY(bool useStaticImage READ useStaticImage WRITE setUseStaticImage NOTIFY useStaticImageChanged)
    Q_PROPERTY(QString staticImagePath READ staticImagePath NOTIFY staticImageChanged)
    Q_PROPERTY(int frameCounter READ frameCounter NOTIFY frameCounterChanged)
    Q_PROPERTY(bool useSecondImage READ useSecondImage WRITE setUseSecondImage NOTIFY secondImageChanged)

public:
    explicit ReverseRecordService(VideoFrameProvider *provider, QObject *parent = nullptr);
    ~ReverseRecordService();

    bool isAutoRecording() const;
    bool isAutoRecordEnabled() const;
    void setAutoRecordEnabled(bool enabled);
    int autoRecordIntervalSec() const;
    void setAutoRecordIntervalSec(int sec);
    int targetFrames() const;
    void setTargetFrames(int frames);
    int currentFrameCount() const;
    QString videoDir() const;
    void setVideoDir(const QString &dir);
    bool isRunning() const;
    QString warningLevel() const;
    qreal steeringAngle() const;
    void setSteeringAngle(qreal angle);
    bool useStaticImage() const;
    void setUseStaticImage(bool use);
    QString staticImagePath() const;
    int frameCounter() const;
    bool useSecondImage() const;
    void setUseSecondImage(bool use);

    Q_INVOKABLE bool start();
    Q_INVOKABLE void stop();
    Q_INVOKABLE bool openCamera(int camId = 0);
    Q_INVOKABLE void closeCamera();
    Q_INVOKABLE void steerLeft();
    Q_INVOKABLE void steerRight();
    Q_INVOKABLE void steerCenter();
    Q_INVOKABLE bool loadStaticImage(const QString &path);

signals:
    void autoRecordingChanged();
    void autoRecordEnabledChanged();
    void autoRecordIntervalChanged();
    void targetFramesChanged();
    void currentFrameCountChanged();
    void videoDirChanged();
    void runningChanged();
    void warningLevelChanged();
    void steeringAngleChanged();
    void useStaticImageChanged();
    void staticImageChanged();
    void frameCounterChanged();
    void secondImageChanged();
    void reverseFrameReady(const QImage &frame);     // 倒车辅助画面（含辅助线）
    void recordingSaved(const QString &filePath, int frames);

private slots:
    void onCaptureTimer();
    void onAutoRecordTimer();

private:
    void processFrame();
    void startAutoRecord();
    void stopAutoRecord();
    void drawAuxiliaryLines(cv::Mat &frame);
    void detectObstacles(const cv::Mat &frame);
    bool detectCarRear(const cv::Mat &frame);  // 检测画面中是否有车屁股

    cv::VideoCapture m_capture;
    cv::VideoWriter m_writer;
    bool m_camOpened;

    bool m_autoRecording;
    bool m_autoRecordEnabled;
    int m_autoRecordIntervalSec;
    int m_targetFrames;
    int m_currentFrameCount;
    QString m_videoDir;
    bool m_running;

    QTimer *m_captureTimer;
    QTimer *m_autoRecordTimer;
    QTimer *m_steerAnimTimer;  // 转向动画定时器

    // 辅助线和障碍物检测
    QString m_warningLevel;  // "safe"/"warning"/"danger"
    double m_obstacleDist;   // 障碍物距离(米)，-1=无障碍物
    bool m_hasCarRear;       // 当前画面是否检测到车屁股
    double m_carRearTopRatio; // 车屁股边界在画面中的位置比例(0.0~1.0)，1.0=无车屁股

    // 转向控制
    qreal m_steeringAngle;       // 当前转向角度 -1.0(左) ~ 0(中) ~ 1.0(右)
    qreal m_targetSteerAngle;    // 目标转向角度（动画目标）
    bool m_useStaticImage;       // 是否使用静态图片
    cv::Mat m_staticImage;       // 缓存的静态背景图（主图：有障碍物）
    cv::Mat m_staticImage2;      // 缓存的第二张图（无障碍物）
    QString m_staticImagePath;   // 静态图片路径
    bool m_useSecondImage;       // 是否使用第二张图（切换用）

    // 警告图片
    cv::Mat m_warningImage;      // 缓存的警告图片（带alpha通道）
    bool m_warningImgLoaded;     // 警告图片是否已加载

    // 帧显示
    VideoFrameProvider *m_frameProvider;
    int m_frameCounter;
};

#endif // REVERSERECORDSERVICE_H
