cmake_minimum_required(VERSION 3.15)
project(360_driving_assistant_server VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ==================== 依赖路径 ====================
# FDBus：交叉编译时用 aarch64 版本，本机编译用 x86 版本
if(CMAKE_CROSSCOMPILING)
    set(FDBUS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/fdbus/build_aarch64/install/usr/usr")
    set(AARCH64_SYSROOT "$ENV{HOME}/tools/aarch64-cross/sysroot")
else()
    set(FDBUS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/fdbus/build/install/usr")
endif()
set(PROTO_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../common/proto")
set(PROTO_GEN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../common/proto/generated")

# 查找Protobuf
find_package(Protobuf REQUIRED)

# 查找MySQL
find_package(Threads REQUIRED)

# MySQL开发库路径（交叉编译时从sysroot找，本机编译从系统路径找）
if(CMAKE_CROSSCOMPILING)
    find_library(MYSQL_CLIENT_LIBRARIES NAMES mysqlclient
        PATHS ${AARCH64_SYSROOT}/usr/lib/aarch64-linux-gnu
    )
    find_path(MYSQL_INCLUDE_DIRS NAMES mysql.h
        PATHS ${AARCH64_SYSROOT}/usr/include/mysql
    )
else()
    find_library(MYSQL_CLIENT_LIBRARIES NAMES mysqlclient mysqlcppconn
        PATHS /usr/lib/x86_64-linux-gnu /usr/lib64 /usr/lib
    )
    find_path(MYSQL_INCLUDE_DIRS NAMES mysql.h
        PATHS /usr/include/mysql /usr/include/mariadb
    )
endif()
if(NOT MYSQL_CLIENT_LIBRARIES)
    message(WARNING "MySQL client library not found, building without MySQL support")
endif()

# 所有C++源文件
set(SOURCES
    src/main.cpp
    # FDBus服务器
    src/services/server/DrivingAssistantServer.cpp
    # 配置管理
    src/config/ServerConfig.cpp
    # MySQL DAO/common
    src/dao/common/MySqlDao.cpp
    # MySQL DAO/user
    src/dao/user/UserDao.cpp
    # MySQL DAO/video
    src/dao/video/VideoDao.cpp
    # MySQL DAO/screenshot
    src/dao/screenshot/ScreenshotDao.cpp
    # 业务逻辑/auth
    src/services/auth/AuthService.cpp
    # utils
    src/utils/PasswordHasher.cpp
    # Protobuf生成代码
    ${PROTO_GEN_DIR}/driving_assistant.pb.cc
)

# 所有头文件
set(HEADERS
    # services/server
    src/services/server/DrivingAssistantServer.h
    # services/auth
    src/services/auth/AuthService.h
    # config
    src/config/ServerConfig.h
    # dao/common
    src/dao/common/MySqlDao.h
    # dao/user
    src/dao/user/UserDao.h
    # dao/video
    src/dao/video/VideoDao.h
    # dao/screenshot
    src/dao/screenshot/ScreenshotDao.h
    # models
    src/models/UserModel.h
    # utils
    src/utils/PasswordHasher.h
    src/utils/ServerLogger.h
    # Protobuf生成头文件
    ${PROTO_GEN_DIR}/driving_assistant.pb.h
)

# 创建可执行文件
add_executable(server
    ${SOURCES}
    ${HEADERS}
)

# 包含目录
target_include_directories(server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    # services 子目录
    ${CMAKE_CURRENT_SOURCE_DIR}/src/services/auth
    ${CMAKE_CURRENT_SOURCE_DIR}/src/services/server
    # config
    ${CMAKE_CURRENT_SOURCE_DIR}/src/config
    # dao 子目录
    ${CMAKE_CURRENT_SOURCE_DIR}/src/dao/common
    ${CMAKE_CURRENT_SOURCE_DIR}/src/dao/user
    ${CMAKE_CURRENT_SOURCE_DIR}/src/dao/video
    ${CMAKE_CURRENT_SOURCE_DIR}/src/dao/screenshot
    # models
    ${CMAKE_CURRENT_SOURCE_DIR}/src/models
    # utils
    ${CMAKE_CURRENT_SOURCE_DIR}/src/utils
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
    # FDBus头文件（include路径为上层目录，代码用#include <fdbus/xxx.h>）
    ${FDBUS_DIR}/include
    # Protobuf生成代码
    ${PROTO_GEN_DIR}
    # MySQL头文件
    ${MYSQL_INCLUDE_DIRS}
)

# 交叉编译时：手动添加 aarch64 sysroot 的头文件路径
if(CMAKE_CROSSCOMPILING)
    target_include_directories(server PRIVATE
        # Protobuf 头文件
        ${AARCH64_SYSROOT}/usr/include
        # MySQL 头文件
        ${AARCH64_SYSROOT}/usr/include/mysql
    )
endif()

# 链接库
if(CMAKE_CROSSCOMPILING)
    # 交叉编译：直接指定 aarch64 库文件
    find_library(Protobuf_LIBRARY NAMES protobuf
        PATHS ${AARCH64_SYSROOT}/usr/lib/aarch64-linux-gnu
    )
    # MySQL 依赖的间接库
    find_library(ZLIB_LIBRARY NAMES z
        PATHS ${AARCH64_SYSROOT}/usr/lib/aarch64-linux-gnu
              ${AARCH64_SYSROOT}/lib/aarch64-linux-gnu
    )
    find_library(SSL_LIBRARY NAMES ssl
        PATHS ${AARCH64_SYSROOT}/usr/lib/aarch64-linux-gnu
    )
    find_library(CRYPTO_LIBRARY NAMES crypto
        PATHS ${AARCH64_SYSROOT}/usr/lib/aarch64-linux-gnu
    )
    target_link_libraries(server PRIVATE
        # Protobuf
        ${Protobuf_LIBRARY}
        # FDBus
        ${FDBUS_DIR}/lib/libfdbus.so
        # MySQL
        ${MYSQL_CLIENT_LIBRARIES}
        # MySQL的间接依赖
        ${ZLIB_LIBRARY}
        ${SSL_LIBRARY}
        ${CRYPTO_LIBRARY}
        # 系统库
        Threads::Threads
        pthread
    )
else()
    # 本机编译：用系统包管理器安装的库
    find_package(OpenSSL REQUIRED)
    target_link_libraries(server PRIVATE
        # Protobuf
        protobuf::libprotobuf
        # FDBus
        ${FDBUS_DIR}/lib/libfdbus.so
        # MySQL
        ${MYSQL_CLIENT_LIBRARIES}
        # OpenSSL（用于 PasswordHasher 的 PBKDF2）
        OpenSSL::Crypto
        OpenSSL::SSL
        # 系统库
        Threads::Threads
        pthread
    )
endif()

# 运行时库路径策略：
# 使用 $ORIGIN 相对路径，将所有动态库拷贝到可执行文件同目录的lib/下
set(LIB_OUTPUT_DIR "${CMAKE_BINARY_DIR}/bin/lib")

add_custom_command(TARGET server POST_BUILD
    COMMENT "拷贝动态库和配置文件到输出目录..."
    COMMAND ${CMAKE_COMMAND} -E make_directory "${LIB_OUTPUT_DIR}"
    # FDBus
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${FDBUS_DIR}/lib/libfdbus.so" "${LIB_OUTPUT_DIR}/"
    # 配置文件
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/config/server.ini" "${CMAKE_BINARY_DIR}/bin/server.ini"
)

set_target_properties(server PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    BUILD_RPATH "$ORIGIN/lib"
    INSTALL_RPATH "$ORIGIN/lib"
    SKIP_BUILD_RPATH FALSE
    BUILD_WITH_INSTALL_RPATH FALSE
)

message(STATUS "==========================================")
message(STATUS "服务端构建配置")
if(CMAKE_CROSSCOMPILING)
    message(STATUS "交叉编译: aarch64")
endif()
message(STATUS "Protobuf版本: ${Protobuf_VERSION}")
message(STATUS "FDBus路径: ${FDBUS_DIR}")
message(STATUS "MySQL库: ${MYSQL_CLIENT_LIBRARIES}")
message(STATUS "C++标准: ${CMAKE_CXX_STANDARD}")
message(STATUS "==========================================")
