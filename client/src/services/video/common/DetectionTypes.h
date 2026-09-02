#ifndef DETECTIONTYPES_H
#define DETECTIONTYPES_H

#include <QString>
#include <QVariantList>
#include <QVariantMap>

#include <algorithm>
#include <cstddef>
#include <numeric>
#include <set>
#include <vector>

// ============================================================================
// 共享数据结构和常量 — 供 VehicleDetector / TrafficLightDetector / DetectionEngine 使用
// ============================================================================

// YOLOv8n 检测结果结构体
struct Detection {
    int x;              // 左上角x
    int y;              // 左上角y
    int width;          // 宽度
    int height;         // 高度
    float confidence;   // 置信度
    int classId;        // COCO类别ID
    QString className;  // 类别名
};

// 红绿灯颜色分析结果
struct TrafficLightResult {
    QVariantList trafficLightRects;  // 红绿灯位置列表（QML用）
    QString state;                   // "red" / "green" / "yellow" / "unknown"
};

// COCO 80类中车辆相关类别ID：car=2, motorcycle=3, bus=5, truck=7
inline const std::set<int> VEHICLE_CLASSES = {2, 3, 5, 7};
// traffic_light=9
inline constexpr int TRAFFIC_LIGHT_CLASS = 9;

// COCO 80类名称
inline const char* COCO_CLASSES[80] = {
    "person", "bicycle", "car", "motorcycle", "airplane",
    "bus", "train", "truck", "boat", "traffic light",
    "fire hydrant", "stop sign", "parking meter", "bench", "bird",
    "cat", "dog", "horse", "sheep", "cow",
    "elephant", "bear", "zebra", "giraffe", "backpack",
    "umbrella", "handbag", "tie", "suitcase", "frisbee",
    "skis", "snowboard", "sports ball", "kite", "baseball bat",
    "baseball glove", "skateboard", "surfboard", "tennis racket", "bottle",
    "wine glass", "cup", "fork", "knife", "spoon",
    "bowl", "banana", "apple", "sandwich", "orange",
    "broccoli", "carrot", "hot dog", "pizza", "donut",
    "cake", "chair", "couch", "potted plant", "bed",
    "dining table", "toilet", "tv", "laptop", "mouse",
    "remote", "keyboard", "cell phone", "microwave", "oven",
    "toaster", "sink", "refrigerator", "book", "clock",
    "vase", "scissors", "teddy bear", "hair drier", "toothbrush"
};

// NMS（非极大值抑制）- 去除重叠检测框
inline std::vector<Detection> nms(const std::vector<Detection> &detections,
                                   float iouThreshold = 0.45f)
{
    if (detections.empty()) return {};

    std::vector<size_t> indices(detections.size());
    std::iota(indices.begin(), indices.end(), 0);
    std::sort(indices.begin(), indices.end(),
              [&detections](size_t a, size_t b) {
                  return detections[a].confidence > detections[b].confidence;
              });

    std::vector<bool> suppressed(detections.size(), false);
    std::vector<Detection> result;

    for (size_t i = 0; i < indices.size(); ++i) {
        if (suppressed[indices[i]]) continue;
        const auto &a = detections[indices[i]];
        result.push_back(a);
        for (size_t j = i + 1; j < indices.size(); ++j) {
            if (suppressed[indices[j]]) continue;
            const auto &b = detections[indices[j]];
            if (a.classId != b.classId) continue;
            int x1 = std::max(a.x, b.x);
            int y1 = std::max(a.y, b.y);
            int x2 = std::min(a.x + a.width, b.x + b.width);
            int y2 = std::min(a.y + a.height, b.y + b.height);
            int interArea = std::max(0, x2 - x1) * std::max(0, y2 - y1);
            int unionArea = a.width * a.height + b.width * b.height - interArea;
            float iou = unionArea > 0 ? static_cast<float>(interArea) / unionArea : 0.f;
            if (iou > iouThreshold) suppressed[indices[j]] = true;
        }
    }
    return result;
}

// 计算两个检测框的IoU
inline float computeIoU(const Detection &a, const Detection &b)
{
    int x1 = std::max(a.x, b.x);
    int y1 = std::max(a.y, b.y);
    int x2 = std::min(a.x + a.width, b.x + b.width);
    int y2 = std::min(a.y + a.height, b.y + b.height);
    int interArea = std::max(0, x2 - x1) * std::max(0, y2 - y1);
    int unionArea = a.width * a.height + b.width * b.height - interArea;
    return unionArea > 0 ? static_cast<float>(interArea) / unionArea : 0.f;
}

// 红绿灯框是否被某个车辆框大部分包含（用于排除刹车灯/尾灯误检）
inline bool isMostlyInsideVehicle(const Detection &tl,
                                   const std::vector<Detection> &allDetections,
                                   float containmentThreshold = 0.6f,
                                   float vehicleConfThreshold = 0.35f)
{
    int tlArea = tl.width * tl.height;
    if (tlArea <= 0) return false;
    for (const auto &det : allDetections) {
        if (!VEHICLE_CLASSES.count(det.classId)) continue;
        if (det.confidence < vehicleConfThreshold) continue;
        int x1 = std::max(tl.x, det.x);
        int y1 = std::max(tl.y, det.y);
        int x2 = std::min(tl.x + tl.width, det.x + det.width);
        int y2 = std::min(tl.y + tl.height, det.y + det.height);
        int interArea = std::max(0, x2 - x1) * std::max(0, y2 - y1);
        float containment = static_cast<float>(interArea) / tlArea;
        if (containment > containmentThreshold) return true;
    }
    return false;
}

#endif // DETECTIONTYPES_H
