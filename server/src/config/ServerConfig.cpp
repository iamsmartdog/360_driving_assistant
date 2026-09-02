#include "ServerConfig.h"
#include "ServerLogger.h"

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <unistd.h>  // readlink

// ============================================================================
// 最小 INI 解析器
// 支持：[section]、key=value、# 注释、行首尾空白裁剪
// 不支持：多行值、转义、嵌套 section（满足当前需求即可，避免引入第三方库）
// ============================================================================
namespace {

std::string trim(const std::string &s)
{
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

} // namespace

bool ServerConfig::loadFromFile(const std::string &filePath)
{
    std::string path = filePath;
    if (path.empty()) {
        path = defaultConfigPath();
    }

    std::ifstream fin(path);
    if (!fin.is_open()) {
        return false;  // 文件不存在，保持默认值
    }

    // 用 "section.key" 做扁平化存储，加载后再映射到字段
    std::string line;
    std::string section;

    while (std::getline(fin, line)) {
        std::string trimmed = trim(line);
        if (trimmed.empty() || trimmed[0] == '#' || trimmed[0] == ';') {
            continue;
        }

        if (trimmed[0] == '[' && trimmed.back() == ']') {
            section = trim(trimmed.substr(1, trimmed.size() - 2));
            continue;
        }

        size_t eq = trimmed.find('=');
        if (eq == std::string::npos) continue;

        std::string key = trim(trimmed.substr(0, eq));
        std::string val = trim(trimmed.substr(eq + 1));
        std::string fullKey = section + "." + key;

        // ---- Database ----
        if (fullKey == "Database.Host")        dbHost = val;
        else if (fullKey == "Database.User")   dbUser = val;
        else if (fullKey == "Database.Password") dbPassword = val;
        else if (fullKey == "Database.Database") dbDatabase = val;
        else if (fullKey == "Database.Port")   dbPort = static_cast<unsigned int>(std::strtoul(val.c_str(), nullptr, 10));
        // ---- FDBus ----
        else if (fullKey == "FDBus.ServerName") fdbusServerName = val;
    }

    LOG_INFO("配置已加载: %s", path.c_str());
    return true;
}

std::string ServerConfig::defaultConfigPath()
{
    // 优先级 1：环境变量
    const char *envPath = std::getenv("SERVER_CONFIG_PATH");
    if (envPath && envPath[0] != '\0') {
        return envPath;
    }

    // 优先级 2：可执行文件同目录下的 server.ini
    // 通过 /proc/self/exe 获取可执行文件路径（Linux）
    char exePath[4096] = {0};
    ssize_t len = ::readlink("/proc/self/exe", exePath, sizeof(exePath) - 1);
    if (len > 0) {
        exePath[len] = '\0';
        std::string p(exePath);
        size_t slash = p.find_last_of('/');
        if (slash != std::string::npos) {
            return p.substr(0, slash) + "/server.ini";
        }
    }

    // 回退：当前工作目录
    return "server.ini";
}
