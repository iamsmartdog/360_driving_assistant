#ifndef DETECTIONENGINE_H
#define DETECTIONENGINE_H

#include <QObject>
#include <QString>
#include <QVariantList>

#include <opencv2/opencv.hpp>

#include "DetectionTypes.h"
#include "VehicleDetector.h"
#include "TrafficLightDetector.h"
#include "TrafficLightColorVoter.h"

// ============================================================================
// DetectionEngine - 检测引擎门面（Facade）
//
// 组合 VehicleDetector + TrafficLightDetector + TrafficLightColorVoter，
// 对外暴露与历史版本兼容的检测接口，供 VideoRecorderService、
// VideoPlaybackService、PlaybackDetectService 共用。
//
// 职责：
//   - 持有三个子组件并完成装配
//   - detectAll/detectVehicles/detectTrafficLights 编排（含 TL 推理失败回退）
//   - drawDetections 统一绘制
//
// 时序投票状态由 m_voter 持有；如需独立投票上下文（如录播分离），
// 可构造多个 DetectionEngine 实例。各子组件本身无跨帧状态。
// ============================================================================
class DetectionEngine : public QObject
{
    Q_OBJECT

public:
    explicit DetectionEngine(QObject *parent = nullptr);
    ~DetectionEngine();

    // 车辆模型是否加载成功
    bool isLoaded() const;

    // TrafficRules 红绿灯专用模型是否加载成功
    bool isTrafficRulesLoaded() const;

    // ===== 核心检测接口 =====

    // 单次推理同时获取车辆+红绿灯（推荐，一次 yolov8n forward + 一次 TrafficRules forward）
    void detectAll(const cv::Mat &frame,
                   QVariantList &vehicleResults,
                   TrafficLightResult &trafficLightResult);

    // 仅车辆检测
    void detectVehicles(const cv::Mat &frame, QVariantList &vehicleResults);

    // 仅红绿灯检测（TrafficRules 优先，未加载或推理失败时回退 YOLOv8n+HSV）
    void detectTrafficLights(const cv::Mat &frame, TrafficLightResult &result);

    // ===== 绘制接口 =====

    // 统一绘制检测框 + 标签 + 水印
    void drawDetections(cv::Mat &frame,
                        const QVariantList &vehicles,
                        const QVariantList &trafficLights,
                        const QString &trafficLightState,
                        const QString &watermark = "360 Driving Assistant");

private:
    VehicleDetector m_vehicleDetector;
    TrafficLightColorVoter m_voter;
    TrafficLightDetector m_tlDetector;
};

#endif // DETECTIONENGINE_H
