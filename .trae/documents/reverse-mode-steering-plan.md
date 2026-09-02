# 倒车模式 - 动态转向辅助线实施计划

## Context

用户没有真实汽车，倒车模式需要改为：
1. 使用静态车尾照片作为背景（而非实时摄像头）
2. 辅助线根据转向信号动态弯曲（左转左弧、回正直线、右转右弧）
3. 转向信号通过 UI 按钮触发（左/右/回正三个按钮）
4. 辅助线风格为**真实倒车轨迹线**（两条弧形平行线模拟后轮轨迹）

## 修改文件

### 1. ReverseRecordService.h — 新增转向属性和静态图片支持

新增成员：
```cpp
// 转向控制
Q_PROPERTY(qreal steeringAngle READ steeringAngle WRITE setSteeringAngle NOTIFY steeringAngleChanged)
Q_PROPERTY(bool useStaticImage READ useStaticImage WRITE setUseStaticImage NOTIFY useStaticImageChanged)

qreal m_steeringAngle;      // -1.0(左) ~ 0(中) ~ 1.0(右)
bool m_useStaticImage;       // 是否使用静态图片代替摄像头
cv::Mat m_staticImage;       // 缓存的静态背景图

qreal steeringAngle() const;
void setSteeringAngle(qreal angle);
bool useStaticImage() const;
void setUseStaticImage(bool use);

Q_INVOKABLE void steerLeft();    // 左打方向盘
Q_INVOKABLE void steerRight();   // 右打方向盘
Q_INVOKABLE void steerCenter();  // 回正方向盘
Q_INVOKABLE bool loadStaticImage(const QString &path);  // 加载车尾照片
```

### 2. ReverseRecordService.cpp — 重写辅助线绘制逻辑

**当前 `drawAuxiliaryLines()`**：画静态梯形区域（绿/黄/红）

**改为**：
- `processFrame()`：当 `m_useStaticImage=true` 时，使用 `m_staticImage` 代替摄像头帧
- `drawAuxiliaryLines()`：根据 `m_steeringAngle` 绘制动态弯曲轨迹线
- 轨迹线算法：
  - 直线时（angle=0）：两条竖直平行线，间距约60px，从底部延伸到画面上方
  - 左转时（angle<0）：两条线向左弯曲，用二次贝塞尔曲线
  - 右转时（angle>0）：两条线向右弯曲，用二次贝塞尔曲线
  - 弯曲程度与 `steeringAngle` 成正比
  - 保留三色区域标注（1m/2m/3m距离标线）
  - 转向动画：`steerLeft()`/`steerRight()` 渐变到目标角度，`steerCenter()` 渐变回0

**轨迹线绘制细节**：
```
画面底部中心出发 → 两条平行线向远方延伸
左打方向盘(angle=-1.0)：
  左线控制点向左偏移，右线控制点向左偏移 → 整体左弧
右打方向盘(angle=1.0)：
  左线控制点向右偏移，右线控制点向右偏移 → 整体右弧
回正(angle=0)：
  两条竖直平行线

每条线用 cv::polylines 画20个点组成的曲线
曲线公式：x = centerX + offset + steeringAngle * curvature * (1 - t)
           y = height - t * (height * 0.7)
其中 t 从0到1，代表从近到远的进度
```

### 3. ReverseModeView.qml — 添加转向按钮

在左侧菜单添加：
```qml
// 转向控制
Text { text: "转向控制"; color: "#aaaaaa" }
RowLayout {
    Layout.fillWidth: true
    spacing: 4
    Button { text: "左转"; onClicked: reverseRecordService.steerLeft() }
    Button { text: "回正"; onClicked: reverseRecordService.steerCenter() }
    Button { text: "右转"; onClicked: reverseRecordService.steerRight() }
}
// 当前方向指示
Text { text: "方向：" + (reverseRecordService.steeringAngle < -0.1 ? "← 左" : reverseRecordService.steeringAngle > 0.1 ? "右 →" : "↑ 直") }
```

### 4. ReverseRecordViewModel — 透传转向属性

新增属性代理：
```cpp
Q_PROPERTY(qreal steeringAngle READ steeringAngle NOTIFY steeringAngleChanged)
Q_INVOKABLE void steerLeft();
Q_INVOKABLE void steerRight();
Q_INVOKABLE void steerCenter();
Q_INVOKABLE bool loadStaticImage(const QString &path);
```

## 车尾照片

用户稍后提供路径。在 `ReverseRecordService` 中实现 `loadStaticImage(path)`，加载后缓存到 `m_staticImage`。UI 中可添加"加载背景"按钮让用户选择图片。

## 实施步骤

1. 修改 `ReverseRecordService.h` — 添加转向和静态图片相关成员
2. 修改 `ReverseRecordService.cpp` — 重写 `drawAuxiliaryLines()` 和 `processFrame()`
3. 修改 `ReverseRecordViewModel.h/.cpp` — 透传转向属性
4. 修改 `ReverseModeView.qml` — 添加转向按钮和方向指示
5. 更新 `CMakeLists.txt`（如有新文件）
6. 编译测试

## 验证

1. 运行客户端 → 进入倒车模式
2. 点击左转按钮 → 辅助线应向左弯曲
3. 点击回正按钮 → 辅助线恢复直线
4. 点击右转按钮 → 辅助线应向右弯曲
5. 加载车尾照片 → 背景应显示该照片而非摄像头画面
