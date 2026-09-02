#include "SettingsViewModel.h"
#include "SettingConfig.h"
#include <QDebug>

SettingsViewModel::SettingsViewModel(QObject *parent)
    : QObject(parent)
{
    if (m_configService.hasConfig()) {
        loadSettings();
    }
}

SettingsViewModel* SettingsViewModel::instance()
{
    static SettingsViewModel _instance;
    return &_instance;
}

int SettingsViewModel::storageSize() const { return m_storageSize; }

void SettingsViewModel::setStorageSize(int size)
{
    if (m_storageSize != size) {
        m_storageSize = size;
        emit storageSizeChanged();
    }
}

bool SettingsViewModel::autoDelete() const { return m_autoDelete; }

void SettingsViewModel::setAutoDelete(bool enabled)
{
    if (m_autoDelete != enabled) {
        m_autoDelete = enabled;
        emit autoDeleteChanged();
    }
}

bool SettingsViewModel::saveSettings()
{
    SettingsConfig config;
    config.storageSize = m_storageSize;
    config.autoDelete = m_autoDelete;

    return m_configService.saveConfig(config);
}

bool SettingsViewModel::loadSettings()
{
    SettingsConfig config = m_configService.loadConfig();

    m_storageSize = config.storageSize;
    m_autoDelete = config.autoDelete;

    emit storageSizeChanged();
    emit autoDeleteChanged();

    return true;
}

bool SettingsViewModel::hasSettings()
{
    return m_configService.hasConfig();
}
