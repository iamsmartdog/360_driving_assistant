#include "BirdRecordService.h"
#include "services/video/common/VideoFrameProvider.h"
#include "services/config/AppConfig.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QDir>
#include <QDebug>

#include <opencv2/imgproc.hpp>

#include <cmath>

// ==========================================================================
// 几何常量简写 (与 param_settings.py 一致)
// ==========================================================================
using namespace BirdViewParams;

// 四路相机名称
static const char *CAM_NAMES[4] = {"front", "back", "left", "right"};

// ==========================================================================
// 子图区域 cv::Rect 辅助函数
// ==========================================================================

// --- Front/Back 投影图 (rows=YT, cols=TOTAL_W) 的子区域 ---
// Python: front[:, :xl] → 全行, 左 xl 列
static cv::Rect rectLeftHalf()   { return cv::Rect(0, 0, XL, YT); }              // (0,0,500,550)
// Python: front[:, xr:] → 全行, 右 (TOTAL_W-XR) 列
static cv::Rect rectRightHalf()  { return cv::Rect(XR, 0, TOTAL_W - XR, YT); }   // (700,0,500,550)
// Python: front[:, xl:xr] → 全行, 中间 (XR-XL) 列
static cv::Rect rectMidCol()     { return cv::Rect(XL, 0, XR - XL, YT); }        // (500,0,200,550)

// --- Left/Right 投影翻转图 (rows=TOTAL_H, cols=XL) 的子区域 ---
// Python: left[:yt, :] → 上 YT 行
static cv::Rect rectTopHalf()    { return cv::Rect(0, 0, XL, YT); }              // (0,0,500,550)
// Python: left[yb:, :] → 下 (TOTAL_H-YB) 行
static cv::Rect rectBottomHalf() { return cv::Rect(0, YB, XL, TOTAL_H - YB); }   // (0,1050,500,550)
// Python: left[yt:yb, :] → 中间 (YB-YT) 行
static cv::Rect rectMidRow()     { return cv::Rect(0, YT, XL, YB - YT); }        // (0,550,500,500)

// --- 画布区域 ---
static cv::Rect rectFL() { return cv::Rect(0, 0, XL, YT); }                      // 左上角
static cv::Rect rectF()  { return cv::Rect(XL, 0, XR - XL, YT); }                // 上中
static cv::Rect rectFR() { return cv::Rect(XR, 0, TOTAL_W - XR, YT); }           // 右上角
static cv::Rect rectBL() { return cv::Rect(0, YB, XL, TOTAL_H - YB); }           // 左下角
static cv::Rect rectB()  { return cv::Rect(XL, YB, XR - XL, TOTAL_H - YB); }     // 下中
static cv::Rect rectBR() { return cv::Rect(XR, YB, TOTAL_W - XR, TOTAL_H - YB); }// 右下角
static cv::Rect rectL()  { return cv::Rect(0, YT, XL, YB - YT); }                // 左中
static cv::Rect rectC()  { return cv::Rect(XL, YT, XR - XL, YB - YT); }          // 中心(车辆)
static cv::Rect rectR()  { return cv::Rect(XR, YT, TOTAL_W - XR, YB - YT); }     // 右中

// ==========================================================================
// 亮度均衡辅助函数 (移植自 surround_view/utils.py)
// ==========================================================================

// mean_luminance_ratio(grayA, grayB, mask) = sum(grayA*mask) / sum(grayB*mask)
static double meanLuminanceRatio(const cv::Mat &grayA, const cv::Mat &grayB,
                                 const cv::Mat &mask)
{
    cv::Mat fA, fB;
    grayA.convertTo(fA, CV_32F);
    grayB.convertTo(fB, CV_32F);
    cv::Mat prodA, prodB;
    cv::multiply(fA, mask, prodA);
    cv::multiply(fB, mask, prodB);
    double sumA = cv::sum(prodA)[0];
    double sumB = cv::sum(prodB)[0];
    if (sumB == 0.0) return 1.0;
    return sumA / sumB;
}

// adjust_luminance(gray, factor) = min(gray * factor, 255).astype(uint8)
static cv::Mat adjustLuminance(const cv::Mat &gray, double factor)
{
    cv::Mat result;
    gray.convertTo(result, CV_32F, factor);
    cv::threshold(result, result, 255.0, 255.0, cv::THRESH_TRUNC);
    result.convertTo(result, CV_8U);
    return result;
}

// tune(x): 亮度调整因子平滑函数
static double tune(double x)
{
    if (x >= 1.0)
        return x * std::exp((1.0 - x) * 0.5);
    else
        return x * std::exp((1.0 - x) * 0.8);
}

// apply_white_balance(image): 按通道均值缩放
static cv::Mat applyWhiteBalance(const cv::Mat &image)
{
    std::vector<cv::Mat> channels;
    cv::split(image, channels);
    double m1 = cv::mean(channels[0])[0];  // B
    double m2 = cv::mean(channels[1])[0];  // G
    double m3 = cv::mean(channels[2])[0];  // R
    if (m1 < 1e-6) m1 = 1e-6;
    if (m2 < 1e-6) m2 = 1e-6;
    if (m3 < 1e-6) m3 = 1e-6;
    double K = (m1 + m2 + m3) / 3.0;
    channels[0] = adjustLuminance(channels[0], K / m1);
    channels[1] = adjustLuminance(channels[1], K / m2);
    channels[2] = adjustLuminance(channels[2], K / m3);
    cv::Mat result;
    cv::merge(channels, result);
    return result;
}

// ==========================================================================
// 加权融合: merge(A, B, weight) = A * weight + B * (1 - weight)
// ==========================================================================
static cv::Mat mergeImages(const cv::Mat &A, const cv::Mat &B, const cv::Mat &weight)
{
    cv::Mat Af, Bf, invWeight, result;
    A.convertTo(Af, CV_32F);
    B.convertTo(Bf, CV_32F);
    // invWeight = 1.0 - weight (weight 是3通道, 用 Scalar 做3通道减法)
    cv::subtract(cv::Scalar(1.0, 1.0, 1.0), weight, invWeight);
    cv::Mat weightedA, weightedB;
    cv::multiply(Af, weight, weightedA);
    cv::multiply(Bf, invWeight, weightedB);
    cv::add(weightedA, weightedB, result);
    result.convertTo(result, CV_8U);
    return result;
}

// ==========================================================================
// FisheyeCameraModel 实现
// ==========================================================================

void FisheyeCameraModel::buildMaps()
{
    // newK = K.copy(); newK[0,0]*=sx; newK[1,1]*=sy; newK[0,2]+=dx; newK[1,2]+=dy
    cv::Mat newK = K.clone();
    newK.at<double>(0, 0) *= scale_x;
    newK.at<double>(1, 1) *= scale_y;
    newK.at<double>(0, 2) += shift_x;
    newK.at<double>(1, 2) += shift_y;

    cv::Mat R = cv::Mat::eye(3, 3, CV_64F);
    // ⚠️ resolution 在 Python 中是 (W, H)，cv::Size 也是 (W, H)
    cv::fisheye::initUndistortRectifyMap(K, D, R, newK, resolution,
                                          CV_16SC2, map1, map2);
}

cv::Mat FisheyeCameraModel::undistort(const cv::Mat &src) const
{
    cv::Mat dst;
    cv::remap(src, dst, map1, map2, cv::INTER_LINEAR, cv::BORDER_CONSTANT);
    return dst;
}

cv::Mat FisheyeCameraModel::project(const cv::Mat &undist) const
{
    cv::Mat dst;
    cv::warpPerspective(undist, dst, P, projectShape);
    return dst;
}

cv::Mat FisheyeCameraModel::flipDirection(const cv::Mat &proj) const
{
    cv::Mat dst;
    if (name == "front") {
        dst = proj.clone();
    } else if (name == "back") {
        // Python: proj[::-1, ::-1, :] → 180°翻转
        cv::flip(proj, dst, -1);
    } else if (name == "left") {
        // Python: cv2.transpose(proj)[::-1] → 转置后行翻转
        cv::Mat tmp;
        cv::transpose(proj, tmp);
        cv::flip(tmp, dst, 0);
    } else {
        // right: Python: np.flip(cv2.transpose(proj), 1) → 转置后列翻转
        cv::Mat tmp;
        cv::transpose(proj, tmp);
        cv::flip(tmp, dst, 1);
    }
    return dst;
}

// ==========================================================================
// BirdRecordService 实现
// ==========================================================================

BirdRecordService::BirdRecordService(VideoFrameProvider *provider, QObject *parent)
    : QObject(parent)
    , m_frameProvider(provider)
    , m_captureTimer(new QTimer(this))
    , m_videoDir(AppConfig::instance().videoDir())
{
    // 采集定时器: 33ms ≈ 30fps
    m_captureTimer->setInterval(33);
    connect(m_captureTimer, &QTimer::timeout, this, &BirdRecordService::onCaptureTimer);

    // 确保视频目录存在
    QDir dir(m_videoDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
}

BirdRecordService::~BirdRecordService()
{
    stop();
}

// ---------------------------------------------------------------------------
// 资源路径解析 (通过 AppConfig 统一查找 resources/ 目录)
// ---------------------------------------------------------------------------
QString BirdRecordService::resolveAsset(const QString &relativePath) const
{
    QString resolved = AppConfig::instance().resolveResource("surround_view/" + relativePath);
    if (!resolved.isEmpty()) {
        return resolved;
    }
    // 未找到时返回候选路径（让后续 imread/FileStorage 报具体错误）
    return QCoreApplication::applicationDirPath() + "/../resources/surround_view/" + relativePath;
}

// ---------------------------------------------------------------------------
// 加载单个相机 yaml 参数
// ---------------------------------------------------------------------------
bool BirdRecordService::loadCameraModel(const QString &name, FisheyeCameraModel &cam)
{
    cam.name = name;

    QString yamlPath = resolveAsset("yaml/" + name + ".yaml");
    cv::FileStorage fs(yamlPath.toStdString(), cv::FileStorage::READ);
    if (!fs.isOpened()) {
        qWarning() << "BirdRecordService: 无法打开 yaml:" << yamlPath;
        return false;
    }

    fs["camera_matrix"] >> cam.K;
    fs["dist_coeffs"] >> cam.D;

    // resolution: 2×1 int → (W, H)
    cv::Mat resMat;
    fs["resolution"] >> resMat;
    cam.resolution = cv::Size(
        static_cast<int>(resMat.at<int>(0, 0)),
        static_cast<int>(resMat.at<int>(1, 0))
    );

    fs["project_matrix"] >> cam.P;

    // ⚠️ scale_xy / shift_xy 是 dt:f (CV_32F), 必须用 at<float> 读取
    cv::Mat scaleMat, shiftMat;
    fs["scale_xy"] >> scaleMat;
    fs["shift_xy"] >> shiftMat;
    if (!scaleMat.empty()) {
        cam.scale_x = scaleMat.at<float>(0, 0);
        cam.scale_y = scaleMat.at<float>(1, 0);
    }
    if (!shiftMat.empty()) {
        cam.shift_x = shiftMat.at<float>(0, 0);
        cam.shift_y = shiftMat.at<float>(1, 0);
    }

    fs.release();

    // 投影输出尺寸
    if (name == "front" || name == "back")
        cam.projectShape = cv::Size(TOTAL_W, YT);   // (1200, 550)
    else
        cam.projectShape = cv::Size(TOTAL_H, XL);   // (1600, 500)

    cam.buildMaps();
    return true;
}

// ---------------------------------------------------------------------------
// 加载 weights.png 和 masks.png
// ⚠️ PIL 存为 RGBA, OpenCV IMREAD_UNCHANGED 读为 BGRA, 必须 BGRA→RGBA 再 split
// ---------------------------------------------------------------------------
bool BirdRecordService::loadWeightsAndMasks()
{
    // --- weights.png: 4通道 RGBA, 每通道 k=0..3 对应 FL/FR/BL/BR 融合权重 ---
    QString weightsPath = resolveAsset("weights.png");
    cv::Mat weightsBGRA = cv::imread(weightsPath.toStdString(), cv::IMREAD_UNCHANGED);
    if (weightsBGRA.empty() || weightsBGRA.channels() != 4) {
        qWarning() << "BirdRecordService: weights.png 加载失败或非4通道:" << weightsPath;
        return false;
    }
    cv::Mat weightsRGBA;
    cv::cvtColor(weightsBGRA, weightsRGBA, cv::COLOR_BGRA2RGBA);
    std::vector<cv::Mat> wChannels;
    cv::split(weightsRGBA, wChannels);
    for (int k = 0; k < 4; ++k) {
        // 单通道 → float [0,1] → 复制为3通道 (与 BGR 图像通道数匹配)
        cv::Mat wFloat;
        wChannels[k].convertTo(wFloat, CV_32F, 1.0 / 255.0);
        std::vector<cv::Mat> triple = {wFloat, wFloat, wFloat};
        cv::merge(triple, m_weights[k]);
    }

    // --- masks.png: 4通道 RGBA, 每通道 k=0..3 对应 FL/FR/BL/BR 亮度掩膜 ---
    QString masksPath = resolveAsset("masks.png");
    cv::Mat masksBGRA = cv::imread(masksPath.toStdString(), cv::IMREAD_UNCHANGED);
    if (masksBGRA.empty() || masksBGRA.channels() != 4) {
        qWarning() << "BirdRecordService: masks.png 加载失败或非4通道:" << masksPath;
        return false;
    }
    cv::Mat masksRGBA;
    cv::cvtColor(masksBGRA, masksRGBA, cv::COLOR_BGRA2RGBA);
    std::vector<cv::Mat> mChannels;
    cv::split(masksRGBA, mChannels);
    for (int k = 0; k < 4; ++k) {
        // 二值掩膜: 0 或 255 → 0.0 或 1.0
        mChannels[k].convertTo(m_masks[k], CV_32F, 1.0 / 255.0);
    }

    return true;
}

// ---------------------------------------------------------------------------
// 加载 car.png 并 resize 到 (XR-XL, YB-YT) = (200, 500)
// ---------------------------------------------------------------------------
bool BirdRecordService::loadCarIcon()
{
    QString carPath = resolveAsset("images/car.png");
    cv::Mat car = cv::imread(carPath.toStdString());
    if (car.empty()) {
        qWarning() << "BirdRecordService: car.png 加载失败:" << carPath;
        return false;
    }
    cv::resize(car, m_carIcon, cv::Size(XR - XL, YB - YT));
    return true;
}

// ---------------------------------------------------------------------------
// 加载前/后/左/右静态 PNG (读取一次, 每帧复用; front 额外 resize 到标定分辨率)
// ---------------------------------------------------------------------------
bool BirdRecordService::loadStaticFrames()
{
    // m_staticFrames[0]=back, [1]=left, [2]=right
    static const char *STATIC_NAMES[3] = {"back", "left", "right"};
    for (int i = 0; i < 3; ++i) {
        QString imgPath = resolveAsset(
            QString("images/%1.png").arg(STATIC_NAMES[i]));
        m_staticFrames[i] = cv::imread(imgPath.toStdString());
        if (m_staticFrames[i].empty()) {
            qWarning() << "BirdRecordService: 静态图片加载失败:" << imgPath;
            return false;
        }
    }

    // front 作为前视源 (左侧拼接输入 + 右侧原始画面), resize 到 front 相机标定分辨率后缓存
    QString frontPath = resolveAsset("images/front.png");
    cv::Mat front = cv::imread(frontPath.toStdString());
    if (front.empty()) {
        qWarning() << "BirdRecordService: front.png 加载失败:" << frontPath;
        return false;
    }
    if (front.size() != m_cams[0].resolution) {
        cv::resize(front, m_frontFrame, m_cams[0].resolution);
    } else {
        m_frontFrame = front;
    }
    return true;
}

// ---------------------------------------------------------------------------
// 一次性加载标定数据 (yaml/weights/masks/car, 不含图像处理)
// ---------------------------------------------------------------------------
bool BirdRecordService::loadCalibrationData()
{
    try {
        // 1. 加载相机模型
        for (int i = 0; i < 4; ++i) {
            if (!loadCameraModel(CAM_NAMES[i], m_cams[i])) {
                qWarning() << "BirdRecordService: 加载相机模型失败:" << CAM_NAMES[i];
                return false;
            }
        }

        // 2. 加载权重和掩膜
        if (!loadWeightsAndMasks()) {
            qWarning() << "BirdRecordService: 加载 weights/masks 失败";
            return false;
        }

        // 3. 加载车辆图标
        if (!loadCarIcon()) {
            qWarning() << "BirdRecordService: 加载 car.png 失败";
            return false;
        }

        // 4. 加载后/左/右静态图片
        if (!loadStaticFrames()) {
            qWarning() << "BirdRecordService: 加载静态图片失败";
            return false;
        }

        return true;
    } catch (const cv::Exception &e) {
        qWarning() << "BirdRecordService: OpenCV异常:" << e.what();
        return false;
    } catch (const std::exception &e) {
        qWarning() << "BirdRecordService: 异常:" << e.what();
        return false;
    } catch (...) {
        qWarning() << "BirdRecordService: 未知异常";
        return false;
    }
}

// ---------------------------------------------------------------------------
// 亮度均衡 (移植自 birdview.py make_luminance_balance)
// ---------------------------------------------------------------------------
void BirdRecordService::makeLuminanceBalance()
{
    // 分离各相机的 B/G/R 通道
    std::vector<cv::Mat> F(3), B(3), L(3), R(3);
    cv::split(m_cams[0].projected, F);  // front
    cv::split(m_cams[1].projected, B);  // back
    cv::split(m_cams[2].projected, L);  // left
    cv::split(m_cams[3].projected, R);  // right

    // 四角亮度比 (使用各角对应的 mask)
    // a = RII/FII (FR角, mask=m2), b = BIV/RIV (BR角, mask=m4)
    // c = LIII/BIII (BL角, mask=m3), d = FI/LI (FL角, mask=m1)
    double a[3], b[3], c[3], d[3];
    for (int ch = 0; ch < 3; ++ch) {
        a[ch] = meanLuminanceRatio(R[ch](rectTopHalf()),    F[ch](rectRightHalf()),  m_masks[1]);
        b[ch] = meanLuminanceRatio(B[ch](rectRightHalf()),  R[ch](rectBottomHalf()), m_masks[3]);
        c[ch] = meanLuminanceRatio(L[ch](rectBottomHalf()), B[ch](rectLeftHalf()),   m_masks[2]);
        d[ch] = meanLuminanceRatio(F[ch](rectLeftHalf()),   L[ch](rectTopHalf()),    m_masks[0]);
    }

    // 几何平均
    double t[3];
    for (int ch = 0; ch < 3; ++ch)
        t[ch] = std::pow(a[ch] * b[ch] * c[ch] * d[ch], 0.25);

    // 各相机调整因子
    double x[3], y[3], z[3], w[3];
    for (int ch = 0; ch < 3; ++ch) {
        x[ch] = tune(t[ch] / std::sqrt(d[ch] / a[ch]));  // front
        y[ch] = tune(t[ch] / std::sqrt(b[ch] / c[ch]));  // back
        z[ch] = tune(t[ch] / std::sqrt(c[ch] / d[ch]));  // left
        w[ch] = tune(t[ch] / std::sqrt(a[ch] / b[ch]));  // right
    }

    // 应用亮度调整
    for (int ch = 0; ch < 3; ++ch) {
        F[ch] = adjustLuminance(F[ch], x[ch]);
        B[ch] = adjustLuminance(B[ch], y[ch]);
        L[ch] = adjustLuminance(L[ch], z[ch]);
        R[ch] = adjustLuminance(R[ch], w[ch]);
    }

    // 合并回 3 通道
    cv::merge(F, m_cams[0].projected);
    cv::merge(B, m_cams[1].projected);
    cv::merge(L, m_cams[2].projected);
    cv::merge(R, m_cams[3].projected);
}

// ---------------------------------------------------------------------------
// 拼接: 中间区域直接粘贴, 四角加权融合
// ---------------------------------------------------------------------------
void BirdRecordService::stitchAllParts()
{
    m_birdAvm = cv::Mat::zeros(TOTAL_H, TOTAL_W, CV_8UC3);

    const cv::Mat &front = m_cams[0].projected;
    const cv::Mat &back  = m_cams[1].projected;
    const cv::Mat &left  = m_cams[2].projected;
    const cv::Mat &right = m_cams[3].projected;

    // 中间区域直接粘贴
    front(rectMidCol()).copyTo(m_birdAvm(rectF()));
    back(rectMidCol()).copyTo(m_birdAvm(rectB()));
    left(rectMidRow()).copyTo(m_birdAvm(rectL()));
    right(rectMidRow()).copyTo(m_birdAvm(rectR()));

    // 四角加权融合
    cv::Mat fl = mergeImages(front(rectLeftHalf()),  left(rectTopHalf()),    m_weights[0]);
    fl.copyTo(m_birdAvm(rectFL()));

    cv::Mat fr = mergeImages(front(rectRightHalf()), right(rectTopHalf()),   m_weights[1]);
    fr.copyTo(m_birdAvm(rectFR()));

    cv::Mat bl = mergeImages(back(rectLeftHalf()),   left(rectBottomHalf()), m_weights[2]);
    bl.copyTo(m_birdAvm(rectBL()));

    cv::Mat br = mergeImages(back(rectRightHalf()),  right(rectBottomHalf()), m_weights[3]);
    br.copyTo(m_birdAvm(rectBR()));
}

// ---------------------------------------------------------------------------
// 白平衡
// ---------------------------------------------------------------------------
void BirdRecordService::makeWhiteBalance()
{
    m_birdAvm = applyWhiteBalance(m_birdAvm);
}

// ---------------------------------------------------------------------------
// 中心叠加车辆图标
// ---------------------------------------------------------------------------
void BirdRecordService::pasteCarIcon()
{
    if (m_carIcon.empty()) return;
    m_carIcon.copyTo(m_birdAvm(rectC()));
}

// ---------------------------------------------------------------------------
// 推送帧到 QML (泛化: 支持 bird_avm / bird_cam 等多帧源)
// ---------------------------------------------------------------------------
void BirdRecordService::pushFrame(const QString &id, const cv::Mat &mat)
{
    if (!m_frameProvider || mat.empty()) return;

    // cv::Mat 是 BGR, QImage 需要 RGB (Qt 5.12.8 无 Format_BGR888)
    cv::Mat rgb;
    cv::cvtColor(mat, rgb, cv::COLOR_BGR2RGB);
    QImage img(rgb.data, rgb.cols, rgb.rows,
               static_cast<int>(rgb.step), QImage::Format_RGB888);
    m_frameProvider->updateFrame(id, img.copy());
}

// ---------------------------------------------------------------------------
// 定时器槽: 每帧采集 + 处理 + 推送 + 录制
// ---------------------------------------------------------------------------
void BirdRecordService::onCaptureTimer()
{
    if (m_running) {
        processFrame();
    }
}

// ---------------------------------------------------------------------------
// 每帧处理核心: 采集 → 矫正 → 投影 → 拼接 → 推送 → 录制
// ---------------------------------------------------------------------------
void BirdRecordService::processFrame()
{
    // 1. 前视源: 固定使用缓存的 front.png (已 resize 到 front 相机标定分辨率)
    if (m_frontFrame.empty()) {
        qWarning() << "BirdRecordService: 前视图未加载";
        return;
    }
    cv::Mat frontFrame = m_frontFrame;

    // 2. 组装 4 路源图像
    cv::Mat sources[4];
    sources[0] = frontFrame;            // front = front.png 静态
    sources[1] = m_staticFrames[0];     // back = 静态 PNG
    sources[2] = m_staticFrames[1];     // left = 静态 PNG
    sources[3] = m_staticFrames[2];     // right = 静态 PNG

    // 3. 对每路相机执行 undistort → project → flipDirection
    for (int i = 0; i < 4; ++i) {
        if (sources[i].empty()) continue;
        cv::Mat undist = m_cams[i].undistort(sources[i]);
        cv::Mat proj = m_cams[i].project(undist);
        m_cams[i].projected = m_cams[i].flipDirection(proj);
    }

    // 4. 亮度均衡
    makeLuminanceBalance();

    // 5. 拼接
    stitchAllParts();

    // 6. 白平衡
    makeWhiteBalance();

    // 7. 叠加车辆图标
    pasteCarIcon();

    // 8. 推送鸟瞰图到 QML (左侧)
    pushFrame("bird_avm", m_birdAvm);

    // 9. 推送前视原图到 QML (右侧)
    pushFrame("bird_cam", frontFrame);

    // 10. 录制: 写入帧, 达到 300 帧自动分段
    if (m_isRecording && m_writer.isOpened()) {
        cv::Mat resized;
        cv::resize(m_birdAvm, resized, cv::Size(RECORD_W, RECORD_H));
        m_writer.write(resized);
        m_writtenFrames++;

        // 更新录制时长 (按实际帧数计算)
        int newDuration = m_writtenFrames / RECORD_FPS;
        if (newDuration != m_recordDuration) {
            m_recordDuration = newDuration;
            emit recordDurationChanged();
        }
        emit writtenFramesChanged();

        // 每 300 帧自动分段
        if (m_writtenFrames >= FRAMES_PER_SEGMENT) {
            stopRecording();
            startRecording();
        }
    }

    // 11. 帧计数器递增 (触发 QML Image 刷新)
    m_frameCounter++;
    emit frameCounterChanged();
}

// ---------------------------------------------------------------------------
// 开始新录制段
// ---------------------------------------------------------------------------
void BirdRecordService::startRecording()
{
    if (m_isRecording) return;

    QDateTime now = QDateTime::currentDateTime();
    QString fileName = "鸟瞰_" + now.toString("yyyyMMdd_HHmmss") + ".avi";
    QString filePath = m_videoDir + "/" + fileName;

    int fourcc = cv::VideoWriter::fourcc('M', 'J', 'P', 'G');
    m_writer.open(filePath.toStdString(), fourcc, RECORD_FPS,
                  cv::Size(RECORD_W, RECORD_H), true);

    if (!m_writer.isOpened()) {
        qWarning() << "BirdRecordService: 无法创建视频文件:" << filePath;
        return;
    }

    m_recordFileName = fileName;
    m_writtenFrames = 0;
    m_recordDuration = 0;
    m_isRecording = true;

    emit recordingStateChanged();
    emit recordFileNameChanged();
    emit recordDurationChanged();
    emit writtenFramesChanged();

    qDebug() << "BirdRecordService: 开始录制" << filePath;
}

// ---------------------------------------------------------------------------
// 停止并保存当前录制段
// ---------------------------------------------------------------------------
void BirdRecordService::stopRecording()
{
    if (!m_isRecording) return;

    m_isRecording = false;

    QString filePath = m_videoDir + "/" + m_recordFileName;

    if (m_writer.isOpened()) {
        m_writer.release();
    }

    emit recordingStateChanged();

    qDebug() << "BirdRecordService: 停止录制, 保存文件:" << filePath
             << "帧数:" << m_writtenFrames
             << "时长:" << m_recordDuration << "秒";
}

// ---------------------------------------------------------------------------
// 启动: 加载标定数据 → 开始录制 → 启动定时器
// ---------------------------------------------------------------------------
bool BirdRecordService::start()
{
    if (m_running) return true;

    // 1. 一次性加载标定数据 (yaml/weights/masks/car + 静态图片)
    if (!m_imagesLoaded) {
        if (!loadCalibrationData()) {
            emit buildFailed("鸟瞰图标定数据加载失败，请检查资源文件");
            return false;
        }
        m_imagesLoaded = true;
        m_avmBuilt = true;
        emit imagesLoadedChanged();
    }

    m_running = true;
    emit runningChanged();

    // 2. 开始自动录制
    startRecording();

    // 3. 启动采集定时器
    m_captureTimer->start();

    qDebug() << "BirdRecordService: 服务启动, 实时分屏模式";
    return true;
}

// ---------------------------------------------------------------------------
// 停止: 停止定时器 → 停止录制
// ---------------------------------------------------------------------------
void BirdRecordService::stop()
{
    if (!m_running) return;

    m_running = false;
    m_captureTimer->stop();

    if (m_isRecording) {
        stopRecording();
    }

    emit runningChanged();
    qDebug() << "BirdRecordService: 服务停止";
}
