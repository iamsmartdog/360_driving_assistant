# 鸟瞰模式：左右分屏 + 全新 AVM 拼接流水线（真实鱼眼标定）

## Context（为什么改）

鸟瞰模式（俯视模式）当前是空壳：

* [BirdRecordService.cpp](file:///home/cccc/Project/360_driving_assistant/client/src/services/video/bird/BirdRecordService.cpp) 只加载单张照片直接显示，无任何 IPM/拼接/鱼眼矫正（头文件注释明说"原 360°环视 IPM 拼接方法已全部删除"）。

* [BirdView.qml](file:///home/cccc/Project/360_driving_assistant/client/src/views/features/bird/BirdView.qml) 是个空黑窗，没有左右分屏、没有 Image。

用户要求（全新方案，不沿用已删除的点击标定实现）：

1. 鸟瞰模式 = **左右分屏**：左侧 = 4 路相机鱼眼矫正→逆透视(IPM)→拼接→中心叠加透明背景车辆图 的鸟瞰图；右侧 = 车前画面（暂用 `/mnt/hgfs/carfront.png` 占位，后续换实时摄像头）。
2. 4 路源图 = `/mnt/hgfs/` 下 `carfront.png`/`carafter.png`/`carleft.png`/`carright.png`（均为 2560×1696 RGBA 鱼眼照片）。
3. 流水线：**真实鱼眼畸变矫正（用户提供的真实 K/D 内参）→ 4点IPM逆透视 → 羽化拼接 → 中心叠加 fushi.png 抠图车辆**。
4. 每 10 秒自动录 300 帧，AVI，文件名 `鸟瞰_yyyyMMdd_HHmmss.avi`（合成帧 1280×720，左右拼接黑边居中）。

## 真实鱼眼标定数据（用户提供，针孔模型 5 系数）

* **内参矩阵 K**（标定分辨率 640×424，由 2560×1696 等比例下采样，系数 4.0）：

  * fx=531.4017560022933, fy=531.3698083055915

  * cx=340.5121891820963, cy=232.0990886525732

* **畸变系数 D**（k1,k2,p1,p2,k3，分辨率无关，直接用）：

  * \[-0.257272028772218, -0.1302531787363336, 0.0008829663197048428, -0.0006837432466250165, 0.5184868514845824]

* 总体标定误差 0.0619 像素（精度极好）

* **模型选择**：D 含 p1,p2（切向畸变）→ 用 **`cv::undistort`/`cv::initUndistortRectifyMap`+`cv::remap`**（针孔 5 系数模型），**不用** `cv::fisheye::`（那是 4 系数无切向）

### K 缩放（等比例，统一系数 4.0）

用户标定前把 2560×1696 **等比例**下采样（2560/4=640, 1696/4=424），标定图实际 640×424（"640×480"为近似口述）。D 分辨率无关直接用；K 按同一系数 4.0 **等比例**放大（保持 fx≈fy 方形像素，物理正确）：

* scale = 2560/640 = 1696/424 = 4.0

* fx' = 531.4017560022933 × 4.0 = 2125.6070

* fy' = 531.3698083055915 × 4.0 = 2125.4792

* cx' = 340.5121891820963 × 4.0 = 1362.0488

* cy' = 232.0990886525732 × 4.0 = 928.3964

* 校验：主点 (340.5, 232.1) 在 640×424 占比 (53.2%, 54.7%)；乘 4 得 (1362.0, 928.4) 在 2560×1696 占比 (53.2%, 54.7%) 一致 ✓；fx'≈fy' 保持方形像素 ✓

## 画布几何（1px = 1cm，沿用用户已确认几何）

* 画布 `1800×1200`（横向X=10m+2×4m边距，纵深Y=6m+2×3m边距）

* 车辆中心矩形：`x∈[810,990], y∈[375,825]`（180×450，车顶朝上，留给 fushi.png，不做 IPM）

* 四方向目标区域（IPM dst 局部尺寸 = 区域尺寸）：

  * front: `x∈[400,1400], y∈[0,375]` → 1000×375

  * rear:  `x∈[400,1400], y∈[825,1200]` → 1000×375

  * left:  `x∈[0,810], y∈[0,1200]` → 810×1200

  * right: `x∈[990,1800], y∈[0,1200]` → 810×1200

* 默认翻转（相机安装方向，可调）：front=恒等, rear=180°(h+v), left=恒等, right=hFlip

## 关键文件改动

### 1. [BirdRecordService.h](file:///home/cccc/Project/360_driving_assistant/client/src/services/video/bird/BirdRecordService.h)

新增私有成员与方法，保留现有录制/摄像头/属性接口不动：

* 鱼眼标定常量（K 原始值、D、标定分辨率 640×424，等比例系数 4.0）+ 运行时缩放后的 `cv::Mat m_K, m_D`

* 成员：`cv::Mat m_birdAvm`（缓存最终左侧鸟瞰图）、`cv::Mat m_camImage`（右侧车前图）、`bool m_avmBuilt`、`cv::Mat m_src[4]`

* 结构体 `DirConfig { std::vector<cv::Point2f> srcPts; int flip; }`，`m_dirCfg[4]`

* 方法：`bool buildBirdView()`、`cv::Mat undistortFisheye(const cv::Mat&)`、`void initCalibration(int targetW, int targetH)`、`cv::Mat warpDirection(int idx)`、`void blendIntoCanvas(cv::Mat& canvas, const cv::Mat& warped, int idx)`、`cv::Mat featherMask(int idx)`、`void orderPoints(std::vector<cv::Point2f>&)`、`void overlayCarIcon(cv::Mat& canvas)`、`cv::Mat composeRecordFrame()`

### 2. [BirdRecordService.cpp](file:///home/cccc/Project/360_driving_assistant/client/src/services/video/bird/BirdRecordService.cpp)

**重写核心，保留构造/录制/属性骨架：**

**(a)** **`initCalibration(targetW,targetH)`**：用硬编码 K 原始值（640×424 标定）+ D，按**等比例系数 scale = targetW/640**（= targetH/424）缩放得运行时 `m_K`、`m_D`（fx/fy/cx/cy 同乘 scale，保持方形像素）。预计算 `m_undistMap1/m_undistMap2`（`cv::initUndistortRectifyMap`，newCameraMatrix 用 `cv::getOptimalNewCameraMatrix(m_K,m_D,Size,1)` 保留全 FOV）。

**(b)** **`undistortFisheye(src)`**：`cv::remap(src, dst, m_undistMap1, m_undistMap2, INTER_LINEAR)`。4 路共用同一 K/D（同一相机）。

**(c)** **`loadStaticImages(dir)`**：改为加载 4 张图（`carfront`/`carafter`/`carleft`/`carright`，搜 `/mnt/hgfs` + dir，复用现有 `tryLoad` lambda），存入 `m_src[4]`；`m_camImage = m_src[front]`；`initCalibration(m_src[0].cols, m_src[0].rows)`；调 `buildBirdView()`。返回是否 4 张齐全。

**(d)** **`buildBirdView()`**（一次性缓存，仿项目记忆"画布缓存"策略）：

1. 4 路各 `undistortFisheye()` → `warpDirection(idx)`（IPM 到全画布 1800×1200，单应矩阵含平移到目标区域）
2. `blendIntoCanvas()`：4 张全画布 warped 用 `featherMask()`（区域内部=1，四边 30px 线性衰减到 0）加权累加 → `m_birdAvm`
3. `overlayCarIcon()`：fushi.png 阈值抠图（灰度≥200→alpha=0 否则 255），缩放到 180×450，alpha 混合到中心矩形
4. `m_avmBuilt=true`

**(e)** **`warpDirection(idx)`**：`orderPoints(srcPts)` 排序（TL=x+y最小, BR=x+y最大, TR=x-y最小, BL=x-y最大）→ `cv::convexHull` 凸性校验 → dstPts=目标区域4角（局部坐标）→ `H_ipm=cv::getPerspectiveTransform` → 平移到全画布 `H=T*H_ipm` → `cv::warpPerspective(src, warpedFull, H, Size(1800,1200))` → 按 `flip` 翻转。

**(f)** **`processFrame()`**：重写——若 `!m_avmBuilt` return；`frame = m_birdAvm.clone()`；`drawOverlay(frame)`（水印+时间）；推 `updateFrame("bird_avm", qimg)`；右侧推 `updateFrame("bird_cam", camQimg)`（m\_camImage 缩放后）；录制用 `composeRecordFrame()` 写 VideoWriter。

**(g)** **`composeRecordFrame()`**：左半 640×720 = AVM（PreserveAspectFit + `copyMakeBorder` 黑边居中），右半 640×720 = 车前图同样处理，`cv::hconcat` → 1280×720。

**(h) 默认源点**：每方向 `srcPts` = 全图内缩 8% 的 4 角（`(0.08W,0.08H)` 等），集中为常量便于后续微调。

**(i)** **`startAutoRecord()`**：VideoWriter 尺寸改 `1280×720`（原 640×480）。

### 3. [BirdView.qml](file:///home/cccc/Project/360_driving_assistant/client/src/views/features/bird/BirdView.qml) — 重写

仿 [ReverseModeView.qml](file:///home/cccc/Project/360_driving_assistant/client/src/views/features/reverse/ReverseModeView.qml) 结构（左菜单 + 主区 + 底状态栏），主区改为**左右分屏**：

* 左 `Image`：`source: viewModel.isRunning ? "image://videoframe/bird_avm?" + viewModel.frameCounter : ""`，`fillMode: PreserveAspectFit`，`cache: false`

* 右 `Image`：`source: ... "image://videoframe/bird_cam?" + viewModel.frameCounter`，同样

* 左菜单：自动录制 Switch + 间隔 SpinBox(5-60,默认10) + 帧数 SpinBox(100-1000,默认300) + 关闭按钮（仿 reverse）

* `Component.onCompleted`：`viewModel.loadStaticImages("/mnt/hgfs")` → `viewModel.setAutoRecordEnabled(true)` → `viewModel.start()`

* `onClosing`：`viewModel.stop()`、`viewModel.closeCamera()`、`settingsViewRef.activeButtonIndex = 5`（仿 reverse）

* 录制指示器（REC N/300）仿 reverse

### 4. [BirdRecordViewModel.h](file:///home/cccc/Project/360_driving_assistant/client/src/viewmodels/video/bird/BirdRecordViewModel.h) / [.cpp](file:///home/cccc/Project/360_driving_assistant/client/src/viewmodels/video/bird/BirdRecordViewModel.cpp)

现有转发已覆盖 `start/stop/loadStaticImages/isRunning/frameCounter/isAutoRecord*/targetFrames` 等，**基本不改**。无需新增属性。

### 不改动

* `main.cpp`（`birdRecordViewModel` 已注册，frameProvider 已挂）、`CMakeLists.txt`（文件已在列）、`qml.qrc`（BirdView\.qml 已注册）、`VideoFrameProvider`（多 ID 已支持）。

* 不新增 `CalibrationPanel.qml`（用户要求全新方案，不做点击标定）。

## 复用现有代码

* 帧推送模式：[ReverseRecordService.cpp:432-441](file:///home/cccc/Project/360_driving_assistant/client/src/services/video/reverse/ReverseRecordService.cpp#L432-L441)（`cvtColor BGR2RGB` → `QImage` → `updateFrame(id, copy)`）

* QML 帧消费模式：[ReverseModeView.qml:565-583](file:///home/cccc/Project/360_driving_assistant/client/src/views/features/reverse/ReverseModeView.qml#L565-L583)（`image://videoframe/<id>?frameCounter` + `cache:false`）

* 录制骨架：现有 `BirdRecordService::startAutoRecord/stopAutoRecord`（改尺寸即可）

* alpha 逐像素混合：[ReverseRecordService.cpp:363-388](file:///home/cccc/Project/360_driving_assistant/client/src/services/video/reverse/ReverseRecordService.cpp#L363-L388)（fushi.png 抠图后叠加可复用此套路）

## 实现顺序

1. `.h` 加成员/方法声明 + 鱼眼标定常量（K 原始值/D/640×424，系数 4.0）+ 默认源点常量
2. `.cpp` 实现 `initCalibration` → `undistortFisheye` → `orderPoints` → `warpDirection` → `featherMask` → `blendIntoCanvas` → `overlayCarIcon` → `buildBirdView`
3. 重写 `loadStaticImages`（加载4张+触发 build）、`processFrame`（推 bird\_avm/bird\_cam）、`composeRecordFrame`、`startAutoRecord` 尺寸
4. 重写 `BirdView.qml`（左右分屏 + 控件）
5. 编译 + 运行验证

## 验证方法

1. **编译**：`cd client && cmake --build .` 通过
2. **运行**：`LD_LIBRARY_PATH=<fdbus lib> ./client`，从设置进"俯视模式"
3. **鱼眼矫正**：先单独 `cv::imwrite` 保存一张矫正后的图检查——直线物体（标定布方格边缘）应变直，无黑边或黑边最小
4. **左侧鸟瞰图**：可见 1800×1200 鸟瞰画布，4 路拼接无明显接缝（羽化区过渡平滑），中心 fushi 车辆图标居中、白背景已抠除
5. **右侧**：显示 carfront.png
6. **录制**：进入后每 10 秒生成一个 `~/Videos/360DrivingAssistant/鸟瞰_<时间>.avi`，1280×720，左右拼接，约 300 帧
7. **关闭**：关窗后服务停止、设置页按钮复位

## 风险点

* K 等比例缩放系数 4.0（2560/640=1696/424）已保持 fx≈fy 方形像素；K/D 集中为常量可调，矫正效果异常时直接改

* 默认源点（全图内缩4角）不会精确对齐标定布，远处拼接会有错位——Homography Demo 固有局限，属预期

* `warpPerspective` 到全画布 4 次 + 羽化累加仅在 `buildBirdView` 一次性执行，`processFrame` 只 clone+推送，30fps 无压力

* fushi.png 阈值 200 若抠图不干净，可加形态学开运算去噪（后续微调）

* `getOptimalNewCameraMatrix(alpha=1)` 保留全 FOV 但可能留黑边；若要无黑边可用 alpha=0（裁剪）。先用 alpha=1，看效果再调

