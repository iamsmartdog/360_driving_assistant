#ifndef SETTINGSVIEWMODEL_H
#define SETTINGSVIEWMODEL_H

#include <QObject>
#include "ConfigService.h"

class SettingsViewModel : public QObject
{
    Q_OBJECT

    Q_PROPERTY(int storageSize READ storageSize WRITE setStorageSize NOTIFY storageSizeChanged)
    Q_PROPERTY(bool autoDelete READ autoDelete WRITE setAutoDelete NOTIFY autoDeleteChanged)

public:
    explicit SettingsViewModel(QObject *parent = nullptr);

    static SettingsViewModel* instance();

    int storageSize() const;
    void setStorageSize(int size);

    bool autoDelete() const;
    void setAutoDelete(bool enabled);

    Q_INVOKABLE bool saveSettings();
    Q_INVOKABLE bool loadSettings();
    Q_INVOKABLE bool hasSettings();

signals:
    void storageSizeChanged();
    void autoDeleteChanged();

private:
    ConfigService m_configService;
    int m_storageSize = 10;
    bool m_autoDelete = false;
};

#endif // SETTINGSVIEWMODEL_H
