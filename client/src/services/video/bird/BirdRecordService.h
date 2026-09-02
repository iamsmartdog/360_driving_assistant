#ifndef BIRDRECORDSERVICE_H
#define BIRDRECORDSERVICE_H

#include <QObject>
#include <QString>
#include <QImage>
#include <QTimer>
#include <QDateTime>

#include <opencv2/opencv.hpp>
#include <opencv2/calib3d.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>

// 鸟瞰模式服务 — 实时分屏展示 + 自动录制
//
// 实时流水线 (QTimer 30fps):
//   前/后/左/右(静态PNG, front.png 作为前视源) → undistort(鱼眼矫正)
//   → warpPerspective(IPM投影) → flipDirection(方向翻转)
//   → makeLuminanceBalance(亮度均衡) → stitchAllParts(加权融合拼接)
//   → makeWhiteBalance(白平衡) → pasteCarIcon(车辆图标)
//   → pushFrame("bird_avm") + pushFrame("bird_cam")
//
// 录制: 每 300 帧自动分段保存 AVI (1280×720 MJPG)
//
// 数据来源: client/resources/surround_view/ (images/ + yaml/ + weights.png + masks.png)
// 参考实现: surround_view/fisheye_camera.py + birdview.py + param_settings.py

class VideoFrameProvider;

// 几何常量 (from surround_view/param_settings.py)
// 画布 1200×1600, 车辆中心区域 500..700 × 550..1050
namespace BirdViewParams {
    static constexpr int SHIFT_W     = 300;
    static constexpr int SHIFT_H     = 300;
    static constexpr int INN_SHIFT_W = 20;
    static constexpr int INN_SHIFT_H = 50;
    static constexpr int TOTAL_W     = 600 + 2 * SHIFT_W;        // 1200
    static constexpr int TOTAL_H     = 1000 + 2 * SHIFT_H;       // 1600
    static constexpr int XL          = SHIFT_W + 180 + INN_SHIFT_W; // 500
    static constexpr int XR          = TOTAL_W - XL;                // 700
    static constexpr int YT          = SHIFT_H + 200 + INN_SHIFT_H; // 550
    static constexpr int YB          = TOTAL_H - YT;                // 1050
}

// 鱼眼相机模型 — 封装单路相机的矫正+投影+翻转
struct FisheyeCameraModel {
    cv::Mat K;               // 3×3 CV_64F camera_matrix
    cv::Mat D;               // 4×1 CV_64F dist_coeffs (鱼眼等距模型)
    cv::Mat P;               // 3×3 CV_64F project_matrix (IPM homography)
    cv::Size resolution;     // 输入分辨率 (960, 640)
    double scale_x = 1.0;    // 来自 scale_xy[0] (yaml dt:f)
    double scale_y = 1.0;    // 来自 scale_xy[1]
    double shift_x = 0.0;    // 来自 shift_xy[0]
    double shift_y = 0.0;    // 来自 shift_xy[1]
    cv::Mat map1, map2;      // initUndistortRectifyMap 输出 (CV_16SC2 / CV_16UC1)
    cv::Size projectShape;   // 投影输出尺寸
    QString name;            // "front"/"back"/"left"/"right"
    cv::Mat projected;       // undistort → project → flip 后的结果

    // 构建 undistort 映射表: newK = K, fx*=sx, fy*=sy, cx+=dx, cy+=dy
    void buildMaps();

    // 鱼眼矫正: cv::remap(src, map1, map2)
    cv::Mat undistort(const cv::Mat &src) const;

    // IPM投影: cv::warpPerspective(src, P, projectShape)
    cv::Mat project(const cv::Mat &undist) const;

    // 方向翻转: front=恒等, back=180°, left=transpose+flip(0), right=transpose+flip(1)
    cv::Mat flipDirection(const cv::Mat &proj) const;
};

class BirdRecordService : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool isRunning READ isRunning NOTIFY runningChanged)
    Q_PROPERTY(bool imagesLoaded READ imagesLoaded NOTIFY imagesLoadedChanged)
    Q_PROPERTY(int frameCounter READ frameCounter NOTIFY frameCounterChanged)
    Q_PROPERTY(bool isRecording READ isRecording NOTIFY recordingStateChanged)
    Q_PROPERTY(QString recordFileName READ recordFileName NOTIFY recordFileNameChanged)
    Q_PROPERTY(int recordDuration READ recordDuration NOTIFY recordDurationChanged)
    Q_PROPERTY(int writtenFrames READ writtenFrames NOTIFY writtenFramesChanged)
    Q_PROPERTY(QString videoDir READ videoDir NOTIFY videoDirChanged)

public:
    explicit BirdRecordService(VideoFrameProvider *provider, QObject *parent = nullptr);
    ~BirdRecordService();

    bool isRunning() const { return m_running; }
    bool imagesLoaded() const { return m_imagesLoaded; }
    int frameCounter() const { return m_frameCounter; }
    bool isRecording() const { return m_isRecording; }
    QString recordFileName() const { return m_recordFileName; }
    int recordDuration() const { return m_recordDuration; }
    int writtenFrames() const { return m_writtenFrames; }
    QString videoDir() const { return m_videoDir; }

    Q_INVOKABLE bool start();
    Q_INVOKABLE void stop();

signals:
    void runningChanged();
    void imagesLoadedChanged();
    void frameCounterChanged();
    void recordingStateChanged();
    void recordFileNameChanged();
    void recordDurationChanged();
    void writtenFramesChanged();
    void videoDirChanged();
    void buildFailed(const QString &reason);

private:
    // 资源路径解析 (多路径回退: 安装目录 → 构建目录 → 开发绝对路径)
    QString resolveAsset(const QString &relativePath) const;

    // 一次性加载标定数据 (yaml/weights/masks/car)
    bool loadCalibrationData();

    // 加载单个相机模型 (yaml → K/D/P/scale/shift + buildMaps)
    bool loadCameraModel(const QString &name, FisheyeCameraModel &cam);

    // 加载 weights.png(4通道RGBA融合权重) 和 masks.png(4通道RGBA亮度掩膜)
    bool loadWeightsAndMasks();

    // 加载 car.png 并 resize 到 (XR-XL, YB-YT) = (200, 500)
    bool loadCarIcon();

    // 加载后/左/右静态 PNG (读取一次, 每帧复用)
    bool loadStaticFrames();

    // 亮度均衡: 用 masks 计算四角亮度比, 调整各相机通道亮度
    void makeLuminanceBalance();

    // 拼接: 中间区域直接粘贴, 四角加权融合
    void stitchAllParts();

    // 白平衡: 按通道均值缩放
    void makeWhiteBalance();

    // 中心叠加车辆图标
    void pasteCarIcon();

    // 推送帧到 QML
    void pushFrame(const QString &id, const cv::Mat &mat);

    // 定时器槽: 采集 → 矫正 → 拼接 → 推送 → 录制
    void onCaptureTimer();
    void processFrame();

    // 录制控制
    void startRecording();
    void stopRecording();

private:
    VideoFrameProvider *m_frameProvider;
    FisheyeCameraModel m_cams[4];     // front, back, left, right
    cv::Mat m_weights[4];             // 3通道 CV_32F 融合权重 (0..1)
    cv::Mat m_masks[4];               // 1通道 CV_32F 亮度掩膜 (0或1)
    cv::Mat m_carIcon;                // 车辆图标 (resize后 BGR)
    cv::Mat m_birdAvm;                // 最终鸟瞰图 (1600×1200 BGR)

    // 实时采集
    cv::Mat m_frontFrame;             // 前视源 front.png (载入并 resize 后缓存, 每帧复用)
    cv::Mat m_staticFrames[3];        // back/left/right 静态图片
    QTimer *m_captureTimer;           // 采集定时器 (33ms ≈ 30fps)

    // 录制
    cv::VideoWriter m_writer;
    bool m_isRecording = false;
    QString m_recordFileName;
    QString m_videoDir;
    int m_writtenFrames = 0;
    int m_recordDuration = 0;

    static constexpr int FRAMES_PER_SEGMENT = 300;  // 每段 300 帧 (≈10秒)
    static constexpr int RECORD_FPS = 30;
    static constexpr int RECORD_W = 1280;
    static constexpr int RECORD_H = 720;

    // 状态
    bool m_avmBuilt = false;
    bool m_running = false;
    bool m_imagesLoaded = false;
    int m_frameCounter = 0;
};

#endif // BIRDRECORDSERVICE_H
