# 360度智能行车辅助系统 - 客户端功能实现计划

## Context
根据PDF规格文档，当前客户端存在以下问题：
1. **视频闪烁** — base64 data URL方案导致QML Image每帧重载闪烁
2. **行车模式四画面** — PDF无此要求，应为单画面实时视频
3. **多个功能为占位** — 视频数据列表、特征图片列表、场景重现等需要真实实现
4. **功能缺失** — 手动截图上传、倒车辅助线、播放记录上传等

---

## 实施步骤

### 步骤1：修复视频闪烁（QQuickImageProvider）⚡最高优先级

**新建文件**：
- `client/src/services/video/VideoFrameProvider.h` — 继承QQuickImageProvider，缓存QImage帧
- `client/src/services/video/VideoFrameProvider.cpp` — requestImage()返回缓存帧，updateFrame()更新帧

**修改文件**：
- `VideoRecorderService.h/cpp` — 删除`currentFrameUrl`属性，新增`m_frameProvider`指针和`frameCounter`属性，processFrame()改为调用provider->updateFrame()
- `main.cpp` — 创建VideoFrameProvider，通过`engine.addImageProvider("videoframe", provider)`注册，传给VideoRecorderService
- `CMakeLists.txt` — 添加VideoFrameProvider源文件

**QML刷新方式**：`source: "image://videoframe/live?t=" + videoRecorderService.frameCounter`

**线程安全**：QMutex保护QImage读写

---

### 步骤2：行车模式改为单画面

**修改文件**：`DrivingModeView.qml`

- 删除Grid+Repeater四画面布局和"单画面/四画面"切换按钮
- 改为单一Image组件占满右侧区域
- 保留：车辆识别、红绿灯检测标注、水印、录制指示器、状态栏

---

### 步骤3：视频数据列表真实实现

**修改文件**：`VideRecordView.qml`

- 删除硬编码ListModel，绑定到`videoRecordViewModel.videoListModel`
- 添加刷新按钮和自动刷新
- 播放按钮传videoId+filePath+lastPlaySec给VideoPlaybackWindow
- 录制完成后自动刷新列表

---

### 步骤4：场景重现增强

**修改文件**：`VideoPlaybackWindow.qml`

- frameImage绑定到ImageProvider（`image://videoframe/playback`）
- 确保进度条拖拽、倍速切换、截图上传正常
- 关闭/结束时上传播放记录
- 续播功能确认

---

### 步骤5：特征数据上传

**修改文件**：`DrivingModeView.qml` + `VideoRecorderService.h/cpp`

- 添加"手动截图"按钮，调用`takeManualScreenshot()`
- 自动截图改为：仅在有检测目标时才截图（每15秒）
- VideoRecorderService新增`Q_INVOKABLE QString takeManualScreenshot()`

---

### 步骤6：特征图片列表

**修改文件**：`FeatureRecordView.qml`

- 图片路径处理（本地file://前缀）
- 确认双击放大查看功能
- 确认刷新按钮正常

---

### 步骤7：倒车模式增强

**修改文件**：`ReverseRecordService.h/cpp` + `ReverseModeView.qml`

- ReverseRecordService集成ImageProvider（`image://videoframe/reverse`）
- 倒车画面从videoRecorderService切换到reverseRecordService
- 确认辅助线绘制和WARNING/DANGER提示

---

### 步骤8：视频播放记录上传

**修改文件**：`VideRecordView.qml` + `VideoPlaybackWindow.qml`

- 传递videoId到播放窗口
- 关闭/结束时调用updatePlayRecord()
- 上传内容：视频名称、播放进度、播放时间

---

### 步骤9：鸟瞰模式集成ImageProvider

**修改文件**：`BirdRecordService.h/cpp` + `BirdView.qml`

- BirdRecordService添加ImageProvider（`image://videoframe/birdeye` + `image://videoframe/birdfront`）
- 画面从videoRecorderService切换到birdRecordService
- 确认自动录制功能

---

## 实施顺序

```
步骤1(ImageProvider) → 步骤2(单画面) → 步骤3(视频列表) → 步骤4(场景重现)
                    → 步骤5(特征上传) → 步骤6(特征图片)
                    → 步骤7(倒车增强)
                    → 步骤9(鸟瞰集成)
步骤8(播放记录上传) ← 依赖步骤3+4
```

## 验证方法
- 每步完成后`cd build && make client`编译验证
- 运行client，进入对应功能界面确认画面无闪烁、功能正常
- 确认摄像头打开/关闭生命周期正确
