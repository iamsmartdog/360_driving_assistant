#ifndef CONFIGSERVICE_H
#define CONFIGSERVICE_H

#include "SettingConfig.h"
#include <QString>

/**
 * @brief 配置服务类
 * 负责配置文件的读取和保存业务逻辑
 * 不暴露给QML，只被ViewModel调用
 */
class ConfigService
{
public:
    ConfigService();

    // 保存配置到文件
    bool saveConfig(const SettingsConfig& config);

    // 从文件加载配置
    SettingsConfig loadConfig();

    // 检查配置文件是否存在
    bool hasConfig() const;

    // 获取配置文件路径
    QString getConfigPath() const;

private:
    QString m_configPath;  // 配置文件路径
};

#endif // CONFIGSERVICE_H