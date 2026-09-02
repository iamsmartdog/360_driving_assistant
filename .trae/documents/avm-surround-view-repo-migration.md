# AVM 鸟瞰：改用 neozhaoliang/surround-view 仓库数据重写管线

## Context（为什么做这个改动）

当前 `BirdRecordService` 的鱼眼矫正 + 手动 4 点 IPM 管线长期无法让用户满意：用户滑动"矫正力度"滑块感觉曲线拉不直。经排查确认：

* **算法本身没错**：当前代码已用 `cv::fisheye::undistortImage`（K/D）+ 4 点 `getPerspectiveTransform` + `warpPerspective`，与参考仓库 `neozhaoliang/surround-view-system-introduction` 的方法**完全相同**。用户看到的"拉不直的曲线"是**透视畸变**（地面俯角），鱼眼矫正本就不修它，只有 IPM 修。

* **真正缺的是已知正确的标定数据**。参考仓库提供了一套完整、已标定好的数据（4 张鱼眼照片 + 4 个 yaml 含 K/D/scale\_xy/shift\_xy/预计算 project\_matrix + weights.png/masks.png + car.png）。

* 我已用 Python 把参考仓库的完整流程跑通（`/tmp/sv_repo/verify_stitch.py`），输出 `/tmp/sv_final_birdview.png` 经浏览器确认是**正确的 1600×1200 环视图**（车居中、四方向布局正确、标定布大致矩形）。

**用户决定**：

1. 彻底删除手动标定 UI（FisheyePhase/IpmPhase/PreviewPhase、滑块、4 点点击、标定按钮）——直接用 yaml 里预计算好的 `project_matrix`。
2. 只留鸟瞰图显示——删除录制功能（video writer/auto-record timer/frame counter/录制 UI）和左右分屏（bird\_cam）。
3. 用仓库的照片 + 摄像头参数。

**预期结果**：启动鸟瞰模式即显示一张正确的 360 环视图，无需任何标定操作。

***

## 参考数据（已下载到 /tmp/sv\_repo/，将拷入项目）

仓库流程（Python，需移植到 C++）：

* `fisheye_camera.py FisheyeCameraModel`：用 `cv::FileStorage` 读 yaml（K 3×3 double, D 4×1 double, resolution\[W,H], project\_matrix P 3×3 double, scale\_xy\[sx,sy] float, shift\_xy\[dx,dy] float）。建图：`newK=K`，`fx*=sx, fy*=sy, cx+=dx, cy+=dy`；`cv::fisheye::initUndistortRectifyMap(K,D,eye(3),newK,(W,H),CV_16SC2)`。undistort=`remap`。project=`warpPerspective(img,P,project_shape)`。

* `param_settings.py`：shift\_w=300, shift\_h=300, total\_w=1200, total\_h=1600, xl=500, xr=700, yt=550, yb=1050。project\_shapes: front/back=(1200,550), left/right=(1600,500)。car resize 到 (xr-xl, yb-yt)=(200,500)。

* 翻转（关键）：front=恒等；back=`cv::flip(-1)`(180°)；left=`transpose` 后 `flip(0)`(行翻转)；right=`flip(transpose,1)`。

* `birdview.py stitch_all_parts`：canvas `Mat(1600,1200)`。直接贴 F=FM(front\[:,500:700])→\[0:550,500:700]，B/L/R 同理；4 个角 FL/FR/BL/BR = `merge(前相机边, 侧相机边, weights[k])`，`merge(A,B,w)=A*w+B*(1-w)`。

* `make_luminance_balance` + `make_white_balance`：用 masks 做亮度均衡 + 全局白平衡，消除拼接缝。

**已验证基线**：`/tmp/sv_final_birdview.png`（Python 跑通的结果，C++ Phase1 输出须与此逐像素一致）。

***

## 实施方案

### 阶段 0：拷贝资源（无代码）

新建 `client/resources/surround_view/`，从 `/tmp/sv_repo/` 拷入：

```
images/{front,back,left,right,car}.png   (960×640 鱼眼 + 车图标)
yaml/{front,back,left,right}.yaml
weights.png   (4通道 RGBA 融合权重 FL/FR/BL/BR)
masks.png      (4通道 亮度均衡掩膜)
```

**不入 qrc，不进 SOURCES**——按现有 `DetectionEngine` 加载 `yolov8n.onnx` 的多路径回退模式从文件系统读（`applicationDirPath()/../resources/surround_view/...` → `.../../../client/resources/...` → 绝对开发路径）。`cv::imread` 和 `cv::FileStorage` 原生支持文件系统路径，避开 qrc 不兼容问题。

### 阶段 1：可工作的鸟瞰图（几何+权重+车，不含亮度/白平衡）

**重写** **[BirdRecordService.h](file:///home/cccc/Project/360_driving_assistant/client/src/services/video/bird/BirdRecordService.h)** **/** **[.cpp](file:///home/cccc/Project/360_driving_assistant/client/src/services/video/bird/BirdRecordService.cpp)**
删除全部标定/录制/IPM/鱼眼自标定代码。新结构：

* `struct FisheyeCameraModel { K, D, P, resolution, scale_x/y, shift_x/y, map1, map2, projectShape, name, projected; buildMaps(); undistort(); project(); flip(); }`

* 类成员：`m_cams[4]`、`m_weights[4]`(3通道float)、`m_masks[4]`(1通道float)、`m_carIcon`、`m_birdAvm`(1600×1200)、`m_frameProvider`、`m_buildWatcher`(QFutureWatcher)。

* 方法：`resolveAsset()`、`loadCameraModel()`、`loadWeightsAndMasks()`、`loadCarIcon()`(仅resize，**不要**移植旧的 fushi.png alpha 抠图)、`buildProjectedImages()`、`stitchAllParts()`、`pasteCarIcon()`、`pushFrame()`、`start()`/`stop()`/`onBuildFinished()`。

* 几何常量与区域 Rect 辅助函数（regionF/B/L/R/FL/FR/BL/BR/C）和子图提取器（FI/FII/FM/BI/BIV/BM/LI/LIII/LM/RII/RIV/RM）按参考移植。

**两个高危易错点（务必注释标注）**：

1. **weights.png/masks.png 通道顺序**：OpenCV `IMREAD_UNCHANGED` 读 4 通道 PNG 为 **BGRA**，而参考用 PIL 存为 **RGBA**（ch0=R=FL…）。必须 `cvtColor(BGRA2RGBA)` 再 `split`，否则 4 个角融合权重错位。
2. **yaml 数据类型**：`scale_xy`/`shift_xy` 是 `dt:f`(CV\_32F)，`K`/`P` 是 `dt:d`(CV\_64F)。读 scale/shift 用 `Mat; fs[...]>>mat; mat.at<float>(0,0)`，**不可用** **`.at<double>`**，否则读到垃圾值。

**线程（MVVM 合规）**：`start()` 用 `QtConcurrent::run` 在线程池跑 `loadDataAndBuild()`（只填成员数据、返回 bool，**不碰 provider、不 emit**）；`QFutureWatcher::finished` → `onBuildFinished()` 在主线程 push 帧 + emit。需 `#include <QtConcurrent>`/`<QFutureWatcher>` 并链接 `Qt5::Concurrent`。

**调试**：`onBuildFinished` 里临时 `cv::imwrite("/tmp/bird_avm_debug.png", m_birdAvm)`，与 `/tmp/sv_final_birdview.png` 逐像素比对，必须一致。

**重写** **[BirdRecordViewModel.h](file:///home/cccc/Project/360_driving_assistant/client/src/viewmodels/video/bird/BirdRecordViewModel.h)** **/** **[.cpp](file:///home/cccc/Project/360_driving_assistant/client/src/viewmodels/video/bird/BirdRecordViewModel.cpp)**

* 删除 Q\_PROPERTY：`isAutoRecording, isAutoRecordEnabled, autoRecordIntervalSec, targetFrames, currentFrameCount, videoDir, calibPhase, calibDir, undistScale, viewScale`。

* 删除 Q\_INVOKABLE：`openCamera, closeCamera, loadStaticImages, loadSingleImage, startCalibration, setCalibDirection, addCalibPoint, removeLastCalibPoint, calibPointCount, expectedCalibPoints, computeFisheyeCalibration, startIpmPhase, finishCalibration, cancelCalibration`。

* 保留：`isRunning, imagesLoaded, frameCounter, useStaticImages` 属性 + `start()/stop()`。

* 构造函数：仅保留 running/imagesLoaded/frameCounter/useStaticImages 的 connect。

**重写** **[BirdView.qml](file:///home/cccc/Project/360_driving_assistant/client/src/views/features/bird/BirdView.qml)**
从 \~600 行精简到 \~60 行：根 Window（竖屏 900×1200 适配 1200×1600 画布），单个 `Image` 填充 `image://videoframe/bird_avm?+frameCounter`（cache:false），`Component.onCompleted: viewModel.start()`，`onClosing: viewModel.stop()`。删除全部左菜单/分屏/标定/录制 UI。

**改** **[CMakeLists.txt](file:///home/cccc/Project/360_driving_assistant/client/CMakeLists.txt)**

* `find_package(Qt5 ... COMPONENTS)` 加 `Concurrent`（L17-22）。

* `target_link_libraries` 加 `Qt5::Concurrent`（L164-177）。

* 无需改 SOURCES/qml.qrc（源文件已在列表，BirdView\.qml 别名已存在）。

### 阶段 2：拼接质量打磨（阶段1验证通过后）

* 移植 `makeLuminanceBalance()`（用 `m_masks` + `mean_luminance_ratio` + `adjust_luminance`），在 `buildProjectedImages` 后、`stitchAllParts` 前调用。

* 移植 `makeWhiteBalance(canvas)`（按通道均值缩放），在 `stitchAllParts` 后、`pasteCarIcon` 前调用。

* 重新比对输出，确认接缝改善且无亮度回退。

### 阶段 3：清理

* 删除临时 `/tmp` 调试写入。

* 全局 grep 残留引用（`undistScale, viewScale, calibPhase, isAutoRecording, targetFrames, loadStaticImages` 等）确保无遗漏。`main.cpp` L57 注册不变。

***

## 涉及文件

| 文件                                                             | 动作                                                       |
| -------------------------------------------------------------- | -------------------------------------------------------- |
| `client/src/services/video/bird/BirdRecordService.{h,cpp}`     | 重写（删标定/录制/IPM，加 FisheyeCameraModel + 新管线 + QtConcurrent） |
| `client/src/viewmodels/video/bird/BirdRecordViewModel.{h,cpp}` | 删 \~10 属性 + \~14 方法，保留 start/stop/flags                  |
| `client/src/views/features/bird/BirdView.qml`                  | 重写为单 Image \~60 行                                        |
| `client/CMakeLists.txt`                                        | 加 Qt5::Concurrent                                        |
| `client/resources/surround_view/**`                            | 新建（拷贝资源，不入 qrc）                                          |
| `client/resources/qml.qrc`                                     | 不变                                                       |
| `client/src/main.cpp`                                          | 不变（L57 注册照旧）                                             |

***

## 验证

1. **构建**：`cd client/build && cmake .. && make -j` 通过（重点看 Qt5::Concurrent 链接、FileStorage 编译）。
2. **逐像素比对**：C++ 输出 `/tmp/bird_avm_debug.png` 与 Python 基线 `/tmp/sv_final_birdview.png` 一致（阶段1）。
3. **运行**：`LD_LIBRARY_PATH` 设好后跑 `client/build/bin/client`，进鸟瞰模式，立即看到正确的 360 环视图（车居中、四方向拼接、标定布矩形）。
4. **回归**：确认 BirdView\.qml 不再引用任何已删属性；其他模式（行车/倒车/播放）不受影响。
5. 阶段2后：接缝处的亮度过渡平滑，无可见拼接线。

