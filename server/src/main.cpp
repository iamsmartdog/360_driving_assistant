#include <fdbus/fdbus.h>
#include "MySqlDao.h"
#include "UserDao.h"
#include "VideoDao.h"
#include "ScreenshotDao.h"
#include "AuthService.h"
#include "DrivingAssistantServer.h"
#include "ServerConfig.h"
#include "ServerLogger.h"

#include <csignal>
#include <cstdlib>
#include <unistd.h>

using namespace ipc::fdbus;

static CBaseWorker main_worker;

// ============================================================================
// 信号处理：仅做 async-signal-safe 的退出
//
// 历史实现用 printf + exit(signum)：printf 非 async-signal-safe（UB），且 exit
// 以信号码作为退出码不规范。此处改用 write（async-signal-safe）输出提示 + _exit(0)。
//
// 关于资源清理：FDBus 未公开 stop() 接口，main_worker / background_worker 线程
// 不可 join，主线程阻塞在 FDB_WORKER_EXE_IN_PLACE。若在信号上下文中主动调用
// MySqlDao::disconnect()，会与仍在处理请求的 worker 线程竞争（可能正在执行查询），
// 存在崩溃风险。因此 DB 连接交由 OS 回收（MySQL 服务端会话超时后自动清理），
// 这是当前 FDBus 生命周期模型下的已知折中。
// ============================================================================
static volatile sig_atomic_t g_shutdownRequested = 0;

static void shutdownSignalHandler(int /*signum*/)
{
    g_shutdownRequested = 1;
    // write 是 async-signal-safe；printf/fprintf 不是
    static const char msg[] = "\n收到关闭信号，服务器退出。\n";
    write(STDERR_FILENO, msg, sizeof(msg) - 1);
    _exit(0);
}

int main(int argc, char *argv[])
{
    signal(SIGINT, shutdownSignalHandler);
    signal(SIGTERM, shutdownSignalHandler);

    LOG_INFO("==========================================");
    LOG_INFO("  360度智能行车辅助系统 - 服务端");
    LOG_INFO("==========================================");

    // 0. 加载服务端配置（MySQL凭据 / FDBus服务名）
    ServerConfig config;
    bool configLoaded = config.loadFromFile();
    if (!configLoaded) {
        LOG_WARN("未找到 server.ini，使用默认配置（详见 config/server.ini）");
    }

    // 1. 初始化MySQL数据库连接
    MySqlDao db;
    if (!db.connect(config.dbHost, config.dbUser, config.dbPassword,
                    config.dbDatabase, config.dbPort)) {
        LOG_ERROR("数据库连接失败: %s@%s:%u/%s",
                  config.dbUser.c_str(), config.dbHost.c_str(),
                  config.dbPort, config.dbDatabase.c_str());
        LOG_ERROR("请检查 server.ini 中的 [Database] 配置或MySQL服务状态");
        return 1;
    }

    // 初始化数据库表（如果不存在则创建）
    const char *createUsersTable = R"(
        CREATE TABLE IF NOT EXISTS users (
            id INT AUTO_INCREMENT PRIMARY KEY,
            username VARCHAR(50) NOT NULL UNIQUE,
            password_md5 VARCHAR(255) NOT NULL,  -- 存储格式：pbkdf2$iter$salt$hash（列名为历史遗留）
            nickname VARCHAR(100) NOT NULL DEFAULT '',
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
    )";

    const char *createVideoTable = R"(
        CREATE TABLE IF NOT EXISTS video_records (
            id INT AUTO_INCREMENT PRIMARY KEY,
            username VARCHAR(50) NOT NULL,
            video_name VARCHAR(255) NOT NULL,
            video_size INT NOT NULL DEFAULT 0,
            video_path VARCHAR(500) DEFAULT '',
            record_type VARCHAR(50) DEFAULT '',
            duration_sec INT NOT NULL DEFAULT 0,
            resolution VARCHAR(20) DEFAULT '',
            fps INT NOT NULL DEFAULT 0,
            camera_source VARCHAR(50) DEFAULT '',
            last_play_sec INT NOT NULL DEFAULT 0,
            play_count INT NOT NULL DEFAULT 0,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            INDEX idx_username (username),
            INDEX idx_created_at (created_at)
        ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
    )";

    if (!db.executeUpdate(createUsersTable)) {
        LOG_ERROR("创建users表失败: %s", db.getLastError().c_str());
        return 1;
    }
    LOG_INFO("users表就绪");

    if (!db.executeUpdate(createVideoTable)) {
        LOG_ERROR("创建video_records表失败: %s", db.getLastError().c_str());
        return 1;
    }
    LOG_INFO("video_records表就绪");

    const char *createScreenshotTable = R"(
        CREATE TABLE IF NOT EXISTS screenshot_records (
            id INT AUTO_INCREMENT PRIMARY KEY,
            username VARCHAR(50) NOT NULL,
            screenshot_name VARCHAR(200) NOT NULL,
            screenshot_size INT NOT NULL DEFAULT 0,
            screenshot_path VARCHAR(500) DEFAULT '',
            record_type VARCHAR(50) DEFAULT '',
            detection_info TEXT,
            resolution VARCHAR(20) DEFAULT '',
            camera_source VARCHAR(50) DEFAULT '',
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            INDEX idx_username (username),
            INDEX idx_record_type (record_type)
        ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
    )";

    if (!db.executeUpdate(createScreenshotTable)) {
        LOG_ERROR("创建screenshot_records表失败: %s", db.getLastError().c_str());
        return 1;
    }
    LOG_INFO("screenshot_records表就绪");

    // 2. 初始化DAO和业务服务
    UserDao userDao(db);
    VideoDao videoDao(db);
    ScreenshotDao screenshotDao(db);
    AuthService authService(userDao, videoDao);

    // 3. 启动FDBus上下文
    FDB_CONTEXT->start();
    LOG_INFO("FDBus上下文已启动");

    // 4. 启动FDBus worker线程
    main_worker.start();
    LOG_INFO("FDBus worker线程已启动");

    // 5. 创建并绑定FDBus服务器
    std::string server_name = config.fdbusServerName;
    std::string svc_url = std::string(FDB_URL_SVC) + server_name;

    auto server = new DrivingAssistantServer(
        server_name.c_str(), &main_worker,
        db, userDao, videoDao, screenshotDao, authService
    );
    server->enableWatchdog(true);
    server->enableUDP(true);
    server->setExportableLevel(FDB_EXPORTABLE_SITE);

    // 绑定svc://（通过name_server自动发现，局域网内零配置）
    server->bind(svc_url.c_str());
    LOG_INFO("FDBus服务绑定: %s", svc_url.c_str());

    LOG_INFO("==========================================");
    LOG_INFO("服务器启动成功，等待客户端连接...");
    LOG_INFO("  连接方式: name_server + svc://%s", server_name.c_str());
    LOG_INFO("按Ctrl+C退出");
    LOG_INFO("==========================================");

    // 6. 将主线程转为FDBus worker（阻塞运行）
    CBaseWorker background_worker;
    background_worker.start(FDB_WORKER_EXE_IN_PLACE);

    return 0;
}
