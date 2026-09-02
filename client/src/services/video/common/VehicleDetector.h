#ifndef VEHICLEDETECTOR_H
#define VEHICLEDETECTOR_H

#include "DetectionTypes.h"

#include <opencv2/dnn.hpp>
#include <opencv2/opencv.hpp>

#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <vector>

// ============================================================================
// VehicleDetector - 车辆检测器（YOLOv8n, COCO 80类）
//
// 单一职责：加载车辆模型 + 预处理/推理/后处理。
//
// 一次 forward 同时输出车辆(car/motorcycle/bus/truck)与红绿灯(class 9)原始检测，
// 供 TrafficLightDetector 的 yolov8n+HSV 回退路径复用；调用 filterVehicles()
// 可仅取车辆。
//
// 不持有任何跨帧状态，可被多线程串行复用。
// ============================================================================
class VehicleDetector
{
public:
    VehicleDetector();
    ~VehicleDetector();

    /// 加载 YOLOv8n 模型（路径由 AppConfig 解析），成功返回 true
    bool loadModel();
    bool isLoaded() const { return m_loaded; }

    /**
     * @brief 执行 YOLOv8n 推理，返回车辆+红绿灯(class9)的原始检测（已 NMS）
     * @note  返回值包含 class 9(traffic_light)，供 TL 回退路径使用；
     *        调用 filterVehicles() 可仅取车辆。
     * @param frame 输入帧（BGR），空帧或模型未加载时返回空
     */
    std::vector<Detection> detect(const cv::Mat &frame);

    /// 从检测结果中筛选车辆并转为 QML QVariantList
    static QVariantList filterVehicles(const std::vector<Detection> &allDetections);

private:
    // Letterbox Resize + BGR→RGB + 归一化
    void preprocess(const cv::Mat &frame, cv::Mat &blob,
                    double &scaleFactor, int &padX, int &padY);
    // 解析 YOLOv8 输出 [1, 84, 8400]，仅保留车辆 + 红绿灯类别
    void postprocess(const cv::Mat &output, int frameWidth, int frameHeight,
                     double scaleFactor, int padX, int padY,
                     std::vector<Detection> &detections);

    cv::dnn::Net m_net;
    bool m_loaded = false;

    // 模型输入尺寸
    static constexpr int INPUT_WIDTH = 640;
    static constexpr int INPUT_HEIGHT = 640;

    // 置信度阈值（红绿灯阈值低，因 YOLO 对红绿灯置信度普遍偏低）
    static constexpr float VEHICLE_CONF_THRESHOLD = 0.35f;
    static constexpr float TRAFFIC_LIGHT_CONF_THRESHOLD = 0.10f;
    // NMS IoU 阈值
    static constexpr float NMS_IOU_THRESHOLD = 0.45f;
};

#endif // VEHICLEDETECTOR_H
