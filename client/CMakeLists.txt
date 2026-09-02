cmake_minimum_required(VERSION 3.15)
project(360_driving_assistant_client VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# ==================== 依赖路径 ====================
set(FDBUS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/fdbus/build/install/usr")
set(PROTO_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../common/proto")
set(PROTO_GEN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../common/proto/generated")

# 查找Qt5组件
find_package(Qt5 REQUIRED COMPONENTS
    Quick
    QuickControls2
    Network
    Widgets    # MessageDialog需要
    Concurrent  # QtConcurrent::run (BirdRecordService后台构建)
)

# 查找Protobuf
find_package(Protobuf REQUIRED)

# 查找OpenCV（必需依赖）
find_package(OpenCV REQUIRED)
message(STATUS "OpenCV版本: ${OpenCV_VERSION}")

# 所有C++源文件
set(SOURCES
    src/main.cpp
    # services/auth
    src/services/auth/AuthService.cpp
    src/services/auth/SessionContext.cpp
    # services/network
    src/services/network/FdbusClientService.cpp
    # services/config
    src/services/config/ConfigService.cpp
    src/services/config/AppConfig.cpp
    # services/video/driving
    src/services/video/driving/VideoRecorderService.cpp
    # services/video/bird
    src/services/video/bird/BirdRecordService.cpp
    # services/video/reverse
    src/services/video/reverse/ReverseRecordService.cpp
    # services/video/playback
    src/services/video/playback/VideoPlaybackService.cpp
    src/services/video/playback/PlaybackDetectService.cpp
    # services/video/common
    src/services/video/common/VideoFrameProvider.cpp
    src/services/video/common/DetectionEngine.cpp
    src/services/video/common/VehicleDetector.cpp
    src/services/video/common/TrafficLightDetector.cpp
    src/services/video/common/TrafficLightColorVoter.cpp
    # viewmodels/auth
    src/viewmodels/auth/LoginViewModel.cpp
    # viewmodels/settings
    src/viewmodels/settings/SettingsViewModel.cpp
    # viewmodels/network
    src/viewmodels/network/NetworkViewModel.cpp
    # viewmodels
    src/viewmodels/ViewModelRegistry.cpp
    # viewmodels/video/driving
    src/viewmodels/video/driving/VideoRecordViewModel.cpp
    # viewmodels/video/reverse
    src/viewmodels/video/reverse/ReverseRecordViewModel.cpp
    # viewmodels/video/playback
    src/viewmodels/video/playback/VideoPlaybackViewModel.cpp
    src/viewmodels/video/playback/PlaybackDetectViewModel.cpp
    # viewmodels/video/screenshot
    src/viewmodels/video/screenshot/ScreenshotViewModel.cpp
    src/viewmodels/video/screenshot/ScreenshotListViewModel.cpp
    # viewmodels/video/bird
    src/viewmodels/video/bird/BirdRecordViewModel.cpp
    # viewmodels/system
    src/viewmodels/system/SystemViewModel.cpp
    # protobuf生成代码
    ${PROTO_GEN_DIR}/driving_assistant.pb.cc
)

# 所有头文件
set(HEADERS
    src/models/UserInfo.h
    src/models/SettingConfig.h
    # services/auth
    src/services/auth/AuthService.h
    src/services/auth/SessionContext.h
    # services/network
    src/services/network/FdbusClientService.h
    # services/config
    src/services/config/ConfigService.h
    src/services/config/AppConfig.h
    # services/video/driving
    src/services/video/driving/VideoRecorderService.h
    # services/video/bird
    src/services/video/bird/BirdRecordService.h
    # services/video/reverse
    src/services/video/reverse/ReverseRecordService.h
    # services/video/playback
    src/services/video/playback/VideoPlaybackService.h
    src/services/video/playback/PlaybackDetectService.h
    # services/video/common
    src/services/video/common/VideoFrameProvider.h
    src/services/video/common/DetectionEngine.h
    src/services/video/common/DetectionTypes.h
    src/services/video/common/VehicleDetector.h
    src/services/video/common/TrafficLightDetector.h
    src/services/video/common/TrafficLightColorVoter.h
    # viewmodels/auth
    src/viewmodels/auth/LoginViewModel.h
    # viewmodels/settings
    src/viewmodels/settings/SettingsViewModel.h
    # viewmodels/network
    src/viewmodels/network/NetworkViewModel.h
    # viewmodels
    src/viewmodels/ViewModelRegistry.h
    # viewmodels/video/driving
    src/viewmodels/video/driving/VideoRecordViewModel.h
    # viewmodels/video/reverse
    src/viewmodels/video/reverse/ReverseRecordViewModel.h
    # viewmodels/video/playback
    src/viewmodels/video/playback/VideoPlaybackViewModel.h
    src/viewmodels/video/playback/PlaybackDetectViewModel.h
    # viewmodels/video/screenshot
    src/viewmodels/video/screenshot/ScreenshotViewModel.h
    src/viewmodels/video/screenshot/ScreenshotListViewModel.h
    # viewmodels/video/bird
    src/viewmodels/video/bird/BirdRecordViewModel.h
    # viewmodels/system
    src/viewmodels/system/SystemViewModel.h
    # protobuf生成头文件
    ${PROTO_GEN_DIR}/driving_assistant.pb.h
)

# QML资源文件
set(QML_RESOURCES
    resources/qml.qrc
)

# 创建可执行文件
add_executable(client
    ${SOURCES}
    ${HEADERS}
    ${QML_RESOURCES}
)

# 包含目录
target_include_directories(client PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/models
    # services 子目录
    ${CMAKE_CURRENT_SOURCE_DIR}/src/services/auth
    ${CMAKE_CURRENT_SOURCE_DIR}/src/services/network
    ${CMAKE_CURRENT_SOURCE_DIR}/src/services/config
    # services/video 子目录
    ${CMAKE_CURRENT_SOURCE_DIR}/src/services/video/driving
    ${CMAKE_CURRENT_SOURCE_DIR}/src/services/video/bird
    ${CMAKE_CURRENT_SOURCE_DIR}/src/services/video/reverse
    ${CMAKE_CURRENT_SOURCE_DIR}/src/services/video/playback
    ${CMAKE_CURRENT_SOURCE_DIR}/src/services/video/common
    # viewmodels 子目录
    ${CMAKE_CURRENT_SOURCE_DIR}/src/viewmodels
    ${CMAKE_CURRENT_SOURCE_DIR}/src/viewmodels/auth
    ${CMAKE_CURRENT_SOURCE_DIR}/src/viewmodels/settings
    ${CMAKE_CURRENT_SOURCE_DIR}/src/viewmodels/network
    ${CMAKE_CURRENT_SOURCE_DIR}/src/viewmodels/system
    # viewmodels/video 子目录
    ${CMAKE_CURRENT_SOURCE_DIR}/src/viewmodels/video/driving
    ${CMAKE_CURRENT_SOURCE_DIR}/src/viewmodels/video/reverse
    ${CMAKE_CURRENT_SOURCE_DIR}/src/viewmodels/video/playback
    ${CMAKE_CURRENT_SOURCE_DIR}/src/viewmodels/video/screenshot
    ${CMAKE_CURRENT_SOURCE_DIR}/src/viewmodels/video/bird
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
    # FDBus头文件（include路径为上层目录，代码用#include <fdbus/xxx.h>）
    ${FDBUS_DIR}/include
    # Protobuf生成代码
    ${PROTO_GEN_DIR}
    # OpenCV头文件
    ${OpenCV_INCLUDE_DIRS}
)

# 链接库
target_link_libraries(client PRIVATE
    Qt5::Quick
    Qt5::QuickControls2
    Qt5::Network
    Qt5::Widgets
    Qt5::Concurrent
    # Protobuf
    protobuf::libprotobuf
    # FDBus（直接指定库文件路径）
    ${FDBUS_DIR}/lib/libfdbus.so
    # 系统库
    pthread
    # OpenCV
    ${OpenCV_LIBS}
)

# 运行时库路径策略：
# 使用 $ORIGIN 相对路径，将所有动态库拷贝到可执行文件同目录的lib/下
# 这样无论在哪台机器上运行，都不需要设置LD_LIBRARY_PATH
set(LIB_OUTPUT_DIR "${CMAKE_BINARY_DIR}/bin/lib")

# 构建完成后自动拷贝动态库和配置文件到输出目录
add_custom_command(TARGET client POST_BUILD
    COMMENT "拷贝动态库和配置文件到输出目录..."
    COMMAND ${CMAKE_COMMAND} -E make_directory "${LIB_OUTPUT_DIR}"
    # FDBus
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${FDBUS_DIR}/lib/libfdbus.so" "${LIB_OUTPUT_DIR}/"
    # 配置文件（部署时可直接编辑 bin/config.ini）
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/config/client.ini" "${CMAKE_BINARY_DIR}/bin/config.ini"
)

set_target_properties(client PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    # $ORIGIN/lib 表示相对于可执行文件所在目录的lib/子目录
    BUILD_RPATH "$ORIGIN/lib"
    INSTALL_RPATH "$ORIGIN/lib"
    SKIP_BUILD_RPATH FALSE
    BUILD_WITH_INSTALL_RPATH FALSE
)

message(STATUS "==========================================")
message(STATUS "客户端构建配置")
message(STATUS "Qt版本: ${Qt5_VERSION}")
message(STATUS "Protobuf版本: ${Protobuf_VERSION}")
message(STATUS "FDBus路径: ${FDBUS_DIR}")
message(STATUS "C++标准: ${CMAKE_CXX_STANDARD}")
message(STATUS "==========================================")
