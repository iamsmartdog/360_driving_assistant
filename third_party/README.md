# 第三方库

本目录用于存放项目依赖的第三方库。

## 建议的第三方库

### 客户端
- Qt 5/6 (GUI框架)
- OpenSSL (加密通信)

### 服务端
- MySQL Connector/C++ (数据库连接)
- OpenSSL (加密通信)
- JSON 库 (如 nlohmann/json)
- 日志库 (如 spdlog)

## 安装方式

### 方式一：系统包管理器
```bash
# Ubuntu/Debian
sudo apt-get install qt5-default libmysqlclient-dev

# CentOS/RHEL
sudo yum install qt5-qtbase-devel mysql-devel
```

### 方式二：源码编译
将源码下载到此目录，编译后链接到项目。

### 方式三：使用 CMake FetchContent
在 CMakeLists.txt 中使用 FetchContent 自动下载依赖。

## 注意事项

- 不要将大型二进制文件提交到 Git
- 第三方库的版本信息请记录在本文件中
- 使用 .gitignore 排除不必要的文件