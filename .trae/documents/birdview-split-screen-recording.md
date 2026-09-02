# 鸟瞰模式分屏显示与自动录制实现计划

## Summary

将鸟瞰模式从静态单图展示改造为实时分屏显示：左侧为 360° AVM 鸟瞰拼接图（前置摄像头实时捕获 + 后/左/右静态画面），右侧为前置摄像头实时画面。同时实现每 300 帧（约 10 秒）自动录制 AVI 视频文件的分段录制功能。

## Current State Analysis

### 当前架构

* `BirdRecordService` 使用 `QtConcurrent::run` 在后台线程一次性构建静态 AVM 图（从 4 张 PNG 读取 → 鱼眼矫正 → IPM 投影 → 亮度均衡 → 拼接 → 白平衡 → 车辆图标）

* 构建完成后推送一次 `bird_avm` 帧到 `VideoFrameProvider`，之后不再更新

* `BirdView.qml` 显示单个全屏 `Image`

* 无摄像头采集、无视频文件读取、无录制功能

* `BirdRecordViewModel` 仅代理 `isRunning`/`imagesLoaded`/`frameCounter` 属性

### 参考模式

* `ReverseRecordService`：QTimer(33ms) 驱动的实时帧处理循环，支持 `m_targetFrames=300` 的分段录制

* `VideoRecorderService`：`cv::VideoWriter` + MJPG + AVI 录制，文件命名 `行车记录_yyyyMMdd_HHmmss.avi`

* `VideoFrameProvider`：基于 ID 的多帧源 HashMap，已支持 `bird_avm`，只需新增 `bird_cam`

### 资源文件

* yaml 参数：`client/resources/surround_view/yaml/{front,back,left,right}.yaml`，分辨率 960×640

* 静态图片：`client/resources/surround_view/images/{front,back,left,right}.png` + `car.png`

* 融合权重：`weights.png` (4通道 RGBA)、`masks.png` (4通道 RGBA)

* 现有录制文件：`~/Videos/360DrivingAssistant/鸟瞰_*.avi`，1280×720\@30fps MJPG

## Proposed Changes

### 1. BirdRecordService.h — 头文件改造

**新增成员变量：**

```cpp
// 实时采集
cv::VideoCapture m_frontCamera;        // 前置摄像头（webcam）
cv::Mat m_staticFrames[3];             // back/left/right 静态图片（读取一次复用）
QTimer *m_captureTimer;                // 采集定时器 (33ms ≈ 30fps)
bool m_cameraOpened = false;

// 录制
cv::VideoWriter m_writer;
bool m_isRecording = false;
QString m_recordFileName;
QString m_videoDir;
int m_writtenFrames = 0;               // 当前段已写入帧数
int m_recordDuration = 0;              // 当前段时长（秒）
static constexpr int FRAMES_PER_SEGMENT = 300;  // 每段 300 帧
static constexpr int RECORD_FPS = 30;
static constexpr int RECORD_W = 1280;  // 录制分辨率（与项目约定一致）
static constexpr int RECORD_H = 720;
```

**新增 Q\_PROPERTY：**

```cpp
Q_PROPERTY(bool isRecording READ isRecording NOTIFY recordingStateChanged)
Q_PROPERTY(QString recordFileName READ recordFileName NOTIFY recordFileNameChanged)
Q_PROPERTY(int recordDuration READ recordDuration NOTIFY recordDurationChanged)
Q_PROPERTY(int writtenFrames READ writtenFrames NOTIFY writtenFramesChanged)
Q_PROPERTY(QString videoDir READ videoDir NOTIFY videoDirChanged)
```

**新增方法：**

```cpp
bool isRecording() const;
QString recordFileName() const;
int recordDuration() const;
int writtenFrames() const;
QString videoDir() const;

Q_INVOKABLE bool openCamera(int deviceId = 0);
Q_INVOKABLE void closeCamera();

private:
    bool loadCalibrationData();        // 加载 yaml/weights/masks/car (一次性，无图像处理)
    void onCaptureTimer();             // 定时器槽函数
    void processFrame();               // 每帧处理：采集→矫正→投影→拼接→推送→录制
    void startRecording();             // 开始新录制段
    void stopRecording();              // 停止并保存当前段
    void pushFrame(const QString &id, const cv::Mat &mat);  // 推送帧到 provider
```

**移除/修改：**

* 移除 `QFutureWatcher *m_buildWatcher` 和 `QtConcurrent` 后台构建（改为同步加载 + 定时器处理）

* 移除 `useStaticImages` 属性（不再需要，始终使用混合源）

* `loadDataAndBuild()` → `loadCalibrationData()`（仅加载参数，不读取图片）

* `buildProjectedImages()` → 移除（改为 per-frame 在 `processFrame()` 中执行）

* `pushFrame()` → 泛化为 `pushFrame(const QString &id, const cv::Mat &mat)`

### 2. BirdRecordService.cpp — 实现改造

**构造函数：**

* 初始化 `m_captureTimer` (33ms)、`m_videoDir` = `~/Videos/360DrivingAssistant`

* 连接 `m_captureTimer` → `onCaptureTimer`

* 确保视频目录存在

**`start()`** **方法改造：**

```
1. 调用 loadCalibrationData() 同步加载 yaml/weights/masks/car (主线程，<100ms)
2. 读取 back/left/right 静态 PNG 到 m_staticFrames[3]
3. 打开前置摄像头 (默认 device 0)
4. 启动自动录制 (startRecording)
5. 启动 m_captureTimer
6. emit runningChanged
```

**`stop()`** **方法改造：**

```
1. 停止 m_captureTimer
2. 停止录制 (stopRecording)
3. 关闭摄像头
4. emit runningChanged
```

**`loadCalibrationData()`** **— 从** **`loadDataAndBuild()`** **简化：**

* 仅执行步骤 1-3：加载 4 个 yaml 参数、weights/masks、car 图标

* 不执行 buildProjectedImages/makeLuminanceBalance/stitchAllParts（这些改为 per-frame）

* 返回 bool 表示加载是否成功

**`onCaptureTimer()`** **→** **`processFrame()`** **核心循环：**

```
1. 从 m_frontCamera 读取一帧 → resize 到 960×640 (yaml 标定分辨率)
   - 如果读取失败，跳过本帧
2. 组装 4 路源图像:
   - sources[0] = 前置摄像头帧 (实时)
   - sources[1] = m_staticFrames[0] (back.png 静态)
   - sources[2] = m_staticFrames[1] (left.png 静态)
   - sources[3] = m_staticFrames[2] (right.png 静态)
3. 对每路相机执行 undistort → project → flipDirection → 存入 m_cams[i].projected
4. makeLuminanceBalance() — 亮度均衡 (per-frame 重计算)
5. stitchAllParts() — 拼接
6. makeWhiteBalance() — 白平衡
7. pasteCarIcon() — 叠加车辆图标
8. pushFrame("bird_avm", m_birdAvm) — 推送鸟瞰图到左侧
9. pushFrame("bird_cam", frontFrame) — 推送原始摄像头画面到右侧
10. 录制: 如果 m_isRecording && m_writer.isOpened():
    - resize m_birdAvm 到 1280×720
    - m_writer.write(resized)
    - m_writtenFrames++
    - emit writtenFramesChanged
    - 如果 m_writtenFrames >= 300: stopRecording() + startRecording() (分段)
11. m_frameCounter++; emit frameCounterChanged()
```

**`startRecording()`** **— 开始新录制段：**

```
1. 生成文件名: 鸟瞰_yyyyMMdd_HHmmss.avi
2. m_writer.open(path, MJPG, 30, Size(1280, 720), true)
3. m_writtenFrames = 0; m_recordDuration = 0
4. m_isRecording = true
5. emit recordingStateChanged / recordFileNameChanged
```

**`stopRecording()`** **— 停止并保存：**

```
1. m_isRecording = false
2. m_writer.release()
3. emit recordingStateChanged
4. qDebug() 记录保存信息
```

**`pushFrame(id, mat)`** **— 泛化推送：**

```cpp
cv::Mat rgb;
cv::cvtColor(mat, rgb, cv::COLOR_BGR2RGB);
QImage img(rgb.data, rgb.cols, rgb.rows, rgb.step, QImage::Format_RGB888);
m_frameProvider->updateFrame(id, img.copy());
```

### 3. BirdRecordViewModel.h/.cpp — 代理新属性

**新增 Q\_PROPERTY：**

* `isRecording` / `recordFileName` / `recordDuration` / `writtenFrames` / `videoDir`

**新增信号转发：**

* `recordingStateChanged` / `recordFileNameChanged` / `recordDurationChanged` / `writtenFramesChanged` / `videoDirChanged`

**移除：**

* `useStaticImages` 属性及其信号

### 4. BirdView\.qml — 分屏布局

```
Window {
    width: 1200  // 加宽以容纳分屏
    height: 800
    
    // 左侧：鸟瞰 AVM 拼接图
    Image {
        id: avmImage
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: parent.width / 2
        source: "image://videoframe/bird_avm?" + viewModel.frameCounter
        fillMode: Image.PreserveAspectFit
        cache: false
    }
    
    // 右侧：前置摄像头实时画面
    Image {
        id: camImage
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: parent.width / 2
        source: "image://videoframe/bird_cam?" + viewModel.frameCounter
        fillMode: Image.PreserveAspectFit
        cache: false
    }
    
    // 标签
    Text { "鸟瞰图" (左侧顶部) }
    Text { "实时摄像头" (右侧顶部) }
    
    // 录制状态指示器
    Rectangle {
        // 右上角红色圆点 + "录制中 00:10 (150/300帧)"
        visible: viewModel.isRecording
    }
    
    // 关闭按钮 (底部居中)
    Button { "关闭" }
}
```

### 5. CMakeLists.txt — 无需修改

已包含 `Qt5::Concurrent` 和所有必要依赖。虽然不再使用 `QtConcurrent::run`，但保留链接不影响功能。

## Assumptions & Decisions

1. **4 路相机**：用户提到"2个不同方向的相机"，但备注中明确提及前/左/右/后 4 个方向，且现有 yaml/weights/masks 均为 4 路标定。保持 4 路 AVM 架构不变。
2. **前置摄像头 = 实时 webcam**：使用 `cv::VideoCapture(0)` 打开默认摄像头，捕获帧 resize 到 960×640 后送入鱼眼矫正流水线。
3. **后/左/右 = 静态 PNG**：用户未提供视频文件，使用现有 `back.png`/`left.png`/`right.png` 作为静态源。架构支持后续替换为视频文件（只需将 `cv::imread` 改为 `cv::VideoCapture::read`）。
4. **录制内容 = AVM 鸟瞰图**：录制拼接后的 1200×1600 鸟瞰图，resize 到 1280×720\@30fps MJPG AVI（与项目约定和现有文件一致）。
5. **分段录制**：连续录制，每 300 帧自动保存并开始新文件，文件名 `鸟瞰_yyyyMMdd_HHmmss.avi`，保存到 `~/Videos/360DrivingAssistant/`。
6. **性能**：per-frame 处理（4×remap + 4×warpPerspective + 亮度均衡 + 拼接 + 白平衡）预计 15-25ms，满足 30fps。主线程 QTimer 驱动，与 `ReverseRecordService`/`VideoRecorderService` 同模式。
7. **摄像头分辨率适配**：webcam 实际分辨率可能非 960×640，在 `processFrame()` 中 resize 到 yaml 标定分辨率后再矫正。

## Verification Steps

1. **编译验证**：`cd client/build && cmake .. && make -j$(nproc)` 确保无编译错误
2. **运行验证**：启动客户端，进入鸟瞰模式：

   * 左侧显示 AVM 鸟瞰拼接图（前置摄像头实时 + 后/左/右静态）

   * 右侧显示前置摄像头原始实时画面

   * 录制状态指示器显示"录制中"和帧计数
3. **录制验证**：

   * 检查 `~/Videos/360DrivingAssistant/` 下生成 `鸟瞰_yyyyMMdd_HHmmss.avi` 文件

   * 每 300 帧自动切换到新文件

   * 视频可播放，内容为鸟瞰 AVM 画面
4. **关闭验证**：关闭鸟瞰窗口时录制正常保存，摄像头正常释放

