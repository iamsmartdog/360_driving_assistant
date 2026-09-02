# 360 度智能行车辅助系统

一个基于 C++/Qt 的智能行车辅助系统，采用客户端、服务端和独立监控守护进程组成的架构，提供驾驶录像、倒车辅助、鸟瞰视图、视频回放、目标检测、用户认证及系统监控等功能。

## 项目组成

### client：Qt 客户端

客户端使用 Qt 5、QML 和 C++17 开发，采用 ViewModel 与 Service 分层结构，主要功能包括：

- 登录与注册、会话管理
- 主界面和系统设置
- 驾驶录像与视频保存
- 倒车录像及倒车辅助界面
- 鸟瞰视图记录
- 视频回放与回放检测
- 截图列表和截图管理
- 车辆检测、交通灯检测
- FDBus 网络通信
- ONNX/OpenCV 视觉模型推理

主要目录：

```text
client/
├── src/
│   ├── models/                 # 客户端数据模型
│   ├── services/               # 业务服务
│   │   ├── auth/               # 认证与会话
│   │   ├── config/             # 配置管理
│   │   ├── network/            # FDBus 网络通信
│   │   └── video/              # 驾驶、倒车、鸟瞰、回放和检测
│   ├── viewmodels/             # ViewModel 层
│   │   ├── auth/
│   │   ├── network/
│   │   ├── settings/
│   │   ├── system/
│   │   └── video/
│   └── views/                  # QML 界面
│       ├── auth/
│       ├── features/
│       └── main/
├── resources/                  # QML、模型、车辆环视资源
└── CMakeLists.txt
```

### server：服务端

服务端使用 C++17 开发，通过 FDBus 与客户端通信，使用 Protobuf 定义消息格式，并通过 MySQL 持久化用户、视频和截图数据。

```text
server/
├── src/
│   ├── services/
│   │   ├── server/             # 服务端启动与通信
│   │   └── auth/               # 认证业务
│   ├── config/                 # 服务端配置
│   ├── dao/
│   │   ├── common/             # MySQL 通用访问
│   │   ├── user/               # 用户数据访问
│   │   ├── video/              # 视频数据访问
│   │   └── screenshot/         # 截图数据访问
│   ├── models/                 # 服务端数据模型
│   └── utils/                  # 日志、密码处理等工具
├── config/server.ini           # 服务端配置
└── CMakeLists.txt
```

### monitor：监控守护进程

独立的系统监控中间件，用于监控指定进程状态、检测异常并发送告警。安装 libcurl 开发库后可启用 LLM 辅助诊断功能。

```text
monitor/
├── src/
│   ├── core/
│   │   ├── ProcMonitor       # 进程监控
│   │   ├── AnomalyDetector   # 异常检测
│   │   ├── AlertNotifier     # 告警通知
│   │   └── LLMDiagnoser      # 可选的 LLM 诊断
│   ├── config/               # 监控配置
│   └── main.cpp
├── monitor.ini
└── CMakeLists.txt
```

### common：共享协议

```text
common/proto/
├── driving_assistant.proto       # Protobuf 协议定义
├── MessageIds.h                  # 消息 ID
└── generated/                    # Protobuf 生成的 C++ 文件
```

## 技术栈

- C++17
- CMake 3.15+
- Qt5：Quick、QuickControls2、Network、Widgets、Concurrent
- QML
- OpenCV
- ONNX Runtime 模型文件
- Protobuf
- FDBus
- MySQL/MariaDB 客户端库
- OpenSSL
- pthread

## 依赖环境

以 Ubuntu/Debian 为例，需要准备：

- C++ 编译器和 CMake
- Qt5 开发组件
- Protobuf 开发库
- OpenCV 开发库
- MySQL 或 MariaDB 客户端开发库
- OpenSSL 开发库
- FDBus（放置或构建在 `third_party/fdbus`）
- 可选：`libcurl4-openssl-dev`，用于启用监控进程的 LLM 诊断

## 构建

项目顶层 CMake 会同时构建客户端、服务端和监控守护进程：

```bash
cmake -S . -B build
cmake --build build -j$(nproc)
```

构建产物统一输出到：

```text
build/bin/
├── client
├── server
├── monitor_daemon
├── config.ini
├── server.ini
└── lib/
    └── libfdbus.so
```

也可以分别构建子项目：

```bash
cmake -S client -B client/build
cmake --build client/build -j$(nproc)

cmake -S server -B server/build
cmake --build server/build -j$(nproc)

cmake -S monitor -B monitor/build
cmake --build monitor/build -j$(nproc)
```

服务端支持通过 CMake 工具链进行 aarch64 交叉编译，交叉编译时需要准备对应 sysroot、FDBus、Protobuf 和 MySQL 依赖。

## 运行

运行前请确认配置文件中的通信地址、端口、数据库连接信息及监控参数正确。客户端和服务端的动态库会在构建后复制到对应 `bin/lib` 目录，并使用 `$ORIGIN/lib` 查找运行时库。

```bash
./build/bin/server
./build/bin/client
./build/bin/monitor_daemon
```

通常建议先启动服务端，再启动客户端；监控守护进程可独立运行。

## 目录说明

- `config/`：项目配置文件
- `third_party/fdbus/`：FDBus 第三方依赖
- `build*/`：本地构建目录，不属于源代码
- `client/resources/models/`：车辆、交通规则等视觉模型
- `client/resources/surround_view/`：环视图片、标定 YAML 及相关资源

## 注意事项

- 客户端编译必须能找到 Qt5、OpenCV、Protobuf 和 FDBus。
- 服务端需要 MySQL/MariaDB 客户端开发库；未找到时会提示并可能关闭 MySQL 支持。
- `monitor` 的 LLM 诊断功能依赖 libcurl，未安装时监控守护进程仍可正常构建。
- 不要将数据库密码、运行日志和本地构建产物提交到版本库。
