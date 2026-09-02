cmake_minimum_required(VERSION 3.15)
project(monitor_daemon VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ==================== 源文件 ====================
set(SOURCES
    src/main.cpp
    src/core/ProcMonitor.cpp
    src/core/AnomalyDetector.cpp
    src/core/AlertNotifier.cpp
    src/config/MonitorConfig.cpp
)

set(HEADERS
    src/core/ProcMonitor.h
    src/core/AnomalyDetector.h
    src/core/AlertNotifier.h
    src/config/MonitorConfig.h
)

# ==================== LLM 诊断（可选，需要 libcurl-dev）====================
# 未安装 libcurl 开发头时，编译期关闭 LLM 功能，保证守护进程可独立构建。
find_package(CURL QUIET)
if(CURL_FOUND)
    list(APPEND SOURCES src/core/LLMDiagnoser.cpp)
    list(APPEND HEADERS src/core/LLMDiagnoser.h)
    message(STATUS "LLM 诊断: 已启用 (libcurl 可用)")
else()
    message(WARNING "libcurl 开发头未找到，LLM 诊断功能将被禁用。"
                    "安装 libcurl4-openssl-dev 后重新构建可启用。")
endif()

# ==================== 可执行文件 ====================
add_executable(monitor_daemon
    ${SOURCES}
    ${HEADERS}
)

target_include_directories(monitor_daemon PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
    ${CMAKE_CURRENT_SOURCE_DIR}/src/config
)

find_package(Threads REQUIRED)
target_link_libraries(monitor_daemon PRIVATE
    Threads::Threads
    pthread
)
if(CURL_FOUND)
    target_link_libraries(monitor_daemon PRIVATE ${CURL_LIBRARIES})
    target_compile_definitions(monitor_daemon PRIVATE MONITOR_ENABLE_LLM=1)
endif()

# 输出到统一 bin 目录
set_target_properties(monitor_daemon PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

message(STATUS "==========================================")
message(STATUS "监控守护进程构建配置")
message(STATUS "CURL: ${CURL_LIBRARIES}")
message(STATUS "==========================================")