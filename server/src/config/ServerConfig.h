#ifndef SERVERCONFIG_H
#define SERVERCONFIG_H

#include <string>

/**
 * @brief 服务端配置（从 INI 文件加载，找不到则用编译期默认值）
 *
 * 集中管理 MySQL 凭据、FDBus 服务名等可变参数，避免硬编码在源码中。
 * 配置文件路径默认为可执行文件同目录下的 server.ini，
 * 也可通过环境变量 SERVER_CONFIG_PATH 覆盖。
 *
 * INI 格式示例：
 *   [Database]
 *   Host=localhost
 *   Port=3306
 *   User=driving
 *   Password=123456
 *   Database=driving_assistant
 *
 *   [FDBus]
 *   ServerName=driving_assistant
 */
struct ServerConfig
{
    // ---- Database ----
    std::string dbHost = "localhost";
    std::string dbUser = "driving";
    std::string dbPassword = "123456";
    std::string dbDatabase = "driving_assistant";
    unsigned int dbPort = 3306;

    // ---- FDBus ----
    std::string fdbusServerName = "driving_assistant";

    /**
     * @brief 从 INI 文件加载配置
     * @param filePath 配置文件路径，传空串则自动探测（环境变量 / 可执行文件同目录）
     * @return 加载成功返回 true；文件不存在或解析失败时返回 false（字段保持默认值）
     */
    bool loadFromFile(const std::string &filePath = "");

    /**
     * @brief 获取默认配置文件路径
     *        优先级：环境变量 SERVER_CONFIG_PATH > /proc/self/exe 同目录/server.ini
     */
    static std::string defaultConfigPath();
};

#endif // SERVERCONFIG_H
