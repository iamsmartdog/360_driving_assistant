# 客户端文件结构整理方案

## 背景
当前 `client/src/` 下的 `viewmodels/video/`、`services/video/`、`views/features/` 三个目录文件过多且平铺，缺乏按功能模块的进一步划分。需要按**功能模块**（驾驶、鸟瞰、倒车、播放、截图）划分子目录，使结构清晰、职责分明。

## 整理原则
- 按**功能模块**划分子目录：driving（驾驶模式）、bird（鸟瞰模式）、reverse（倒车模式）、playback（播放）、screenshot（截图）
- 共享/基础组件放入 `common/` 子目录
- 所有 `#include` 路径、CMakeLists.txt、qml.qrc 同步更新
- QML alias 路径保持不变（确保 QML 引用不受影响）

---

## 一、viewmodels/video/ 拆分

**当前**（6个ViewModel平铺）:
```
viewmodels/video/
├── PlaybackDetectViewModel.h/cpp
├── ReverseRecordViewModel.h/cpp
├── ScreenshotListViewModel.h/cpp
├── ScreenshotViewModel.h/cpp
├── VideoPlaybackViewModel.h/cpp
└── VideoRecordViewModel.h/cpp
```

**整理后**:
```
viewmodels/video/
├── driving/          → VideoRecordViewModel.h/cpp
├── reverse/          → ReverseRecordViewModel.h/cpp
├── playback/         → VideoPlaybackViewModel.h/cpp, PlaybackDetectViewModel.h/cpp
└── screenshot/       → ScreenshotViewModel.h/cpp, ScreenshotListViewModel.h/cpp
```

## 二、services/video/ 拆分

**当前**（6个Service平铺）:
```
services/video/
├── BirdRecordService.h/cpp
├── PlaybackDetectService.h/cpp
├── ReverseRecordService.h/cpp
├── VideoFrameProvider.h/cpp
├── VideoPlaybackService.h/cpp
└── VideoRecorderService.h/cpp
```

**整理后**:
```
services/video/
├── driving/          → VideoRecorderService.h/cpp
├── bird/             → BirdRecordService.h/cpp
├── reverse/          → ReverseRecordService.h/cpp
├── playback/         → VideoPlaybackService.h/cpp, PlaybackDetectService.h/cpp
└── common/           → VideoFrameProvider.h/cpp
```

## 三、views/features/ 拆分

**当前**（7个QML平铺）:
```
views/features/
├── BirdView.qml
├── DrivingModeView.qml
├── FeatureRecordView.qml
├── ReverseModeView.qml
├── SystemSettingView.qml
├── VideRecordView.qml
└── VideoPlaybackWindow.qml
```

**整理后**:
```
views/features/
├── driving/          → DrivingModeView.qml, VideRecordView.qml, FeatureRecordView.qml
├── bird/             → BirdView.qml
├── reverse/          → ReverseModeView.qml
├── playback/         → VideoPlaybackWindow.qml
└── system/           → SystemSettingView.qml
```

---

## 四、需要修改的文件

### 4.1 C++ #include 路径修改

**main.cpp** — 更新所有 video 相关的 include 路径：
```cpp
// 旧:
#include "VideoRecordViewModel.h"
#include "VideoRecorderService.h"
// 新:
#include "viewmodels/video/driving/VideoRecordViewModel.h"
#include "services/video/driving/VideoRecorderService.h"
// ... 以此类推
```

**viewmodels/video/ 内部交叉引用**:
- `PlaybackDetectViewModel.cpp` → 修改 `#include "viewmodels/video/ScreenshotViewModel.h"` 为 `#include "viewmodels/video/screenshot/ScreenshotViewModel.h"`
- `ScreenshotListViewModel.cpp` → 修改 `#include "FdbusClientService.h"` 为 `#include "services/network/FdbusClientService.h"`
- `VideoRecordViewModel.cpp` → 修改 `#include "FdbusClientService.h"` 为 `#include "services/network/FdbusClientService.h"`
- `ScreenshotViewModel.cpp` → 修改 `#include "services/network/FdbusClientService.h"` (路径已是正确的)

**services/video/ 内部交叉引用**:
- `VideoRecorderService.cpp` → `#include "VideoFrameProvider.h"` 改为 `#include "services/video/common/VideoFrameProvider.h"`
- `VideoPlaybackService.cpp` → `#include "VideoFrameProvider.h"` 改为 `#include "services/video/common/VideoFrameProvider.h"`

### 4.2 CMakeLists.txt 修改

更新 SOURCES、HEADERS 中的路径，以及 target_include_directories 中新增的子目录：
```
src/viewmodels/video/driving/
src/viewmodels/video/reverse/
src/viewmodels/video/playback/
src/viewmodels/video/screenshot/
src/services/video/driving/
src/services/video/bird/
src/services/video/reverse/
src/services/video/playback/
src/services/video/common/
```

### 4.3 qml.qrc 修改

更新 `<file>` 标签的物理路径，**保持 alias 不变**（QML 中用 alias 引用，无需改动 QML 代码）：
```xml
<file alias="views/DrivingModeView.qml">../src/views/features/driving/DrivingModeView.qml</file>
<file alias="views/BirdView.qml">../src/views/features/bird/BirdView.qml</file>
<!-- ... 以此类推 -->
```

---

## 五、执行步骤

1. **创建所有新子目录** (mkdir -p)
2. **移动文件到新目录** (git mv 保留历史)
3. **更新 main.cpp 中的 #include 路径**
4. **更新 viewmodels 内部 #include 路径**
5. **更新 services 内部 #include 路径**
6. **更新 CMakeLists.txt**
7. **更新 qml.qrc**
8. **编译验证** — 确保无编译错误

## 六、验证方式

```bash
cd /home/cccc/Project/360_driving_assistant/build
cmake .. && make -j$(nproc)
```

确认编译通过且无 #include 找不到的错误。
