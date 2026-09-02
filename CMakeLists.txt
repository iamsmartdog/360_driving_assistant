cmake_minimum_required(VERSION 3.15)
project(360_driving_assistant VERSION 1.0.0)

# 添加子目录
add_subdirectory(client)
add_subdirectory(server)
# 监控守护进程（独立系统监控中间件）
add_subdirectory(monitor)
