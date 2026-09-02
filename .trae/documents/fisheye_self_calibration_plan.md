# 鱼眼自标定实现方案（从标定布圆心反算 K/D）

## Context

当前 `BirdRecordService` 的鱼眼矫正使用硬编码的 K/D（640×424 分辨率标定，[L13-L26](file:///home/cccc/Project/360_driving_assistant/client/src/services/video/bird/BirdRecordService.cpp#L13-L26)），与实际 2560×1696 照片可能不匹配，导致多层车影和拼接错位。

目标：用户在标定 UI 上点击标定布的圆心（前后 2×2=4 个，左右 2×3=6 个），系统用 `cv::calibrateCamera` 从这些点反算 K/D，替换硬编码值。无需任何相机参数文件。

## 核心设计决策

1. **四路共享 K/D** — 假设四颗鱼眼镜头同型号（AVM 常见配置）。4+4+6+6=20 个圆心跨 4 个视角，提供 40 个方程约束 8(K/D)+4×6(外参)=32 未知数，超定可解
2. **标准针孔模型** `cv::calibrateCamera`（5 系数）— 与现有代码一致，`undistortFisheye()` 无需改动
3. **两阶段标定**：Phase 1 点圆心算 K/D → Phase 2 在矫正后图上点 4 个 IPM 点
4. **QSettings 持久化** — 标定结果保存到 `config.ini`，下次启动自动加载
5. **回退机制** — 未标定时使用硬编码 K/D（现有行为不变）

## 世界坐标（每方向局部坐标系，原点=靠近车的左侧圆心）

```
前后(2×2)：  近(0,0)  近(0.8,0)  →  远(0,0.8)  远(0.8,0.8)
左右(2×3)：  近(0,0)  近(0.8,0)  近(1.6,0)  →  远(0,0.8)  远(0.8,0.8)  远(1.6,0.8)

点击顺序：逐行从左到右，先近(靠车)后远
```

## 修改文件清单

### 1. BirdRecordService.h — 新增标定状态与方法

新增成员：
```cpp
// 标定状态
enum CalibPhase { NoCalib, FisheyePhase, IpmPhase };
CalibPhase m_calibPhase;
int m_calibDir;  // 当前标定方向 0-3

// 鱼眼标定点（每方向点击的圆心图像坐标）
std::vector<cv::Point2f> m_fisheyePts[4];

// IPM 标定点（每方向 4 点，存入 m_dirCfg[idx].srcPts）
// 已有 DirConfig.srcPts，复用

// 自标定完成的 K/D（区别于硬编码）
bool m_fisheyeSelfCalibDone;
```

新增 Q_INVOKABLE 方法：
```cpp
Q_INVOKABLE void startCalibration();           // 进入标定模式
Q_INVOKABLE void setCalibDirection(int dir);   // 切换标定方向
Q_INVOKABLE void addCalibPoint(qreal x, qreal y); // 鼠标点击
Q_INVOKABLE void removeLastCalibPoint();       // 删除上一个点
Q_INVOKABLE int  calibPointCount(int dir) const;   // 已点数
Q_INVOKABLE int  expectedCalibPoints(int dir) const; // 期望数(4或6)
Q_INVOKABLE bool computeFisheyeCalibration();  // 计算 K/D
Q_INVOKABLE void startIpmPhase();              // 进入 IPM 阶段
Q_INVOKABLE void finishCalibration();          // 保存+重建 AVM
Q_INVOKABLE void cancelCalibration();          // 取消标定
```

新增信号：
```cpp
void calibPhaseChanged();
void calibDirChanged();
void calibPointsChanged();
void calibPreviewReady();  // 预览图刷新
```

### 2. BirdRecordService.cpp — 核心实现

#### a) `addCalibPoint(x, y)` — 收集点击点
- 根据 `m_calibPhase` 判断存入 `m_fisheyePts[dir]` 还是 `m_dirCfg[dir].srcPts`
- 在原图上绘制已点击点（圆圈+编号），推送到 `VideoFrameProvider` 的 `"bird_calib"` 帧
- 鱼眼阶段：在**原图**上点击
- IPM 阶段：在**矫正后图**上点击

#### b) `computeFisheyeCalibration()` — 调用 cv::calibrateCamera
```cpp
bool BirdRecordService::computeFisheyeCalibration() {
    std::vector<std::vector<cv::Point3f>> allObj;
    std::vector<std::vector<cv::Point2f>> allImg;
    cv::Size imgSize(m_src[0].cols, m_src[0].rows);

    for (int d = 0; d < 4; d++) {
        int n = expectedCalibPoints(d);
        if ((int)m_fisheyePts[d].size() < n) continue;

        // 生成世界坐标
        int cols = (d == Front || d == Rear) ? 2 : 3;
        std::vector<cv::Point3f> obj;
        for (int r = 0; r < 2; r++)
            for (int c = 0; c < cols; c++)
                obj.push_back(cv::Point3f(c * 0.8f, r * 0.8f, 0.0f));

        allObj.push_back(obj);
        allImg.push_back(m_fisheyePts[d]);
    }
    if (allImg.empty()) return false;

    cv::Mat K = cv::Mat::eye(3, 3, CV_64F);
    cv::Mat D;
    std::vector<cv::Mat> rvecs, tvecs;

    // 初始 K 猜测：中心 + 估焦距
    K.at<double>(0,0) = K.at<double>(1,1) = imgSize.width * 0.8;
    K.at<double>(0,2) = imgSize.width / 2.0;
    K.at<double>(1,2) = imgSize.height / 2.0;

    double rms = cv::calibrateCamera(allObj, allImg, imgSize,
                                      K, D, rvecs, tvecs, 0);
    qDebug() << "鱼眼自标定 RMS =" << rms;

    m_K = K; m_D = D;
    m_fisheyeSelfCalibDone = true;

    // 重新生成矫正映射
    cv::Mat newK = cv::getOptimalNewCameraMatrix(K, D, imgSize, 0, imgSize);
    cv::initUndistortRectifyMap(K, D, cv::Mat(), newK, imgSize,
                                 CV_16SC2, m_undistMap1, m_undistMap2);
    m_calibReady = true;
    return true;
}
```

#### c) `startIpmPhase()` — 切换到 IPM 阶段
- 对每个方向，将矫正后图像推送到 `"bird_calib"` 供用户点击 4 点
- IPM 点存入 `m_dirCfg[dir].srcPts`

#### d) `finishCalibration()` — 保存并重建
- 调用 `saveCalibration()` 写 QSettings
- 调用 `buildBirdView()` 重建 AVM
- 退出标定模式

#### e) `saveCalibration()` / `loadCalibration()` — QSettings 持久化
```cpp
// 保存（config.ini，[BirdCalib] 组）
QSettings settings(configPath, QSettings::IniFormat);
settings.beginGroup("BirdCalib");
settings.setValue("version", 2);  // schema 版本
settings.setValue("fisheyeDone", m_fisheyeSelfCalibDone);
// K/D（9+5 个 double）
for (int i = 0; i < 9; i++)
    settings.setValue("K"+QString::number(i), m_K.at<double>(i));
for (int i = 0; i < 5; i++)  // 或 D.rows*D.cols
    settings.setValue("D"+QString::number(i), m_D.at<double>(i));
// 每方向 IPM 4 点 + flip
for (int d = 0; d < 4; d++) {
    for (int p = 0; p < 4; p++) {
        settings.setValue(QString("dir%1_%2").arg(d).arg(p),
            QString("%1,%2").arg(m_dirCfg[d].srcPts[p].x).arg(m_dirCfg[d].srcPts[p].y));
    }
}
settings.endGroup();
```

#### f) `initCalibration()` 修改
- 先调 `loadCalibration()`，若已自标定则用加载的 K/D
- 否则用硬编码 K/D（现有逻辑不变）

### 3. BirdRecordViewModel.h / .cpp — 转发标定方法

在 ViewModel 中添加对应的 Q_INVOKABLE 转发方法和属性：
```cpp
// 新增属性
Q_PROPERTY(int calibPhase READ calibPhase NOTIFY calibPhaseChanged)
Q_PROPERTY(int calibDir READ calibDir NOTIFY calibDirChanged)

// 转发方法（同 Service 签名）
Q_INVOKABLE void startCalibration();
Q_INVOKABLE void setCalibDirection(int dir);
Q_INVOKABLE void addCalibPoint(qreal x, qreal y);
Q_INVOKABLE void removeLastCalibPoint();
Q_INVOKABLE int calibPointCount(int dir) const;
Q_INVOKABLE int expectedCalibPoints(int dir) const;
Q_INVOKABLE bool computeFisheyeCalibration();
Q_INVOKABLE void startIpmPhase();
Q_INVOKABLE void finishCalibration();
Q_INVOKABLE void cancelCalibration();
```

连接 Service 的新信号到 ViewModel 信号。

### 4. BirdView.qml — 标定 UI

#### 布局变化
- 左侧面板新增"鱼眼标定"按钮
- 点击后右侧区域切换为标定模式（显示原图/矫正图 + MouseArea）
- 标定模式下隐藏录制控件，显示标定进度

#### 标定模式 UI
```qml
// 标定模式覆盖层
Rectangle {
    visible: viewModel.calibPhase > 0
    anchors.fill: parent

    // 方向选择器
    Row {
        Repeater {
            model: ["前", "后", "左", "右"]
            Button { text: modelData; onClicked: viewModel.setCalibDirection(index) }
        }
    }

    // 标定图像 + 点击区
    Image {
        id: calibImg
        source: "image://videoframe/bird_calib?" + viewModel.frameCounter
        fillMode: Image.PreserveAspectFit

        MouseArea {
            anchors.fill: parent
            onClicked: {
                // 坐标转换：QML坐标 → 图像坐标
                var s = Math.min(calibImg.width / calibImg.sourceSize.width,
                                 calibImg.height / calibImg.sourceSize.height);
                var pw = calibImg.sourceSize.width * s;
                var ph = calibImg.sourceSize.height * s;
                var ox = (calibImg.width - pw) / 2;
                var oy = (calibImg.height - ph) / 2;
                var imgX = (mouseX - ox) / s;
                var imgY = (mouseY - oy) / s;
                viewModel.addCalibPoint(imgX, imgY);
            }
        }
    }

    // 进度提示
    Text {
        text: viewModel.calibPhase === 1
              ? "点击圆心 " + viewModel.calibPointCount(viewModel.calibDir) + "/" + viewModel.expectedCalibPoints(viewModel.calibDir)
              : "点击 IPM 4 点 " + viewModel.calibPointCount(viewModel.calibDir) + "/4"
    }

    // 操作按钮：撤销 / 计算鱼眼 / 下一步 / 完成 / 取消
}
```

## 标定流程（用户视角）

```
1. 点击"鱼眼标定"按钮
2. 选择方向"前" → 看到前向原图
3. 按顺序点击 4 个圆心（靠近车→远离车，左→右）
   └ 可撤销上一个点
4. 切换"后"→点 4 个圆心 → 切换"左"→点 6 个圆心 → 切换"右"→点 6 个圆心
5. 点击"计算鱼眼标定" → 看到矫正后预览（弯线变直）
6. 点击"进入 IPM 标定" → 对每个方向在矫正图上点 4 个 IPM 点
7. 点击"完成" → AVM 鸟瞰图自动重建
8. 下次启动自动从 config.ini 加载，无需重新标定
```

## 验证方式

1. 编译：`cd client/build && cmake .. && make -j$(nproc)`
2. 运行客户端，进入鸟瞰模式，点击"鱼眼标定"
3. 按流程点击 4 个方向的圆心
4. 点击"计算鱼眼标定"，检查 `/tmp/bird_undist_front.png` — 标定布上的方格线应变直
5. 完成 IPM 标定后，检查 `/tmp/bird_avm_final.png` — 四路拼接无明显错位
6. 重启客户端，确认从 config.ini 加载标定数据，无需重新标定
7. 对比自标定前后的 AVM 图：多层车影应减轻或消失
