#ifndef TRAFFICLIGHTDETECTOR_H
#define TRAFFICLIGHTDETECTOR_H

#include "DetectionTypes.h"
#include "TrafficLightColorVoter.h"

#include <opencv2/dnn.hpp>
#include <opencv2/opencv.hpp>

#include <QString>
#include <vector>

// ============================================================================
// TrafficLightDetector - 红绿灯检测器
//
// 主路径：TrafficRules YOLO11n（8类：F0,F1,L0,L1,S0,S1,R0,R1，0红1绿）
// 回退路径：YOLOv8n class9 + HSV 判色（TrafficRules 模型未加载或推理失败时）
//
// 单一职责：
//   - 加载 TrafficRules 红绿灯专用模型
//   - TrafficRules 推理 + 后处理 + 颜色判定（含黄灯启发式）
//   - YOLOv8n class9 + HSV 判色回退路径
//   - HSV 颜色分析（BGR+HSV 混合）
//   - 通过 TrafficLightColorVoter 做时序投票
//
// 时序投票状态由外部 TrafficLightColorVoter 维护，本类不持有跨帧状态。
// ============================================================================
class TrafficLightDetector
{
public:
    /// @param voter 外部持有的时序投票器（多个路径共享同一投票器）
    explicit TrafficLightDetector(TrafficLightColorVoter &voter);

    /// 加载 TrafficRules 红绿灯专用模型（路径由 AppConfig 解析）
    bool loadModel();
    bool isLoaded() const { return m_tlLoaded; }

    /**
     * @brief TrafficRules 主路径检测
     * @param frame             输入帧
     * @param vehicleDetections 车辆检测（用于刹车灯包含过滤，可含 class9）
     * @param result            输出红绿灯结果（经时序投票）
     * @return true 成功；false 表示推理异常（调用方应走 detectFallback）
     */
    bool detect(const cv::Mat &frame,
                const std::vector<Detection> &vehicleDetections,
                TrafficLightResult &result);

    /**
     * @brief YOLOv8n + HSV 回退路径
     * @param frame          输入帧
     * @param yoloDetections YOLOv8n 全量检测（含 class 9 红绿灯）
     * @param result         输出红绿灯结果（经时序投票）
     */
    void detectFallback(const cv::Mat &frame,
                        const std::vector<Detection> &yoloDetections,
                        TrafficLightResult &result);

private:
    // ===== TrafficRules 推理 =====
    // 直接 resize 到 640x480（无 letterbox，匹配训练分布）
    // X/Y 缩放分离，因为 640x480 非正方形，坐标回映需分别使用
    void preprocessTL(const cv::Mat &frame, cv::Mat &blob,
                      double &scaleX, double &scaleY);
    // 解析 YOLO11n 输出 [1, 12, 6300]，8类全部保留
    void postprocessTL(const cv::Mat &output, int frameWidth, int frameHeight,
                       double scaleX, double scaleY,
                       std::vector<Detection> &detections);

    // ===== 红绿灯颜色分析 =====
    // 在检测框内 HSV 分析颜色（HSV+BGR 混合，用于黄灯仲裁和回退路径）
    QString analyzeTrafficLightColor(const cv::Mat &frame, const Detection &tlDet);
    // HSV + BGR 混合颜色分析（同时接受 HSV 和 BGR 的 ROI）
    QString analyzeTrafficLightColorROI(const cv::Mat &hsvROI, const cv::Mat &bgrROI, int roiArea);
    // 纯 HSV 颜色分析（仅 HSV，用于后备扫描等无 BGR 场景）
    QString analyzeTrafficLightColorHSVOnly(const cv::Mat &hsvROI, int roiArea);
    // TrafficRules 颜色判定：模型红/绿 + 黄灯启发式（双阈值 + 小框 HSV 仲裁）
    QString classifyTrafficLightColor(const Detection &tlDet, const cv::Mat &frame);

    // ===== 过滤 =====
    // 旧路径：yolov8n class9 + HSV 判色
    TrafficLightResult filterTrafficLights(const cv::Mat &frame,
                                           const std::vector<Detection> &allDetections);
    // 新路径：TrafficRules 8类检测 + classifyTrafficLightColor 颜色判定
    TrafficLightResult filterTrafficLightsTL(const cv::Mat &frame,
                                             const std::vector<Detection> &tlDetections,
                                             const std::vector<Detection> &vehicleDetections);

    cv::dnn::Net m_tlNet;
    bool m_tlLoaded = false;
    TrafficLightColorVoter &m_voter;

    // ===== TrafficRules YOLO11n 红绿灯专用模型参数 =====
    // 输入尺寸 640x480（高<宽），预处理直接 resize 不做 letterbox
    static constexpr int TL_INPUT_WIDTH = 640;
    static constexpr int TL_INPUT_HEIGHT = 480;
    // 8类：F0,F1,L0,L1,S0,S1,R0,R1（F=圆形,L=左箭,S=直行,R=右箭；0=红,1=绿）
    static constexpr int TL_NUM_CLASSES = 8;
    static const char* TL_CLASSES[8];
    // 置信度/NMS 阈值（与训练 config.toml 一致）
    static constexpr float TL_CONF_THRESHOLD = 0.25f;
    static constexpr float TL_NMS_IOU = 0.45f;
    // 黄灯启发式阈值：高置信直接采信模型，极低置信且 HSV 不明则判黄
    static constexpr float TL_HIGH_CONF = 0.45f;
    static constexpr float TL_LOW_CONF = 0.30f;
    // 小框过滤放宽（原 yolov8n 路径 bw<10 过严，远处小灯漏检）
    static constexpr int TL_MIN_BOX = 6;

    // ===== 过滤参数 =====
    // 红绿灯最小面积阈值（像素），低于此值不进行颜色分析
    static constexpr int TL_MIN_AREA = 50;
    // HSV 后备扫描：红绿灯只在画面上方搜索
    static constexpr double TL_UPPER_RATIO = 0.55;
    // 红绿灯长宽比阈值（宽/高），超过则视为刹车灯/反光条
    static constexpr float TL_MAX_ASPECT_RATIO = 3.0f;
};

#endif // TRAFFICLIGHTDETECTOR_H
