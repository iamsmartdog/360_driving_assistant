# IPM鸟瞰图重构方案

## Context

当前 `BirdRecordService::computeIPMHomography()` 使用硬编码的梯形参数（地平线30%、收窄比例等）来估算单应矩阵，与实际摄像头图像不匹配，导致鸟瞰图拼接效果差。

用户更换了新的4路摄像头图片（同一视角/背景/高度，2304×1728，位于 `/mnt/hgfs/`），需要用\*\*逆透视变换(IPM)\*\*正确生成车辆俯视图。

核心改造：**让用户在每张图上交互式选择4个源点**，用 `cv::getPerspectiveTransform` 计算精确单应矩阵，替代硬编码参数。

***

## 实现步骤

### Phase 1: Service层 - 替换硬编码IPM为精确4点标定

**文件:** **`client/src/services/video/bird/BirdRecordService.h`**

* 新增成员：`std::map<std::string, std::vector<cv::Point2f>> m_userSrcPts` — 每个方向的用户标定4点

* 新增成员：`bool m_calibrationMode` — 标定模式标志

* 新增Q\_INVOKABLE方法：

  * `setCalibrationPoints(position, pts)` — 设置某方向的4个源点

  * `getCalibrationPoints(position)` — 获取某方向的4个源点

  * `setCalibrationMode(bool)` — 进入/退出标定模式

  * `saveCalibration()` — 保存标定到INI文件

  * `loadCalibration()` — 从INI文件加载标定

  * `resetCalibration(position)` — 重置为默认值

  * `getOriginalImage(position)` — 返回原始图片QImage供标定UI显示

* 新增私有方法：`computeIPMHomographyFromPoints(srcPts, dstSize)` — 用 `cv::getPerspectiveTransform` 从4点算单应矩阵

* 新增信号：`calibrationModeChanged()`, `calibrationUpdated()`

**文件:** **`client/src/services/video/bird/BirdRecordService.cpp`**

* **改造** **`computeIPMHomography()`**（L292-343）：先查 `m_userSrcPts`，有用户标定点则用之；否则用当前硬编码逻辑生成默认点，再调 `computeIPMHomographyFromPoints()`

* **新增** **`computeIPMHomographyFromPoints()`**：

  ```cpp
  // 目标点：矩形（与当前逻辑相同）
  dstPts = { (0,0), (dw-1,0), (dw-1,dh-1), (0,dh-1) };
  // 关键：用 getPerspectiveTransform 而非 findHomography
  H = cv::getPerspectiveTransform(srcPts, dstPts);
  ```

* **新增标定方法实现**：setCalibrationPoints 调用后自动 precomputeHomographies + 清除cornerFill缓存

* **新增** **`drawCalibrationOverlay()`**：标定模式下在鸟瞰画布上叠加显示当前选中方向的梯形框和4点标记

* **修改** **`processFrame()`**：标定模式下额外绘制 overlay

* **新增 save/load**：用 QSettings 存INI文件到 `~/.config/360DrivingAssistant/ipm_calibration.ini`，格式：

  ```ini
  [IPM_Calibration]
  front_src=460.0,518.0,1844.0,518.0,2252.0,1728.0,52.0,1728.0
  rear_src=...
  left_src=...
  right_src=...
  ```

### Phase 2: ViewModel层 - 新增BirdRecordViewModel

**新文件:** **`client/src/viewmodels/video/bird/BirdRecordViewModel.h/.cpp`**

* 遵循 `ReverseRecordViewModel` 的模式：持有 Service 实例，代理属性和信号

* 转发已有属性：isAutoRecording, isRunning, imagesLoaded, useStaticImages, frameCounter 等

* 转发已有方法：start(), stop(), loadStaticImages(), loadSingleImage()

* 新增标定相关属性：

  * `calibrationMode` — 标定模式开关

  * `selectedCamera` — 当前选中的摄像头方向（"front"/"rear"/"left"/"right"）

  * `currentCalibrationPts` — 当前选中方向的4个标定点

* 新增标定方法：

  * `updateCalibrationPoint(position, pointIndex, x, y)` — 单点更新

  * `saveCalibration()`, `loadCalibration()`, `resetCalibration(position)`

**文件:** **`client/src/main.cpp`**

* 替换 `birdRecordService` 注册为 `birdRecordViewModel`

* 保留 `birdRecordService` 兼容性注册（内部使用）

### Phase 3: QML层 - 标定UI

**新文件:** **`client/src/views/features/bird/CalibrationPanel.qml`**

* 标定面板组件，包含：

  1. 摄像头选择器（ComboBox: 前/后/左/右）
  2. 原始图片显示区（带4个可拖拽圆点叠加）
  3. 梯形框连线（Canvas绘制4点连线）
  4. 每个点的坐标输入框（SpinBox，精确微调）
  5. 操作按钮：重置默认、保存标定

* 拖拽点实现：MouseArea + drag.target，拖动时将QML坐标换算回原始图片像素坐标

* 坐标换算：`imageX = pointX / paintedWidth * srcWidth`

**文件:** **`client/src/views/features/bird/BirdView.qml`**

* 替换 `birdRecordService` 引用为 `birdRecordViewModel`

* 左侧菜单新增：

  * "IPM标定"分区

  * "进入标定"/"退出标定"按钮

  * "保存标定"按钮（标定模式下可见）

* 主内容区：标定模式下右侧加载 CalibrationPanel 覆盖层

* Component.onCompleted 先 loadCalibration 再 loadStaticImages

**文件:** **`client/resources/qml.qrc`**

* 新增 CalibrationPanel.qml

### Phase 4: 构建与资源

**文件:** **`client/CMakeLists.txt`**

* 新增源文件：BirdRecordViewModel.cpp

* 新增头文件：BirdRecordViewModel.h

***

## 关键技术点

1. **`cv::getPerspectiveTransform`** **vs** **`cv::findHomography`**：4点精确对应用前者，更简单精确；后者用于多点RANSAC
2. **坐标换算**：QML显示图片是缩放后的，拖拽坐标需 ×(srcWidth/paintedWidth) 还原到原始像素坐标
3. **标定数据流**：QML拖拽 → ViewModel.updatePoint() → Service.setCalibrationPoints() → precomputeHomographies() → 下一帧processFrame()自动用新矩阵
4. **默认值兼容**：没有标定文件时，用现有硬编码逻辑生成默认4点，保持行为一致

## 验证方式

1. 不做任何标定时，鸟瞰图效果应与当前完全相同（默认值 = 硬编码参数）
2. 进入标定模式后，拖拽4点应实时更新鸟瞰预览
3. 保存标定后重启应用，应自动加载标定参数
4. 重置标定后应恢复默认效果
5. 用 `/mnt/hgfs/` 的新图片测试完整流程

