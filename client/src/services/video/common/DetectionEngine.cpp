#include "DetectionEngine.h"

#include <QDebug>
#include <QVariantMap>

// ============================================================================
// 构造/析构
//
// 注意成员声明顺序：m_vehicleDetector → m_voter → m_tlDetector
// m_tlDetector 构造时引用 m_voter，故 m_voter 必须先于 m_tlDetector 构造。
// ============================================================================
DetectionEngine::DetectionEngine(QObject *parent)
    : QObject(parent)
    , m_vehicleDetector()
    , m_voter()
    , m_tlDetector(m_voter)
{
}

DetectionEngine::~DetectionEngine() = default;

bool DetectionEngine::isLoaded() const
{
    return m_vehicleDetector.isLoaded();
}

bool DetectionEngine::isTrafficRulesLoaded() const
{
    return m_tlDetector.isLoaded();
}

// ============================================================================
// 核心接口：双模型推理同时获取车辆+红绿灯
// 1. yolov8n 推理 → filterVehicles（车辆检测）
// 2. TrafficRules 推理 → filterTrafficLightsTL（红绿灯检测）
//    若 TrafficRules 未加载或推理失败，回退 yolov8n+HSV 路径
// ============================================================================
void DetectionEngine::detectAll(const cv::Mat &frame,
                                 QVariantList &vehicleResults,
                                 TrafficLightResult &trafficLightResult)
{
    vehicleResults.clear();
    trafficLightResult.trafficLightRects.clear();
    trafficLightResult.state = "unknown";

    if (!m_vehicleDetector.isLoaded() || frame.empty()) return;

    // ===== Step 1: YOLOv8n 车辆推理（同时输出 class9 红绿灯原始检测，供回退路径复用）=====
    std::vector<Detection> allDetections = m_vehicleDetector.detect(frame);
    vehicleResults = VehicleDetector::filterVehicles(allDetections);

    // ===== Step 2: 红绿灯检测 =====
    if (m_tlDetector.isLoaded()) {
        if (!m_tlDetector.detect(frame, allDetections, trafficLightResult)) {
            // TrafficRules 推理失败 → 回退 yolov8n+HSV（复用已推理结果）
            m_tlDetector.detectFallback(frame, allDetections, trafficLightResult);
        }
    } else {
        // 模型未加载 → yolov8n+HSV 回退路径
        m_tlDetector.detectFallback(frame, allDetections, trafficLightResult);
    }

    // 调试日志：每60帧输出一次检测结果摘要
    static int debugCounter = 0;
    debugCounter++;
    if (debugCounter % 60 == 0) {
        qDebug() << "[检测调试] 车辆:" << vehicleResults.size()
                 << "红绿灯:" << trafficLightResult.trafficLightRects.size()
                 << "模型:" << (m_tlDetector.isLoaded() ? "TrafficRules" : "YOLOv8n+HSV回退")
                 << "状态:" << trafficLightResult.state;
    }
}

// ============================================================================
// 仅车辆检测 - 只跑 YOLOv8n，不跑红绿灯模型
// ============================================================================
void DetectionEngine::detectVehicles(const cv::Mat &frame, QVariantList &vehicleResults)
{
    vehicleResults.clear();
    if (!m_vehicleDetector.isLoaded() || frame.empty()) return;

    std::vector<Detection> allDetections = m_vehicleDetector.detect(frame);
    vehicleResults = VehicleDetector::filterVehicles(allDetections);
}

// ============================================================================
// 仅红绿灯检测 - 优先 TrafficRules，未加载或推理失败时回退 YOLOv8n+HSV
// ============================================================================
void DetectionEngine::detectTrafficLights(const cv::Mat &frame, TrafficLightResult &result)
{
    result.trafficLightRects.clear();
    result.state = "unknown";

    if (frame.empty()) return;

    if (m_tlDetector.isLoaded()) {
        // 需要 vehicleDetections 用于刹车灯包含过滤
        std::vector<Detection> vehicleDetections;
        if (m_vehicleDetector.isLoaded()) {
            vehicleDetections = m_vehicleDetector.detect(frame);
        }
        if (!m_tlDetector.detect(frame, vehicleDetections, result)) {
            // TL 推理失败 → 回退 yolov8n+HSV（复用已推理的 vehicleDetections）
            if (m_vehicleDetector.isLoaded()) {
                m_tlDetector.detectFallback(frame, vehicleDetections, result);
            }
        }
    } else if (m_vehicleDetector.isLoaded()) {
        // 模型未加载 → yolov8n+HSV 回退路径
        std::vector<Detection> allDetections = m_vehicleDetector.detect(frame);
        m_tlDetector.detectFallback(frame, allDetections, result);
    }
}

// ============================================================================
// 统一绘制检测框 + 标签 + 水印
// ============================================================================
void DetectionEngine::drawDetections(cv::Mat &frame,
                                      const QVariantList &vehicles,
                                      const QVariantList &trafficLights,
                                      const QString &trafficLightState,
                                      const QString &watermark)
{
    Q_UNUSED(trafficLightState);

    // 绘制车辆检测框（绿色）
    for (const QVariant &item : vehicles) {
        QVariantMap vehicle = item.toMap();
        int x = vehicle["x"].toInt();
        int y = vehicle["y"].toInt();
        int w = vehicle["width"].toInt();
        int h = vehicle["height"].toInt();
        float conf = vehicle["confidence"].toFloat();
        QString className = vehicle["class"].toString();

        cv::rectangle(frame, cv::Point(x, y), cv::Point(x + w, y + h),
                      cv::Scalar(0, 255, 0), 2);

        QString label = QString("%1 %2%").arg(className).arg(static_cast<int>(conf * 100));

        int baseline = 0;
        cv::Size textSize = cv::getTextSize(label.toStdString(), cv::FONT_HERSHEY_SIMPLEX,
                                             0.5, 1, &baseline);
        cv::rectangle(frame, cv::Point(x, y - textSize.height - 4),
                      cv::Point(x + textSize.width, y),
                      cv::Scalar(0, 255, 0), cv::FILLED);
        cv::putText(frame, label.toStdString(), cv::Point(x, y - 2),
                    cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 0, 0), 1);
    }

    // 绘制红绿灯检测框
    for (const QVariant &item : trafficLights) {
        QVariantMap tl = item.toMap();
        int x = tl["x"].toInt();
        int y = tl["y"].toInt();
        int w = tl["width"].toInt();
        int h = tl["height"].toInt();
        QString color = tl["color"].toString();
        QString type = tl["type"].toString();

        cv::Scalar boxColor;
        QString labelText;
        if (color == "red") {
            boxColor = cv::Scalar(0, 0, 255);
            labelText = "RED - Stop";
        } else if (color == "green") {
            boxColor = cv::Scalar(0, 200, 0);  // 使用较深绿色，BGR中更清晰
            labelText = "GREEN - Go";
        } else if (color == "yellow") {
            boxColor = cv::Scalar(0, 255, 255);
            labelText = "YELLOW - Caution";
        } else {
            // 检测到红绿灯但颜色未确认：用白色框+问号
            boxColor = cv::Scalar(255, 255, 255);
            labelText = "TL ?";
        }

        // HSV后备检测用虚线框区分（已删除hsvFallbackScan，保留兼容旧回退路径）
        int thickness = (type == "hsv_fallback") ? 1 : 2;
        cv::rectangle(frame, cv::Point(x, y), cv::Point(x + w, y + h),
                      boxColor, thickness);
        cv::putText(frame, labelText.toStdString(), cv::Point(x, y - 5),
                    cv::FONT_HERSHEY_SIMPLEX, 0.5, boxColor, 1);
    }

    // 绘制水印（左下角）
    if (!watermark.isEmpty()) {
        cv::putText(frame, watermark.toStdString(),
                    cv::Point(10, frame.rows - 10),
                    cv::FONT_HERSHEY_SIMPLEX, 0.5,
                    cv::Scalar(255, 255, 255), 1);
    }
}
