# 360度智能行车辅助系统

## 项目概述

本项目是一个基于客户端-服务器架构的360度智能行车辅助系统，客户端使用Qt框架实现可视化界面，服务端使用C++实现核心业务逻辑。

## 项目结构

```
360_driving_assistant/
├── client/                    # 客户端（Qt）
│   ├── src/
│   │   ├── views/            # 视图层（UI界面）
│   │   ├── viewmodels/       # 视图模型层（MVVM）
│   │   ├── models/           # 数据模型层
│   │   ├── services/         # 服务层（网络通信）
│   │   └── utils/            # 工具类
│   ├── resources/            # 资源文件（图片、QSS等）
│   ├── config/               # 客户端配置文件
│   └── CMakeLists.txt        # 客户端构建文件
│
├── server/                    # 服务端（C++）
│   ├── src/
│   │   ├── network/          # 网络通信模块
│   │   ├── protocol/         # 协议解析
│   │   ├── business/         # 业务逻辑层
│   │   ├── dao/              # 数据访问层
│   │   └── utils/            # 工具类
│   ├── config/               # 服务端配置文件
│   ├── log/                  # 运行时日志（.gitignore）
│   ├── db/                   # 数据库文件（.gitignore）
│   └── Makefile              # 服务端构建文件
│
├── common/                    # 客户端服务端共享代码
│   ├── protocol_def.h        # 协议定义
│   └── error_code.h          # 错误码定义
│
├── test/                      # 测试代码
│   ├── client_test/          # 客户端测试
│   ├── server_test/          # 服务端测试
│   └── stress_test/          # 压力测试
│
├── docs/                      # 文档
│   ├── 需求文档.pdf
│   ├── 协议设计.md
│   ├── 数据库设计.md
│   └── 开发日志.md
│
├── scripts/                   # 脚本工具
│   ├── build.sh              # 编译脚本
│   ├── run_server.sh         # 启动服务端
│   ├── run_client.sh         # 启动客户端
│   └── init_db.sql           # 数据库初始化
│
├── third_party/               # 第三方库
│   └── README.md
│
├── .gitignore                 # Git忽略文件
├── README.md                  # 项目说明
└── CMakeLists.txt             # 顶层构建文件

```

## 技术栈

### 客户端
- Qt 5/6 (Widgets + MVVM架构)
- C++17
- CMake

### 服务端
- C++17
- Linux系统编程
- 网络编程
- MySQL数据库

## 构建项目

### 客户端
```bash
cd client
mkdir build && cd build
cmake ..
make
```

### 服务端
```bash
cd server
make
```

### 一键构建
```bash
./scripts/build.sh
```

## 开发进度

- [x] 项目架构搭建
- [ ] 协议设计
- [ ] 数据库设计
- [ ] 客户端UI开发
- [ ] 服务端核心功能开发
- [ ] 网络通信实现
- [ ] 测试编写

## 注意事项

- `server/log/` 和 `server/db/` 目录已在 `.gitignore` 中配置，运行时自动生成
- 第三方库请放在 `third_party/` 目录下
- 所有共享的头文件请放在 `common/` 目录