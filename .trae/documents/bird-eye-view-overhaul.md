# 鸟瞰模式改造计划 -- 4路IPM无缝拼接 + 3D立体车辆

## 背景

当前鸟瞰模式存在两个核心问题：
1. `stitchBirdEyeView()` 完全没有调用 IPM 拼接逻辑，只是加载 caroverlook.png 或返回深色占位图
2. `initCarTopDownImage()` 生成的是平面2D灰色方框+圆圈，没有立体感

用户要求：4路摄像头画面通过 IPM 无缝融合成一张连续的俯视路面图，中间放置3D立体感的车辆俯视图。

## 修改文件

- `client/src/services/video/bird/BirdRecordService.cpp` — 主要改动
- `client/src/services/video/bird/BirdRecordService.h` — 添加成员和方法声明

**不需要修改**: BirdView.qml、VideoFrameProvider、computeIPMHomography()

## 实施步骤

### 步骤1: 添加预计算单应矩阵方法

**BirdRecordService.h**: 添加 `precomputeHomographies()` 声明

**BirdRecordService.cpp**: 实现 `precomputeHomographies()`
- 在图片加载后一次性计算4路单应矩阵，存入 `m_homographyFront/Rear/Left/Right`
- 在 `loadStaticImages()` 和 `loadSingleImage()` 末尾调用
- `warpToBirdEye()` 中优先使用预计算矩阵，避免每帧重复计算

### 步骤2: 重写 `stitchBirdEyeView()` — 核心拼接管线

替换当前第197-218行的简单实现，新流程：

1. 创建 `CV_32FC3` 浮点画布 + `CV_32FC1` 权重画布（800x800）
2. 对已加载的4路图片分别调用 `warpToBirdEye()` 做IPM投影 + 浮点累加
3. 权重归一化：`result = sum / weight`，权重为0的区域保持深色
4. 叠加3D车辆俯视图（复用 `drawCarHighlight()`）

### 步骤3: 修改 `warpToBirdEye()` — 扩大覆盖 + 浮点累加

关键改动：
- **扩大覆盖区域**：前/后覆盖整个画布宽度×半高，左/右覆盖半宽×全高，使4路在对角区域有重叠
- **改用浮点累加**：像素值乘以alpha后累加到 `CV_32FC3` 画布，权重累加到 `weightCanvas`
- **增大渐变宽度**：从20px增到60px，靠近车辆中心方向使用2倍渐变宽度
- **使用预计算矩阵**：优先使用 `m_homography*` 成员

### 步骤4: 重写 `initCarTopDownImage()` — 3D立体效果车辆

替换当前平面2D轮廓，用 OpenCV 生成带3D效果的车辆俯视图：

1. **车身底部阴影**：偏移5px的半透明黑色轮廓 + 高斯模糊，模拟地面投影
2. **车身渐变填充**：横向渐变（中心亮/两侧暗），模拟车顶圆弧光照
3. **玻璃高光条纹**：斜向亮条模拟天空反射
4. **边缘环境光遮蔽**：粗深色描边 + 细亮线描边
5. **轮胎3层椭圆**：外圈深黑(轮胎壁) + 中灰(轮毂盘面) + 亮灰偏上(中心反光)
6. **车灯光晕**：径向渐变发光效果

### 步骤5: 对角空白区域填充

4路IPM扩大后，画布四角仍可能有权重为0的空白区域：
- 用相邻有效像素迭代扩散填充
- 第一帧计算后缓存到成员变量 `m_cornerFillMask`/`m_cornerFillColor`，后续帧直接复用

**BirdRecordService.h**: 添加 `m_cornerFillMask`, `m_cornerFillColor`, `m_cornerFillReady` 成员

## 验证方式

1. 编译项目：在 `client/build/` 下执行 cmake + make
2. 运行客户端，打开鸟瞰模式
3. 验证点：
   - 4路图片是否融合成一张连续的俯视路面图（无明显接缝）
   - 中间车辆是否有立体感（渐变、阴影、高光）
   - 帧率是否稳定在 25-30fps
