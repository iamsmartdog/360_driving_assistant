#include "TrafficLightDetector.h"
#include "services/config/AppConfig.h"

#include <QDebug>
#include <QFileInfo>
#include <QVariantMap>

#include <cmath>

// ============================================================================
// TrafficRules 8类：F0,F1,L0,L1,S0,S1,R0,R1
// F=圆形, L=左箭头, S=直行(上箭头), R=右箭头；0=红, 1=绿
// ============================================================================
const char* TrafficLightDetector::TL_CLASSES[8] = {
    "F0", "F1", "L0", "L1", "S0", "S1", "R0", "R1"
};

// ============================================================================
// 构造
// ============================================================================
TrafficLightDetector::TrafficLightDetector(TrafficLightColorVoter &voter)
    : m_voter(voter)
{
    m_tlLoaded = loadModel();
}

// ============================================================================
// 模型加载 - TrafficRules YOLO11n 红绿灯专用模型
// ============================================================================
bool TrafficLightDetector::loadModel()
{
    QString modelPath = AppConfig::instance().resolveTrafficLightModelPath();

    if (modelPath.isEmpty()) {
        qWarning() << "未找到TrafficRules模型文件，将回退YOLOv8n+HSV判色路径";
        return false;
    }

    // 验证文件大小（trafficrules-yolo11n.onnx约10.5MB，空文件无效）
    QFileInfo fi(modelPath);
    if (fi.size() < 1024 * 1024) {
        qWarning() << "TrafficRules模型文件过小（可能损坏）:" << modelPath
                    << "大小:" << fi.size() << "字节";
        return false;
    }

    try {
        m_tlNet = cv::dnn::readNetFromONNX(modelPath.toStdString());
        m_tlNet.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
        m_tlNet.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
        qDebug() << "TrafficRules YOLO11n红绿灯模型加载成功:" << modelPath;
        return true;
    } catch (const cv::Exception &e) {
        qWarning() << "TrafficRules模型加载失败:" << e.what() << "将回退YOLOv8n+HSV路径";
        return false;
    }
}

// ============================================================================
// TrafficRules 预处理 - 直接 resize 到 640x480（无 letterbox，匹配训练分布）
// X/Y 缩放分离，因为 640x480 非正方形，坐标回映需分别使用
// ============================================================================
void TrafficLightDetector::preprocessTL(const cv::Mat &frame, cv::Mat &blob,
                                          double &scaleX, double &scaleY)
{
    int fw = frame.cols;
    int fh = frame.rows;

    // 直接 resize 到 640x480（拉伸，匹配训练分布，不做 letterbox）
    cv::Mat resized;
    cv::resize(frame, resized, cv::Size(TL_INPUT_WIDTH, TL_INPUT_HEIGHT));

    // BGR→RGB + 归一化到[0,1] + NCHW格式
    blob = cv::dnn::blobFromImage(resized, 1.0 / 255.0,
            cv::Size(TL_INPUT_WIDTH, TL_INPUT_HEIGHT), cv::Scalar(0, 0, 0), true, false);

    // X/Y 缩放因子（不相等，坐标回映分别用）
    scaleX = static_cast<double>(TL_INPUT_WIDTH) / fw;   // 640/fw
    scaleY = static_cast<double>(TL_INPUT_HEIGHT) / fh;  // 480/fh
}

// ============================================================================
// TrafficRules 后处理 - 解析 YOLO11n 输出 [1, 12, 6300]
// 输出格式：每列 = [cx, cy, w, h, F0, F1, L0, L1, S0, S1, R0, R1]
// 8类全部保留（F0=圆形红, F1=圆形绿, L0=左箭红, L1=左箭绿, ...）
// ============================================================================
void TrafficLightDetector::postprocessTL(const cv::Mat &output, int frameWidth, int frameHeight,
                                           double scaleX, double scaleY,
                                           std::vector<Detection> &detections)
{
    // 输出维度：[1, 12, 6300]
    // 12 = 4(坐标) + 8(类别分数)
    // 6300 = 预测框数量
    cv::Mat output2d;
    if (output.dims == 3) {
        // [1, 12, 6300] → [12, 6300]
        int numChannels = output.size[1];  // 12
        output2d = output.reshape(0, numChannels);  // reshape为 [12, 6300]
    } else {
        output2d = output.clone();
    }

    int numPredictions = output2d.cols;  // 6300
    int numClasses = output2d.rows - 4;  // 8

    for (int i = 0; i < numPredictions; ++i) {
        // 读取4个坐标值（前4行）
        float cx = output2d.at<float>(0, i);
        float cy = output2d.at<float>(1, i);
        float w  = output2d.at<float>(2, i);
        float h  = output2d.at<float>(3, i);

        // 找到最高置信度的类别（第4行到第11行）
        float maxScore = 0;
        int maxClassId = -1;
        for (int c = 0; c < numClasses; ++c) {
            float score = output2d.at<float>(4 + c, i);
            if (score > maxScore) {
                maxScore = score;
                maxClassId = c;
            }
        }

        // 置信度过滤
        if (maxScore < TL_CONF_THRESHOLD) continue;

        // 坐标从 640x480 映射回原图（X/Y 缩放分别用）
        int left = static_cast<int>((cx - w / 2.0) / scaleX);
        int top  = static_cast<int>((cy - h / 2.0) / scaleY);
        int bw   = static_cast<int>(w / scaleX);
        int bh   = static_cast<int>(h / scaleY);

        // clamp到图像范围内
        left = std::max(0, std::min(left, frameWidth - 1));
        top  = std::max(0, std::min(top, frameHeight - 1));
        bw   = std::min(bw, frameWidth - left);
        bh   = std::min(bh, frameHeight - top);

        // 小框过滤（放宽到6像素，原 yolov8n 路径 bw<10 过严）
        if (bw < TL_MIN_BOX || bh < TL_MIN_BOX) continue;

        Detection det;
        det.x = left;
        det.y = top;
        det.width = bw;
        det.height = bh;
        det.confidence = maxScore;
        det.classId = maxClassId;
        det.className = (maxClassId >= 0 && maxClassId < 8) ? TL_CLASSES[maxClassId] : "unknown";
        detections.push_back(det);
    }
}

// ============================================================================
// 红绿灯颜色分析 - 在YOLO检测框内HSV分析
// ============================================================================
QString TrafficLightDetector::analyzeTrafficLightColor(const cv::Mat &frame,
                                                         const Detection &tlDet)
{
    // 面积过小不分析
    if (tlDet.width * tlDet.height < TL_MIN_AREA) {
        return "unknown";
    }

    // 裁剪ROI（确保不越界）
    int x1 = std::max(0, tlDet.x);
    int y1 = std::max(0, tlDet.y);
    int x2 = std::min(frame.cols, tlDet.x + tlDet.width);
    int y2 = std::min(frame.rows, tlDet.y + tlDet.height);
    if (x2 <= x1 || y2 <= y1) return "unknown";

    cv::Rect roi(x1, y1, x2 - x1, y2 - y1);
    cv::Mat roiBgr = frame(roi);

    // 转HSV
    cv::Mat hsv;
    cv::cvtColor(roiBgr, hsv, cv::COLOR_BGR2HSV);

    return analyzeTrafficLightColorROI(hsv, roiBgr, roi.area());
}

// ============================================================================
// ROI内HSV + BGR混合颜色分析
// 核心策略：只分析灯珠发光区域（高亮像素），暗色外壳全部忽略
// ============================================================================
QString TrafficLightDetector::analyzeTrafficLightColorROI(const cv::Mat &hsvROI,
                                                            const cv::Mat &bgrROI,
                                                            int roiArea)
{
    // ===== 第一步：提取灯珠发光区域 =====
    // 红绿灯灯珠是发光体，亮度远高于暗色外壳
    // 只分析高亮像素，彻底排除外壳/背景干扰
    cv::Mat brightMask;
    cv::inRange(hsvROI, cv::Scalar(0, 0, 160), cv::Scalar(180, 255, 255), brightMask);

    int brightPixelCount = cv::countNonZero(brightMask);
    if (brightPixelCount < 3) {
        // 没有足够的高亮像素，可能是远处暗灯，降低阈值重试
        cv::inRange(hsvROI, cv::Scalar(0, 0, 100), cv::Scalar(180, 255, 255), brightMask);
        brightPixelCount = cv::countNonZero(brightMask);
        if (brightPixelCount < 3) {
            return "unknown";
        }
    }

    // ===== 第二步：在发光区域内统计颜色 =====
    // 核心策略：BGR为主，HSV为辅
    // HSV对远处灯光不可靠（黄灯R≈G时Hue偏移到60-90被误判绿色）
    // BGR通道关系更稳定：红=R最大, 绿=G最大, 黄=R≈G>>B
    int redCount = 0, yellowCount = 0, greenCount = 0;
    int bgrRed = 0, bgrYellow = 0, bgrGreen = 0;
    int hsvRed = 0, hsvYellow = 0, hsvGreen = 0;

    for (int y = 0; y < hsvROI.rows; y++) {
        const uchar *hsvRow = hsvROI.ptr<uchar>(y);
        const uchar *bgrRow = bgrROI.ptr<uchar>(y);
        const uchar *maskRow = brightMask.ptr<uchar>(y);

        for (int x = 0; x < hsvROI.cols; x++) {
            if (maskRow[x] == 0) continue;

            int h = hsvRow[x * 3];
            int s = hsvRow[x * 3 + 1];
            int v = hsvRow[x * 3 + 2];
            int b = bgrRow[x * 3];
            int g = bgrRow[x * 3 + 1];
            int r = bgrRow[x * 3 + 2];

            // 亮度权重
            int weight = (v > 220) ? 3 : 1;

            // ---- BGR通道比较（主要判定）----
            // R>G+25 且 R>B+25 → 红
            // G>R+25 且 G>B+20 → 绿
            // |R-G|<40 且 R>B+30 且 G>B+30 → 黄（R≈G且都远大于B）
            bool bgrDecided = false;
            if (r > g + 25 && r > b + 25) {
                redCount += weight; bgrRed += weight;
                bgrDecided = true;
            } else if (g > r + 25 && g > b + 20) {
                greenCount += weight; bgrGreen += weight;
                bgrDecided = true;
            } else if (std::abs(r - g) < 40 && r > b + 30 && g > b + 30) {
                yellowCount += weight; bgrYellow += weight;
                bgrDecided = true;
            }

            // ---- HSV色调（辅助判定，仅BGR不确定时使用）----
            if (!bgrDecided && s >= 40) {
                if (h <= 12 || h >= 155) {
                    redCount += weight; hsvRed += weight;
                } else if (h >= 13 && h <= 38) {
                    yellowCount += weight; hsvYellow += weight;
                } else if (h >= 50 && h <= 80) {  // 绿色缩小到50-80（纯绿色范围）
                    greenCount += weight; hsvGreen += weight;
                }
            }
        }
    }

    // ===== 诊断日志：每15帧输出一次 =====
    static int diagCounter = 0;
    diagCounter++;
    if (diagCounter % 15 == 0 && roiArea > 0) {
        qDebug() << "===== 红绿灯颜色诊断 =====";
        qDebug() << "  发光像素:" << brightPixelCount << "/总:" << roiArea;
        qDebug() << "  红:" << redCount << "(HSV:" << hsvRed << "BGR:" << bgrRed << ")"
                 << "黄:" << yellowCount << "(HSV:" << hsvYellow << "BGR:" << bgrYellow << ")"
                 << "绿:" << greenCount << "(HSV:" << hsvGreen << "BGR:" << bgrGreen << ")";

        // 采样子：每种颜色的前3个像素的HSV和BGR值
        int sampleR = 0, sampleY = 0, sampleG = 0;
        for (int y = 0; y < hsvROI.rows && (sampleR < 2 || sampleY < 2 || sampleG < 2); y++) {
            const uchar *hsvRow = hsvROI.ptr<uchar>(y);
            const uchar *bgrRow = bgrROI.ptr<uchar>(y);
            const uchar *maskRow = brightMask.ptr<uchar>(y);
            for (int x = 0; x < hsvROI.cols && (sampleR < 2 || sampleY < 2 || sampleG < 2); x++) {
                if (maskRow[x] == 0) continue;
                int h = hsvRow[x * 3], s = hsvRow[x * 3 + 1], v = hsvRow[x * 3 + 2];
                int bb = bgrRow[x * 3], g = bgrRow[x * 3 + 1], r = bgrRow[x * 3 + 2];
                if (s >= 30 && (h <= 12 || h >= 155) && sampleR < 2) {
                    qDebug() << "  红色样本: HSV(" << h << s << v << ") BGR(" << bb << g << r << ")";
                    sampleR++;
                } else if (s >= 30 && h >= 18 && h <= 35 && sampleY < 2) {
                    qDebug() << "  黄色样本: HSV(" << h << s << v << ") BGR(" << bb << g << r << ")";
                    sampleY++;
                } else if (((s >= 30 && h >= 45 && h <= 100) || (s < 30 && g > r + 15 && g > bb + 10)) && sampleG < 2) {
                    qDebug() << "  绿色样本: HSV(" << h << s << v << ") BGR(" << bb << g << r << ")"
                             << (s < 30 ? "(BGR判定)" : "(HSV判定)");
                    sampleG++;
                }
            }
        }
    }

    int total = redCount + yellowCount + greenCount;
    if (total < 2) {
        return "unknown";
    }

    // 谁多判谁
    if (redCount >= yellowCount && redCount >= greenCount && redCount > 0) {
        return "red";
    } else if (yellowCount >= redCount && yellowCount >= greenCount && yellowCount > 0) {
        return "yellow";
    } else if (greenCount > 0) {
        return "green";
    } else {
        return "unknown";
    }
}

// ============================================================================
// 纯HSV颜色分析（后备扫描等无BGR场景）
// ============================================================================
QString TrafficLightDetector::analyzeTrafficLightColorHSVOnly(const cv::Mat &hsvROI, int roiArea)
{
    Q_UNUSED(roiArea)
    // 同样只分析发光区域
    cv::Mat brightMask;
    cv::inRange(hsvROI, cv::Scalar(0, 0, 160), cv::Scalar(180, 255, 255), brightMask);

    int brightPixelCount = cv::countNonZero(brightMask);
    if (brightPixelCount < 3) {
        cv::inRange(hsvROI, cv::Scalar(0, 0, 100), cv::Scalar(180, 255, 255), brightMask);
        if (cv::countNonZero(brightMask) < 3) return "unknown";
    }

    int redCount = 0, yellowCount = 0, greenCount = 0;
    for (int y = 0; y < hsvROI.rows; y++) {
        const uchar *hsvRow = hsvROI.ptr<uchar>(y);
        const uchar *maskRow = brightMask.ptr<uchar>(y);
        for (int x = 0; x < hsvROI.cols; x++) {
            if (maskRow[x] == 0) continue;
            int h = hsvRow[x * 3];
            int s = hsvRow[x * 3 + 1];
            int v = hsvRow[x * 3 + 2];
            int weight = (v > 220) ? 3 : 1;
            if (s >= 30) {
                if (h <= 12 || h >= 155) redCount += weight;
                else if (h >= 13 && h <= 38) yellowCount += weight;
                else if (h >= 45 && h <= 100) greenCount += weight;
            }
        }
    }

    if (redCount + yellowCount + greenCount < 2) return "unknown";
    if (redCount >= yellowCount && redCount >= greenCount && redCount > 0) return "red";
    if (yellowCount >= redCount && yellowCount >= greenCount && yellowCount > 0) return "yellow";
    if (greenCount > 0) return "green";
    return "unknown";
}

// ============================================================================
// TrafficRules 颜色判定 - 模型红/绿 + 黄灯启发式
// 偶数类=红(F0,L0,S0,R0)，奇数类=绿(F1,L1,S1,R1)
// 策略：
//   - 高置信(>=TL_HIGH_CONF): 直接采信模型红/绿
//   - 中低置信: 小框 HSV 仲裁（复用 analyzeTrafficLightColor）
//   - HSV 有明确颜色 → 采用
//   - HSV 有黄色 → 判黄
//   - HSV 不明 + 极低置信(<TL_LOW_CONF) → 判黄（远处暗灯可能误判）
//   - HSV 不明 + 非极低置信 → 维持模型判断（避免远处误黄）
// ============================================================================
QString TrafficLightDetector::classifyTrafficLightColor(const Detection &tlDet, const cv::Mat &frame)
{
    float s = tlDet.confidence;
    bool isRed = (tlDet.classId % 2 == 0);  // 偶数类=红，奇数类=绿

    // 高置信：直接采信模型
    if (s >= TL_HIGH_CONF) {
        return isRed ? "red" : "green";
    }

    // 中低置信：小框 HSV 仲裁
    QString hsv = analyzeTrafficLightColor(frame, tlDet);
    if (hsv == "red" || hsv == "green") return hsv;
    if (hsv == "yellow") return "yellow";

    // HSV 不明：极低置信判黄，否则维持模型（避免远处误黄）
    if (s < TL_LOW_CONF) return "yellow";
    return isRed ? "red" : "green";
}

// ============================================================================
// 红绿灯过滤（YOLOv8n 回退路径）- 四级过滤 + 时序投票
//   1. 置信度过滤（TRAFFIC_LIGHT_CONF_THRESHOLD）
//   2. 位置过滤（画面上半部分）
//   3. 长宽比过滤（排除过宽的刹车灯/反光条）
//   4. 车辆包含过滤（排除落在车辆框内的尾灯/刹车灯）
// 颜色状态经时序投票（最近N帧多数）输出，避免单帧误判闪烁
// ============================================================================
TrafficLightResult TrafficLightDetector::filterTrafficLights(const cv::Mat &frame,
                                                               const std::vector<Detection> &allDetections)
{
    TrafficLightResult result;
    result.state = "unknown";

    // 红绿灯永远在画面上方 — 只在上部区域搜索
    int maxYLights = static_cast<int>(frame.rows * TL_UPPER_RATIO);

    QString bestState = "unknown";
    float bestConf = 0;
    int rejectedByAspect = 0;    // 被长宽比过滤的数量（调试用）
    int rejectedByVehicle = 0;   // 被车辆重叠过滤的数量（调试用）

    for (const auto &det : allDetections) {
        if (det.classId != TRAFFIC_LIGHT_CLASS) continue;
        // 置信度过滤使用与 VehicleDetector 一致的低阈值（红绿灯置信度普遍偏低）
        if (det.confidence < 0.10f) continue;

        // 过滤1：位置 — 中心点必须在画面上部
        int centerY = det.y + det.height / 2;
        if (centerY > maxYLights) continue;

        // 过滤2：长宽比 — 宽/高超过阈值视为刹车灯/反光条
        // 横排红绿灯宽/高通常 < 3.0，刹车灯往往 > 3.0
        if (det.height <= 0) continue;
        float aspectRatio = static_cast<float>(det.width) / det.height;
        if (aspectRatio > TL_MAX_ASPECT_RATIO) {
            rejectedByAspect++;
            continue;
        }

        // 过滤3：车辆包含 — 红绿灯框大部分落在车辆框内 → 尾灯/刹车灯
        if (isMostlyInsideVehicle(det, allDetections)) {
            rejectedByVehicle++;
            continue;
        }

        // 通过所有过滤 → HSV颜色分析
        QString color = analyzeTrafficLightColor(frame, det);

        QVariantMap tlMap;
        tlMap["x"] = det.x;
        tlMap["y"] = det.y;
        tlMap["width"] = det.width;
        tlMap["height"] = det.height;
        tlMap["color"] = color;
        tlMap["type"] = "yolo";
        result.trafficLightRects.append(tlMap);

        // 取置信度最高的作为本帧候选状态
        if (det.confidence > bestConf) {
            bestConf = det.confidence;
            bestState = (color == "unknown") ? "detected" : color;
        }
    }

    // 时序投票：用最近N帧的多数投票替换单帧结果，避免状态闪烁
    bool tlDetected = !result.trafficLightRects.isEmpty();
    QString votedState = m_voter.vote(bestState, tlDetected);

    // 投票有明确颜色 → 采用；否则保留本帧原始判断（detected/unknown）
    if (votedState != "unknown") {
        result.state = votedState;
    } else if (tlDetected) {
        result.state = (bestState == "unknown") ? "detected" : bestState;
    } else {
        result.state = "unknown";
    }

    // 调试日志：每60帧输出一次过滤+投票摘要
    static int tlDebugCounter = 0;
    tlDebugCounter++;
    if (tlDebugCounter % 60 == 0) {
        qDebug() << "[红绿灯过滤] 通过:" << result.trafficLightRects.size()
                 << "长宽比过滤:" << rejectedByAspect
                 << "车辆重叠过滤:" << rejectedByVehicle
                 << "本帧:" << bestState << "投票:" << votedState
                 << "缓冲区:" << m_voter.currentSize() << "最终:" << result.state;
    }

    return result;
}

// ============================================================================
// 红绿灯过滤（TrafficRules路径）- 位置 + 长宽比 + 车辆包含 + 颜色判定
// TrafficRules 模型直接输出红/绿8类，颜色由 classifyTrafficLightColor 判定
// 黄灯通过双阈值 + HSV 仲裁实现（训练无黄灯类）
// ============================================================================
TrafficLightResult TrafficLightDetector::filterTrafficLightsTL(const cv::Mat &frame,
                                                                 const std::vector<Detection> &tlDetections,
                                                                 const std::vector<Detection> &vehicleDetections)
{
    TrafficLightResult result;
    result.state = "unknown";

    // 红绿灯永远在画面上方 — 放宽到 0.65（远处红绿灯可能偏下）
    int maxYLights = static_cast<int>(frame.rows * 0.65);

    QString bestState = "unknown";
    float bestConf = 0;
    int rejectedByAspect = 0;
    int rejectedByVehicle = 0;

    for (const auto &det : tlDetections) {
        // 位置过滤：中心点必须在画面上方区域
        int centerY = det.y + det.height / 2;
        if (centerY > maxYLights) continue;

        // 长宽比收紧到 2.5（箭头/圆形灯本身接近1，刹车灯偏宽）
        if (det.height <= 0) continue;
        float aspectRatio = static_cast<float>(det.width) / det.height;
        if (aspectRatio > 2.5f) {
            rejectedByAspect++;
            continue;
        }

        // 车辆包含过滤（排除刹车灯误检）
        if (isMostlyInsideVehicle(det, vehicleDetections)) {
            rejectedByVehicle++;
            continue;
        }

        // 颜色判定：模型红/绿 + 黄灯启发式
        QString color = classifyTrafficLightColor(det, frame);

        QVariantMap tlMap;
        tlMap["x"] = det.x;
        tlMap["y"] = det.y;
        tlMap["width"] = det.width;
        tlMap["height"] = det.height;
        tlMap["color"] = color;
        tlMap["type"] = "trafficrules";
        result.trafficLightRects.append(tlMap);

        // 取置信度最高的作为本帧候选状态
        if (det.confidence > bestConf) {
            bestConf = det.confidence;
            bestState = (color == "unknown") ? "detected" : color;
        }
    }

    // 时序投票：用最近N帧的多数投票替换单帧结果，避免状态闪烁（黄灯抖动平滑）
    bool tlDetected = !result.trafficLightRects.isEmpty();
    QString votedState = m_voter.vote(bestState, tlDetected);

    if (votedState != "unknown") {
        result.state = votedState;
    } else if (tlDetected) {
        result.state = (bestState == "unknown") ? "detected" : bestState;
    } else {
        result.state = "unknown";
    }

    // 调试日志：每60帧输出一次
    static int tlTLDebugCounter = 0;
    tlTLDebugCounter++;
    if (tlTLDebugCounter % 60 == 0) {
        qDebug() << "[TL过滤-TrafficRules] 通过:" << result.trafficLightRects.size()
                 << "长宽比过滤:" << rejectedByAspect
                 << "车辆重叠过滤:" << rejectedByVehicle
                 << "本帧:" << bestState << "投票:" << votedState
                 << "缓冲区:" << m_voter.currentSize() << "最终:" << result.state;
    }

    return result;
}

// ============================================================================
// 主路径检测 - TrafficRules 推理 + filterTrafficLightsTL
// 返回 false 表示推理异常，调用方应走 detectFallback
// ============================================================================
bool TrafficLightDetector::detect(const cv::Mat &frame,
                                    const std::vector<Detection> &vehicleDetections,
                                    TrafficLightResult &result)
{
    result.trafficLightRects.clear();
    result.state = "unknown";

    if (frame.empty()) return true;

    cv::Mat tlBlob;
    double tlScaleX, tlScaleY;
    preprocessTL(frame, tlBlob, tlScaleX, tlScaleY);

    m_tlNet.setInput(tlBlob);
    cv::Mat tlOutput;
    try {
        tlOutput = m_tlNet.forward();
    } catch (const cv::Exception &e) {
        qWarning() << "TrafficRules推理失败:" << e.what();
        return false;
    }

    std::vector<Detection> tlDetections;
    postprocessTL(tlOutput, frame.cols, frame.rows, tlScaleX, tlScaleY, tlDetections);
    tlDetections = nms(tlDetections, TL_NMS_IOU);

    result = filterTrafficLightsTL(frame, tlDetections, vehicleDetections);
    return true;
}

// ============================================================================
// 回退路径 - YOLOv8n class9 + HSV 判色
// ============================================================================
void TrafficLightDetector::detectFallback(const cv::Mat &frame,
                                            const std::vector<Detection> &yoloDetections,
                                            TrafficLightResult &result)
{
    result.trafficLightRects.clear();
    result.state = "unknown";
    if (frame.empty()) return;

    result = filterTrafficLights(frame, yoloDetections);
}
