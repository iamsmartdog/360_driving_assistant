# AVM 鸟瞰：交互式点击描点标定 + 真实尺寸画布 + fushi 抠图

## Context（为什么改）

用户照片里铺的是 6m×10m 黑白棋盘格标定布（40cm 方格），颜色和地面反差大，肉眼一眼能看清4角。但当前标定面板 [CalibrationPanel.qml](file:///home/cccc/Project/360_driving_assistant/client/src/views/features/bird/CalibrationPanel.qml) 用 SpinBox 手动输 8 个数字，且标定模式下 [BirdView.qml](file:///home/cccc/Project/360_driving_assistant/client/src/views/features/bird/BirdView.qml#L469-L486) 仍显示合成鸟瞰图（看不到原图），用户根本不知道填什么。

目标：参照 AVM 标准流程（https://blog.51cto.com/u_13157605/6037935），实现"进入标定 → 右侧显示当前方向校正后原图 → 鼠标点击4次描标定布4角 → orderPoints 自动排序 → getPerspectiveTransform 出单应矩阵"，画布按真实尺寸（1像素=1cm）布局，fushi.png 阈值抠白背景后贴画布中央。

## 关键发现（必改 Bug）

**`computeIPMHomography()` ([BirdRecordService.cpp:344-351](file:///home/cccc/Project/360_driving_assistant/client/src/services/video/bird/BirdRecordService.cpp#L344-L351)) 永远调用 `generateDefaultSrcPts()`，完全忽略 `m_userSrcPts`** —— 即使用户在 SpinBox 改了坐标，预计算的单应矩阵仍是默认梯形。这是当前标定"改了没用"的根因，必须修。

## 画布布局（1像素=1cm）

- 画布：`700×1100`（标定布 6m×10m + shiftW=shiftH=50cm 边距），`m_pixelPerMeter=100.0`
- 车辆矩形（居中）：`x∈[260,440], y∈[350,750]`（180cm宽 × 400cm长）

| 方向 | dstX | dstY | dstW | dstH | IPM dstSize(旋转前) |
|------|------|------|------|------|---------------------|
| front | 0 | 0 | 700 | 350 | (700, 350) |
| rear | 0 | 750 | 700 | 350 | (700, 350) |
| left | 0 | 0 | 260 | 1100 | (1100, 260) |
| right | 440 | 0 | 260 | 1100 | (1100, 260) |

- 4个对角为2路重叠区（alpha 渐变融合），中心车辆区无 IPM 覆盖（留给 fushi.png）
- alpha 渐变只朝车辆中心单边渐变（front 渐变底边、rear 渐变顶边、left 渐变右边、right 渐变左边）

## 文件修改清单

### 1. [BirdRecordService.h](file:///home/cccc/Project/360_driving_assistant/client/src/services/video/bird/BirdRecordService.h)
- 新增 `Q_PROPERTY(int calibClickIndex ...)` 和 `Q_INVOKABLE void clickCalibrationPoint(position, imgX, imgY)`
- 新增私有方法：`generateCalibrationPreview()`、`getProjectionRegion(position, &dstX,&dstY,&dstW,&dstH, &ipmDstSize)`
- 新增成员 `int m_calibClickIndex = 0`
- 删除 `drawCalibrationOverlay` 声明（被 generateCalibrationPreview 取代）

### 2. [BirdRecordService.cpp](file:///home/cccc/Project/360_driving_assistant/client/src/services/video/bird/BirdRecordService.cpp)

**(a) 构造函数 L25-27**：画布参数改 `m_birdCanvasW(700), m_birdCanvasH(1100), m_pixelPerMeter(100.0)`

**(b) 修复 Bug — 重写 `precomputeHomographies()` (L1022-1049)**：优先用 `m_userSrcPts[pos]`，无标定时才回退 `generateDefaultSrcPts`。每路用 `getProjectionRegion()` 取 ipmDstSize，不再硬编码 1/3。

**(c) 新增 `getProjectionRegion()`**：按上表返回坐标和 IPM dstSize，保证 ipmDstSize 与放置区域一致（避免 resize 变形）。

**(d) 重写 `warpToBirdEye()` (L517-649)**：用 `getProjectionRegion()` 替代硬编码 1/3 布局；删除 resize（IPM 输出已等于目标区域）；旋转/翻转逻辑保留（front=flip(0), rear=不变, left=transpose+flip(0), right=transpose+flip(1)）；alpha 渐变按上表单边朝车辆方向。

**(e) 重写 `processFrame()` (L167-190)**：标定模式分支调 `generateCalibrationPreview()` 并 return（不生成合成图、不录制）；删除 `drawCalibrationOverlay(frame)` 调用。

**(f) 新增 `generateCalibrationPreview()`**：取当前选中方向校正后原图，画梯形连线 + 4个彩色角点 P0-P3（带序号）+ 顶部提示文字，BGR→RGB 后 `updateFrame("calib", qimg.copy())`。

**(g) 删除 `drawCalibrationOverlay()` (L1408-1461)**。

**(h) 修改 `loadCarTopDownImage()` (L992-1016)** — fushi.png 阈值抠图：转 BGRA 后转灰度，`gray>=200` 置 alpha=0（白背景透明），否则 alpha=255（车体不透明）。

**(i) 新增 `clickCalibrationPoint()`**：调 `updateCalibrationPoint(position, m_calibClickIndex, x, y)`，`m_calibClickIndex=(m_calibClickIndex+1)%4`，emit 信号，主动调一次 `generateCalibrationPreview()` 即时反馈。

**(j) `drawCarHighlight()` (L1137-1187)**：缩放改为按车辆矩形 `180×400px`（不再用画布宽20%），放置位置改为 `(260, 350)`（车辆矩形左上角，不再居中）。

**(k) `setSelectedCamera()`**：切换方向时 `m_calibClickIndex=0` 重置描点序号。

**(l) `loadCalibration()` (L1358-1406)**：不再从 ini 读 `birdCanvasW/H/pixelPerMeter`（画布尺寸固定 700/1100/100），避免旧配置覆盖。

### 3. [BirdRecordViewModel.h](file:///home/cccc/Project/360_driving_assistant/client/src/viewmodels/video/bird/BirdRecordViewModel.h) / [.cpp](file:///home/cccc/Project/360_driving_assistant/client/src/viewmodels/video/bird/BirdRecordViewModel.cpp)
- 新增 `Q_PROPERTY(int calibClickIndex)` 和 `Q_INVOKABLE void clickCalibrationPoint(position, imgX, imgY)`，转发到 Service
- 构造函数 connect Service `calibClickIndexChanged` → ViewModel `calibClickIndexChanged`

### 4. [CalibrationPanel.qml](file:///home/cccc/Project/360_driving_assistant/client/src/views/features/bird/CalibrationPanel.qml) — 完全重写
- 移除 SpinBox 输入，改为：摄像头选择 + "正在描 P{N}/4" 状态 + 4点坐标只读 Label + 操作提示 + [重置][保存] 按钮
- 点击操作在 BirdView.qml 主区域完成（不在面板内）

### 5. [BirdView.qml](file:///home/cccc/Project/360_driving_assistant/client/src/views/features/bird/BirdView.qml)
- 主画面 L461-517：`birdEyeImage` 设 `visible: !calibrationMode`；新增 `calibImage`（`visible: calibrationMode`，source=`image://videoframe/calib?frameCounter`，PreserveAspectFit）
- `calibImage` 内 MouseArea `onClicked`：按映射公式把鼠标坐标转原图像素坐标，调 `viewModel.clickCalibrationPoint(selectedCamera, imgX, imgY)`
- 标定面板宽度 380→280（L525，不再需要 SpinBox）

## QML 点击坐标 → 原图像素坐标映射

```
scale = min(compW/srcW, compH/srcH)   // PreserveAspectFit 缩放比
dispW = srcW*scale;  dispH = srcH*scale
offX = (compW-dispW)/2;  offY = (compH-dispH)/2
imgX = (mouse.x - offX) / scale
imgY = (mouse.y - offY) / scale
// 边界检查：点在留白区(offX..offX+dispW 之外)则忽略
```

## 实现顺序

1. **fushi.png 抠图**（独立）→ 改 `loadCarTopDownImage`
2. **画布布局**（依赖1）→ 构造函数参数 + `getProjectionRegion` + 修复 `precomputeHomographies` Bug + 重写 `warpToBirdEye`
3. **车辆叠加**（依赖2）→ `drawCarHighlight` 按车辆矩形缩放放置
4. **标定预览**（依赖2）→ `generateCalibrationPreview` + `processFrame` 分支 + 删 `drawCalibrationOverlay` + `calibClickIndex`/`clickCalibrationPoint`
5. **ViewModel 转发**（依赖4）→ 新增属性和方法
6. **QML 改造**（依赖5）→ 重写 CalibrationPanel + BirdView 加 calibImage/MouseArea
7. **持久化**（依赖2）→ `loadCalibration` 不再读画布参数

## 验证方法

1. **fushi 抠图**：加载后 `cv::imwrite` 保存检查 alpha 通道，白区=0、车体=255
2. **画布尺寸**：`drawOverlay` 打印 `frame.cols×rows` 应为 700×1100，4路投影无空白（除中心车辆区），重叠区无接缝
3. **Bug 修复**：标定模式改4点 → 退出标定 → 鸟瞰图必须变化（原代码不变）
4. **描点交互**：进入标定 → 显示原图+4点 → 点击4角 → P0-P3 依次更新 → 第5次回绕 → 切换方向独立标定
5. **端到端**：描完4路 → 退出标定 → 鸟瞰图梯形与标定布对齐、直线物体拼接处无错位 → 保存重启恢复
6. **编译**：`cd client && cmake --build .` 通过

## 风险点

- IPM dstSize 必须与放置区域严格一致（否则 resize 变形）—— `getProjectionRegion` 内部保证
- 左/右路 IPM dstSize=(1100,260)，transpose 后变 (260,1100) 匹配放置区域
- fushi.png 阈值 200 若因 JPEG 噪声不准，可加形态学开运算去噪
- 旧 ini 标定点像素坐标仍有效（相对原图），但画布字段需在 `loadCalibration` 忽略
