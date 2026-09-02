#include "ConfigService.h"
#include <QSettings>
#include <QFile>
#include <QCoreApplication>
#include <QDebug>

ConfigService::ConfigService()
{
    QString appPath = QCoreApplication::applicationDirPath();
    m_configPath = appPath + "/config.ini";
}

bool ConfigService::saveConfig(const SettingsConfig& config)
{
    QSettings settings(m_configPath, QSettings::IniFormat);

    settings.beginGroup("Storage");
    settings.setValue("StorageSize", config.storageSize);
    settings.setValue("AutoDelete", config.autoDelete);
    settings.endGroup();

    settings.sync();

    if (settings.status() == QSettings::NoError) {
        qDebug() << "配置保存成功:" << m_configPath;
        return true;
    } else {
        qWarning() << "配置保存失败";
        return false;
    }
}

SettingsConfig ConfigService::loadConfig()
{
    SettingsConfig config;

    if (!hasConfig()) {
        qWarning() << "配置文件不存在，使用默认配置";
        return config;
    }

    QSettings settings(m_configPath, QSettings::IniFormat);

    settings.beginGroup("Storage");
    config.storageSize = settings.value("StorageSize", 10).toInt();
    config.autoDelete = settings.value("AutoDelete", false).toBool();
    settings.endGroup();

    qDebug() << "配置加载成功:" << config.storageSize << "GB, autoDelete:" << config.autoDelete;
    return config;
}

bool ConfigService::hasConfig() const
{
    return QFile::exists(m_configPath);
}

QString ConfigService::getConfigPath() const
{
    return m_configPath;
}
