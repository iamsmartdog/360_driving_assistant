#include "VehicleDetector.h"
#include "services/config/AppConfig.h"

#include <QDebug>
#include <QFileInfo>

#include <cmath>

// ============================================================================
// 构造/析构
// ============================================================================
VehicleDetector::VehicleDetector()
{
    m_loaded = loadModel();
}

VehicleDetector::~VehicleDetector() = default;

// ============================================================================
// 模型加载 - 通过 AppConfig 解析模型路径
// ============================================================================
bool VehicleDetector::loadModel()
{
    QString modelPath = AppConfig::instance().resolveVehicleModelPath();

    if (modelPath.isEmpty()) {
        qWarning() << "未找到车辆模型文件，请在 config.ini [Models] 中配置 Dir 或将"
                    << AppConfig::instance().vehicleModelName()
                    << "放置到 resources/models/ 目录";
        return false;
    }

    // 验证文件大小（yolov8n.onnx约12MB，空文件无效）
    QFileInfo fi(modelPath);
    if (fi.size() < 1024 * 1024) {
        qWarning() << "YOLOv8n模型文件过小（可能损坏）:" << modelPath
                    << "大小:" << fi.size() << "字节";
        return false;
    }

    try {
        m_net = cv::dnn::readNetFromONNX(modelPath.toStdString());
        m_net.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
        m_net.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
        qDebug() << "YOLOv8n模型加载成功:" << modelPath;
        return true;
    } catch (const cv::Exception &e) {
        qWarning() << "YOLOv8n模型加载失败:" << e.what();
        return false;
    }
}

// ============================================================================
// 预处理 - Letterbox Resize + BGR→RGB + 归一化
// ============================================================================
void VehicleDetector::preprocess(const cv::Mat &frame, cv::Mat &blob,
                                  double &scaleFactor, int &padX, int &padY)
{
    int fw = frame.cols;
    int fh = frame.rows;

    // 计算letterbox缩放因子（保持宽高比，最长边缩放到640）
    scaleFactor = std::min(static_cast<double>(INPUT_WIDTH) / fw,
                           static_cast<double>(INPUT_HEIGHT) / fh);

    int newW = static_cast<int>(fw * scaleFactor);
    int newH = static_cast<int>(fh * scaleFactor);

    // 计算padding
    padX = (INPUT_WIDTH - newW) / 2;
    padY = (INPUT_HEIGHT - newH) / 2;

    // Resize
    cv::Mat resized;
    cv::resize(frame, resized, cv::Size(newW, newH));

    // 创建带padding的640x640图像（灰色填充114,114,114）
    cv::Mat padded(INPUT_HEIGHT, INPUT_WIDTH, frame.type(), cv::Scalar(114, 114, 114));
    resized.copyTo(padded(cv::Rect(padX, padY, newW, newH)));

    // BGR→RGB + 归一化到[0,1] + NCHW格式
    blob = cv::dnn::blobFromImage(padded, 1.0 / 255.0, cv::Size(INPUT_WIDTH, INPUT_HEIGHT),
                                   cv::Scalar(0, 0, 0), true, false);
}

// ============================================================================
// 后处理 - 解析YOLOv8输出 [1, 84, 8400]
// YOLOv8输出格式：每列 = [cx, cy, w, h, class0_score, ..., class79_score]
// 使用reshape将[1,84,8400]转为[84,8400]方便用2D索引访问
// ============================================================================
void VehicleDetector::postprocess(const cv::Mat &output, int frameWidth, int frameHeight,
                                   double scaleFactor, int padX, int padY,
                                   std::vector<Detection> &detections)
{
    // YOLOv8输出维度：[1, 84, 8400]
    // 84 = 4(坐标) + 80(类别分数)
    // 8400 = 预测框数量
    //
    // 关键修复：将输出reshape为[84, 8400]的2D矩阵，
    // 这样可以用 output.at<float>(row, col) 直接访问，
    // 避免cv::Vec<int,4>访问3维张量时的维度不匹配问题

    cv::Mat output2d;
    if (output.dims == 3) {
        // [1, 84, 8400] → [84, 8400]
        int numChannels = output.size[1];  // 84
        output2d = output.reshape(0, numChannels);  // reshape为 [84, 8400]
    } else {
        output2d = output.clone();
    }

    int numPredictions = output2d.cols;  // 8400
    int numClasses = output2d.rows - 4;  // 80

    float minConf = std::min(VEHICLE_CONF_THRESHOLD, TRAFFIC_LIGHT_CONF_THRESHOLD);

    for (int i = 0; i < numPredictions; ++i) {
        // 读取4个坐标值（前4行）
        float cx = output2d.at<float>(0, i);
        float cy = output2d.at<float>(1, i);
        float w  = output2d.at<float>(2, i);
        float h  = output2d.at<float>(3, i);

        // 找到最高置信度的类别（第4行到第83行）
        float maxScore = 0;
        int maxClassId = -1;
        for (int c = 0; c < numClasses; ++c) {
            float score = output2d.at<float>(4 + c, i);
            if (score > maxScore) {
                maxScore = score;
                maxClassId = c;
            }
        }

        // 只保留车辆和红绿灯类别
        if (maxScore < minConf) continue;
        if (!VEHICLE_CLASSES.count(maxClassId) && maxClassId != TRAFFIC_LIGHT_CLASS) continue;

        // 坐标从640x640映射回原图
        int left = static_cast<int>((cx - w / 2.0 - padX) / scaleFactor);
        int top  = static_cast<int>((cy - h / 2.0 - padY) / scaleFactor);
        int bw   = static_cast<int>(w / scaleFactor);
        int bh   = static_cast<int>(h / scaleFactor);

        // clamp到图像范围内
        left = std::max(0, std::min(left, frameWidth - 1));
        top  = std::max(0, std::min(top, frameHeight - 1));
        bw   = std::min(bw, frameWidth - left);
        bh   = std::min(bh, frameHeight - top);

        // 过滤过小的检测框
        if (bw < 10 || bh < 10) continue;

        Detection det;
        det.x = left;
        det.y = top;
        det.width = bw;
        det.height = bh;
        det.confidence = maxScore;
        det.classId = maxClassId;
        det.className = (maxClassId >= 0 && maxClassId < 80) ? COCO_CLASSES[maxClassId] : "unknown";
        detections.push_back(det);
    }
}

// ============================================================================
// 核心检测接口 - 预处理 → 推理 → 后处理 → NMS
// 返回车辆 + 红绿灯(class9) 的全量检测（供 TL 回退路径复用）
// ============================================================================
std::vector<Detection> VehicleDetector::detect(const cv::Mat &frame)
{
    if (!m_loaded || frame.empty()) return {};

    cv::Mat blob;
    double scaleFactor;
    int padX, padY;
    preprocess(frame, blob, scaleFactor, padX, padY);

    m_net.setInput(blob);
    cv::Mat output;
    try {
        output = m_net.forward();
    } catch (const cv::Exception &e) {
        qWarning() << "YOLOv8推理失败:" << e.what();
        return {};
    }

    std::vector<Detection> detections;
    postprocess(output, frame.cols, frame.rows, scaleFactor, padX, padY, detections);
    return nms(detections, NMS_IOU_THRESHOLD);
}

// ============================================================================
// 车辆过滤 - 从所有检测结果中筛选车辆，转换为QVariantList
// ============================================================================
QVariantList VehicleDetector::filterVehicles(const std::vector<Detection> &allDetections)
{
    QVariantList vehicles;
    for (const auto &det : allDetections) {
        if (!VEHICLE_CLASSES.count(det.classId)) continue;
        if (det.confidence < VEHICLE_CONF_THRESHOLD) continue;

        QVariantMap vehicle;
        vehicle["x"] = det.x;
        vehicle["y"] = det.y;
        vehicle["width"] = det.width;
        vehicle["height"] = det.height;
        vehicle["confidence"] = det.confidence;
        vehicle["class"] = det.className;
        vehicles.append(vehicle);
    }
    return vehicles;
}
