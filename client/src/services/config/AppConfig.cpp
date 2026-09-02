#include "AppConfig.h"

#include <QSettings>
#include <QCoreApplication>
#include <QFileInfo>
#include <QDir>

AppConfig& AppConfig::instance()
{
    static AppConfig inst;
    return inst;
}

AppConfig::AppConfig()
{
    QSettings settings(configFilePath(), QSettings::IniFormat);

    // [FDBus]
    settings.beginGroup("FDBus");
    m_fdbusServerName = settings.value("ServerName", "driving_assistant").toString();
    settings.endGroup();

    // [Models]
    settings.beginGroup("Models");
    m_modelDir = settings.value("Dir", "").toString();
    m_vehicleModelName = settings.value("VehicleModel", "yolov8n.onnx").toString();
    m_trafficLightModelName = settings.value("TrafficLightModel", "trafficrules-yolo11n.onnx").toString();
    settings.endGroup();

    // [Storage]
    settings.beginGroup("Storage");
    m_videoSubDir = settings.value("VideoDir", "Videos/360DrivingAssistant").toString();
    settings.endGroup();
}

QString AppConfig::configFilePath() const
{
    return QCoreApplication::applicationDirPath() + "/config.ini";
}

QString AppConfig::fdbusServerName() const
{
    return m_fdbusServerName;
}

QString AppConfig::modelDir() const
{
    if (!m_modelDir.isEmpty()) {
        return m_modelDir;
    }
    // 默认：可执行文件同目录的 ../resources/models
    return QCoreApplication::applicationDirPath() + "/../resources/models";
}

QString AppConfig::vehicleModelName() const
{
    return m_vehicleModelName;
}

QString AppConfig::trafficLightModelName() const
{
    return m_trafficLightModelName;
}

QString AppConfig::resolveVehicleModelPath() const
{
    // 候选路径：配置目录 → 开发目录回退
    QStringList candidates = {
        modelDir() + "/" + m_vehicleModelName,
        QCoreApplication::applicationDirPath() + "/../../client/resources/models/" + m_vehicleModelName,
    };
    for (const QString &p : candidates) {
        if (QFileInfo::exists(p)) {
            return p;
        }
    }
    return QString();  // 未找到
}

QString AppConfig::resolveTrafficLightModelPath() const
{
    QStringList candidates = {
        modelDir() + "/" + m_trafficLightModelName,
        QCoreApplication::applicationDirPath() + "/../../client/resources/models/" + m_trafficLightModelName,
    };
    for (const QString &p : candidates) {
        if (QFileInfo::exists(p)) {
            return p;
        }
    }
    return QString();
}

QString AppConfig::videoDir() const
{
    return QDir::homePath() + "/" + m_videoSubDir;
}

QString AppConfig::resolveResource(const QString &relativePath) const
{
    QStringList candidates = {
        QCoreApplication::applicationDirPath() + "/../resources/" + relativePath,
        QCoreApplication::applicationDirPath() + "/../../client/resources/" + relativePath,
    };
    for (const QString &p : candidates) {
        if (QFileInfo::exists(p)) {
            return p;
        }
    }
    return QString();
}
