# YOLOv8n 检测引擎迁移计划

## Context

当前车辆和红绿灯检测使用 MobileNet-SSD + HSV 颜色扫描方案，效果很差：
- 车辆漏检严重（只检测到2/10辆车）
- 红绿灯误检路桩、红色招牌等（HSV全图扫描无法区分）
- 三个Service中约1600行重复的检测代码

**目标**：迁移到 YOLOv8n-COCO（预训练，不需要自己训练），显著提升检测精度。

## 方案

- **车辆检测**：YOLOv8n 直接检测 car(2), motorcycle(3), bus(5), truck(7)
- **红绿灯检测**：YOLOv8n 定位 traffic_light(9) → 框内 HSV 分析颜色（红/绿/黄）
- **共享 DetectionEngine 类**：消除3个Service的代码重复
- **单次推理**：一次 forward pass 同时获取车辆+红绿灯，比原来分开跑更高效

## 新增文件

### 1. `client/src/services/video/common/DetectionEngine.h`
共享检测引擎头文件：
- `Detection` 结构体（x, y, width, height, confidence, classId, className）
- `TrafficLightResult` 结构体（trafficLightRects, state）
- `DetectionEngine` 类：
  - `detectAll(frame, vehicles, trafficLights)` — 单次推理同时获取两类结果
  - `detectVehicles(frame)` — 仅车辆
  - `detectTrafficLights(frame)` — 仅红绿灯
  - `drawDetections(frame, vehicles, trafficLights, state, watermark)` — 绘制
  - `isLoaded()` — 模型是否加载

### 2. `client/src/services/video/common/DetectionEngine.cpp`
核心实现：
- **loadModel()**：加载 yolov8n.onnx，多路径回退
- **preprocess()**：letterbox resize 640x640 + BGR→RGB + 归一化
- **postprocess()**：解析 YOLOv8 输出 [1,84,8400]，置信度过滤，坐标映射回原图
- **nms()**：非极大值抑制去重
- **analyzeTrafficLightColor()**：在 YOLO 检测框内 HSV 分析颜色（~40行替代原来300行）
- **drawDetections()**：统一的检测框绘制+标签

### 3. `client/resources/models/yolov8n.onnx`
从 Ultralytics 下载 YOLOv8n ONNX 模型（~6MB）

## 修改文件

### 4. `client/src/services/video/driving/VideoRecorderService.h/.cpp`
- **删除**：TrafficLightBulb结构体、m_dnnNet/m_dnnLoaded、detectVehicles()、detectTrafficLight()、findTrafficLightBulbs()、findDigitalTrafficLights()、hasDarkHousing()、verifyTrafficLightStructure()、drawDetectionBoxes()、所有MobileNet-SSD常量
- **添加**：`DetectionEngine *m_detectionEngine`
- **修改** processFrame()：调用 m_detectionEngine->detectAll() + drawDetections()

### 5. `client/src/services/video/playback/VideoPlaybackService.h/.cpp`
同上模式，修改 detectAndDrawFrame()

### 6. `client/src/services/video/playback/PlaybackDetectService.h/.cpp`
同上模式

### 7. `client/CMakeLists.txt`
添加 DetectionEngine.h/cpp 到源文件列表

## 删除文件（迁移完成后）
- `client/resources/models/MobileNetSSD_deploy.caffemodel`（23MB）
- `client/resources/models/MobileNetSSD_deploy.prototxt`

## 技术细节

### YOLOv8n ONNX + OpenCV DNN
- 输入：[1, 3, 640, 640] NCHW
- 输出：[1, 84, 8400]（4坐标 + 80类分数 × 8400预测）
- 预处理：letterbox + RGB + /255.0
- 后处理：置信度阈值(车辆0.35/红绿灯0.3) → NMS(IoU 0.45)
- OpenCV 4.12 完全支持 YOLOv8 ONNX

### 红绿灯颜色分析（在YOLO框内）
```
1. YOLOv8 检测到 traffic_light 边界框
2. 裁剪框内 ROI → 转 HSV
3. 分别统计 红/绿/黄 像素数
4. 像素最多的颜色即为当前灯色
5. 最小面积阈值：框 < 400px 时返回 unknown
```

### 帧跳跃策略
- YOLOv8 单次推理同时获取车辆+红绿灯
- 每2帧执行一次检测，中间帧复用上次结果
- 比原来分开跑更高效

## 实施步骤

1. 下载 yolov8n.onnx 模型
2. 创建 DetectionEngine.h/.cpp
3. 更新 CMakeLists.txt
4. 迁移 VideoRecorderService（行车模式，最核心）
5. 迁移 VideoPlaybackService（播放模式）
6. 迁移 PlaybackDetectService（回放检测）
7. 编译测试
8. 删除旧 MobileNet-SSD 模型文件

## 验证方式
- 编译通过
- 行车模式：车辆检测框选准确（多目标不漏检）
- 行车模式：红绿灯只框选真实红绿灯，颜色判断正确
- 视频播放：回放检测功能正常
- QML 界面：detectedVehicles/trafficLightState 属性正常显示
