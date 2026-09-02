# 集成 TrafficRules YOLO11n 红绿灯专用模型

## Context

当前红绿灯检测使用 COCO 预训练的 `yolov8n.onnx`（只标 "traffic light" 类别，无颜色标注），导致4个问题：

1. **红旗被识别为红绿灯颜色** — YOLO 把红旗误分类为 class 9，HSV 再判红
2. **三个上下连着的交通指示牌被识别为红绿灯** — COCO 模型分类层面的固有缺陷
3. **远处红绿灯识别不到** — 640×640 输入小目标 + `bw<10‖bh<10` 过滤 + 远处置信度低于0.10
4. **检到红绿灯但识别不到颜色** — HSV `brightPixelCount>=3` + 颜色阈值偏严

根因：COCO 模型不标注颜色，颜色全靠 HSV 后处理不可靠；红旗/指示牌误检是模型分类缺陷，纯后处理无法根治。

**方案**：换用 [TrafficRules](https://github.com/LIU42/TrafficRules) 专门训练的 YOLO11n 模型，直接输出红绿灯颜色+形状（8类），治本。已验证模型 IO 格式与现有 YOLOv8 后处理兼容，cv::dnn 可加载。

**黄灯**：用户要求"红绿灯非红非绿即判黄"（不做专门黄灯训练）。采用双阈值 + 小框 HSV 仲裁实现。

## 已验证的模型信息

* 文件：`/tmp/TrafficRules/inferences/models/detection-fp32.onnx`（10.5MB）

* 输入 `images`：`[1,3,480,640]`（高480宽640，RGB，归一化\[0,1]，NCHW）

* 输出 `output0`：`[1,12,6300]`（4坐标 + 8类别，6300框）

* 8类：F0,F1,L0,L1,S0,S1,R0,R1（F=圆形,L=左箭,S=直行,R=右箭；**0=红,1=绿**）

* 预处理：直接 `resize(640,480)`，**无 letterbox**（匹配训练分布）

* conf=0.25, iou=0.45

## 改动文件

| 文件                                                                                                                         | 改动                                 |
| -------------------------------------------------------------------------------------------------------------------------- | ---------------------------------- |
| [DetectionEngine.h](file:///home/cccc/Project/360_driving_assistan/client/src/services/video/common/DetectionEngine.h)     | 新增成员/方法/常量；删除 `hsvFallbackScan` 声明 |
| [DetectionEngine.cpp](file:///home/cccc/Project/360_driving_assistan/client/src/services/video/common/DetectionEngine.cpp) | 主体改造（双模型、TL推理、黄灯启发式、detectAll重构）   |
| `client/resources/models/trafficrules-yolo11n.onnx`                                                                        | 新增（复制自上述 onnx）                     |
| 三个调用方 `.cpp`                                                                                                               | **不改**（接口透明）                       |
| `client/CMakeLists.txt`                                                                                                    | **不改**（模型按相对路径查找）                  |

## 架构：双模型 + 接口拆分

保留 `yolov8n` 检车辆，新增 `m_tlNet`（TrafficRules）检红绿灯。关键重构：`detectVehicles`/`detectTrafficLights` **不再调 detectAll**，各自只跑所需模型，避免无用推理。

* `detectAll`：`m_net`(车辆) + `m_tlNet`(红绿灯) 两次推理

* `detectVehicles`：仅 `m_net`

* `detectTrafficLights`：仅 `m_tlNet`；若 `m_tlLoaded=false` 回退到 `m_net` + 旧 `filterTrafficLights`(HSV)

性能：每检测帧多一次 480×640 YOLO11n 推理（CPU 约20-40ms）。当前每2帧检测一次，平均+10-20ms/帧。实测不达标可降为每3帧检测。

## DetectionEngine.h 新增

```cpp
// TrafficRules 模型
static constexpr int TL_INPUT_WIDTH  = 640;
static constexpr int TL_INPUT_HEIGHT = 480;   // 高<宽
static constexpr int TL_NUM_CLASSES  = 8;
static constexpr float TL_CONF_THRESHOLD = 0.25f;
static constexpr float TL_NMS_IOU = 0.45f;
static const char* TL_CLASSES[8];
// 黄灯启发式阈值
static constexpr float TL_HIGH_CONF = 0.45f;  // >=此值直接采信模型
static constexpr float TL_LOW_CONF  = 0.30f;  // <此值且HSV不明则判黄
static constexpr int TL_MIN_BOX = 6;          // 小框过滤放宽（原10过严）

cv::dnn::Net m_tlNet;
bool m_tlLoaded = false;

bool loadTrafficRulesModel();
void preprocessTL(const cv::Mat &frame, cv::Mat &blob, double &scaleX, double &scaleY);
void postprocessTL(const cv::Mat &output, int fw, int fh, double sx, double sy,
                   std::vector<Detection> &detections);
TrafficLightResult filterTrafficLightsTL(const cv::Mat &frame,
                                         const std::vector<Detection> &tlDetections,
                                         const std::vector<Detection> &vehicleDetections);
QString classifyTrafficLightColor(const Detection &tlDet, const cv::Mat &frame);
bool isTrafficRulesLoaded() const;
```

删除 `hsvFallbackScan` 声明与实现（无调用点）。保留 `analyzeTrafficLightColor` 系列（黄灯 HSV 仲裁 + 回退路径复用）、`temporalVoteColor`、`isMostlyInsideVehicle`、`nms`。

## DetectionEngine.cpp 关键实现

### preprocessTL（直接 resize，X/Y 缩放分离）

```cpp
cv::resize(frame, resized, cv::Size(TL_INPUT_WIDTH, TL_INPUT_HEIGHT));  // 拉伸
blob = cv::dnn::blobFromImage(resized, 1.0/255.0,
        cv::Size(TL_INPUT_WIDTH, TL_INPUT_HEIGHT), cv::Scalar(0,0,0), true, false);
// scaleX = 640/fw, scaleY = 480/fh（不相等，坐标回映分别用）
```

### postprocessTL（复用现有 reshape 模式）

* `output.reshape(0, 12)` → `[12, 6300]`

* `numClasses = 8`，全部8类保留

* 坐标回映：`left=(cx-w/2)/scaleX`，`top=(cy-h/2)/scaleY`（分别用 X/Y 缩放）

* 小框过滤：`bw<TL_MIN_BOX || bh<TL_MIN_BOX`

* `className = TL_CLASSES[classId]`，classId 保持 0-7

### classifyTrafficLightColor（黄灯启发式核心）

```cpp
QString classifyTrafficLightColor(const Detection &tlDet, const cv::Mat &frame) {
    float s = tlDet.confidence;
    bool isRed = (tlDet.classId % 2 == 0);  // 偶数类=红
    // 高置信：直接采信模型
    if (s >= TL_HIGH_CONF) return isRed ? "red" : "green";
    // 中低置信：小框 HSV 仲裁（复用 analyzeTrafficLightColor）
    QString hsv = analyzeTrafficLightColor(frame, tlDet);
    if (hsv == "red" || hsv == "green") return hsv;
    if (hsv == "yellow") return "yellow";
    // HSV 不明：极低置信判黄，否则维持模型（避免远处误黄）
    if (s < TL_LOW_CONF) return "yellow";
    return isRed ? "red" : "green";
}
```

安全网：HSV 完全无高亮像素（远处暗灯）时 `analyzeTrafficLightColor` 返回 unknown，配合 `s<TL_LOW_CONF` 判黄仍可能误判远处暗红/暗绿为黄。如需更稳，可要求 HSV 至少有1个高亮像素才判黄（实现时在 `analyzeTrafficLightColor` 增加 `hasBright` 出参）。

### filterTrafficLightsTL

* 位置过滤保留（`centerY < frame.rows*0.55`，可放宽0.65）

* 长宽比收紧到 2.5（箭头/圆形灯本身接近1）

* 车辆包含过滤保留（`isMostlyInsideVehicle(det, vehicleDetections)`），刹车灯安全网

* 颜色用 `classifyTrafficLightColor`

* `type="trafficrules"`，`temporalVoteColor` 保留（黄灯抖动平滑）

### detectAll 重构

```
1. preprocess(yolov8n) → forward → postprocess → nms → filterVehicles → vehicleResults
2. if m_tlLoaded:
     preprocessTL → m_tlNet.forward → postprocessTL → nms → filterTrafficLightsTL(frame, tlDets, vehicleDets)
   else:
     filterTrafficLights(frame, vehicleDets)  // 旧 HSV 回退
```

### drawDetections 适配

* 标签保持颜色文字（`RED - Stop`/`GREEN - Go`/`YELLOW - Caution`/`TL ?`），视觉一致

* 删除 `type=="hsv_fallback"` 分支；`type=="trafficrules"` 走实线框 thickness=2

## 黄灯可靠性分析

| 场景           | s\_max    | HSV     | 输出                   | 正确?  |
| ------------ | --------- | ------- | -------------------- | ---- |
| 近处明确红/绿      | ≥0.45     | 不走      | red/green            | ✓    |
| 真黄灯（训练无黄）    | 0.20-0.35 | yellow  | yellow               | ✓    |
| 真黄灯，HSV模糊    | 0.25      | unknown | yellow(s<0.30)       | ✓    |
| 远处红/绿，HSV看不清 | 0.32      | unknown | red/green(维持模型)      | ✓    |
| 远处暗灯，模型极低    | 0.22      | unknown | yellow               | ⚠️风险 |
| 红旗/指示牌       | —         | —       | 不进函数(TrafficRules不检) | ✓治本  |

风险点（远处暗灯误黄）靠 `TL_LOW_CONF` 收紧 + 可选 `hasBright` 安全网缓解。

## 验证方法

1. **离线对比**：对 `test_images/{front,left,right,rear}.png` 跑 TL 推理，输出 `[class,conf,bbox,color]`，与 `/tmp/TrafficRules/run_test.py` 已验证结果对比（类别/颜色必须一致）
2. **红旗场景**：验证 TrafficRules 不输出红绿灯框（治本验证）
3. **指示牌场景**：三个连排指示牌不再误检
4. **远处红绿灯**：`bw/bh` 在6-10像素区间仍被检出
5. **黄灯**：合成黄灯图，验证 `classifyTrafficLightColor` 返回 `"yellow"`
6. **回退**：临时重命名 onnx，验证回退 yolov8n+HSV 路径仍工作
7. **性能**：测 `detectAll` 耗时，目标<33ms；不达标降为每3帧检测

## 实施顺序

1. 复制 onnx → `client/resources/models/trafficrules-yolo11n.onnx`
2. 改 `DetectionEngine.h`（成员/常量/声明，删 hsvFallbackScan 声明）
3. 改 `DetectionEngine.cpp`：构造函数双模型加载 → `loadTrafficRulesModel`/`preprocessTL`/`postprocessTL` → `classifyTrafficLightColor` → `filterTrafficLightsTL` → 重构 `detectAll`/`detectVehicles`/`detectTrafficLights` → 删 `hsvFallbackScan` 实现 → `drawDetections` 适配
4. 编译（`build/` 目录 cmake build）
5. 离线对比 + 集成测试 + 性能测试

