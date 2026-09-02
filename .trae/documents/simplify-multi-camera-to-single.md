# 精简多摄像头逻辑为单摄像头

## 背景
用户只有一个摄像头，当前 BirdRecordService（4路：前/左/右/后）和 ReverseRecordService（2路：左后/右后）的多摄像头逻辑都是冗余的，需要精简为单摄像头方案。

## 修改范围

### 1. BirdRecordService（鸟瞰录制服务）

**文件**: `client/src/services/video/bird/BirdRecordService.h` + `.cpp`

精简内容：
- `openCamera(4个参数)` → `openCamera(int camId = 0)`，只开一个摄像头
- `closeCameras()` → `closeCamera()`，只关一个摄像头
- 删除 `m_leftCapture`, `m_rightCapture`, `m_rearCapture` 三个 VideoCapture
- 删除 `m_leftCamOpened`, `m_rightCamOpened`, `m_rearCamOpened` 三个 bool
- 删除 `m_leftWarpMatrix`, `m_rightWarpMatrix` 两个透视变换矩阵
- 删除 `m_birdEyeSize`
- 删除 `warpPerspective()` 方法
- `processFrame()` 简化：只从单摄像头读帧，发送 `frontFrameReady` 信号（鸟瞰拼接逻辑删除）
- 删除 `birdEyeFrameReady` 信号（不再有拼接帧）

### 2. BirdView.qml（鸟瞰界面）

**文件**: `client/src/views/features/bird/BirdView.qml`

精简内容：
- 左侧不再分"左侧鸟瞰摄像头/右侧鸟瞰摄像头"上下两个区域，改为显示单摄像头画面
- 右侧仍为实时摄像头影像（保持不变）
- 删除拼接线指示器
- 删除右上角的左/右/后方小画面叠加（"视频填充"占位区域）
- 状态栏 "鸟瞰拼接：2路摄像头" → "摄像头：单路"
- 标题 "鸟瞰视角（多摄像头拼接）" → "鸟瞰视角"

### 3. ReverseRecordService（倒车录制服务）

**文件**: `client/src/services/video/reverse/ReverseRecordService.h` + `.cpp`

精简内容：
- `openCamera(2个参数)` → `openCamera(int camId = 0)`，只开一个摄像头
- `closeCameras()` → `closeCamera()`
- 删除 `m_rightRearCapture`，只保留一个 `m_capture`
- 删除 `m_leftRearCamOpened`, `m_rightRearCamOpened`，只保留 `m_camOpened`
- 删除 `m_leftWarpMatrix`, `m_rightWarpMatrix`, `m_reverseSize`
- 删除 `warpPerspective()` 和 `blendSeam()` 方法
- `processFrame()` 简化：从单摄像头读帧，绘制辅助线+障碍物检测后发送
- 删除 `leftRearFrameReady`, `rightRearFrameReady` 信号，只保留 `reverseFrameReady`

### 4. ReverseRecordViewModel

**文件**: `client/src/viewmodels/video/reverse/ReverseRecordViewModel.h` + `.cpp`

精简内容：
- `openCamera(int leftRearCamId, int rightRearCamId)` → `openCamera(int camId = 0)`
- `closeCameras()` → `closeCamera()`

### 5. ReverseModeView.qml

**文件**: `client/src/views/features/reverse/ReverseModeView.qml`

精简内容：
- 占位文字 "倒车拼接画面\n左后 + 右后摄像头" → "倒车辅助画面"
- 关闭时调用 `reverseRecordViewModel.closeCamera()` 而非 `closeCameras()`

## 不需要修改的文件
- `main.cpp` — 已注册的 context property 名称不变
- `CMakeLists.txt` — 文件路径不变
- `qml.qrc` — 路径不变

## 验证
```bash
cd /home/cccc/Project/360_driving_assistant/build
cmake ../client && make -j$(nproc)
```
