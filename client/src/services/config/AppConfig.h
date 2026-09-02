#ifndef APPCONFIG_H
#define APPCONFIG_H

#include <QString>

/**
 * @brief 应用级只读配置（单例）
 *
 * 集中管理模型路径、视频保存目录、FDBus 服务名等启动期参数，
 * 避免散落在各 Service/ViewModel 中的硬编码。
 *
 * 数据来源：可执行文件同目录下的 config.ini（与 ConfigService 共用同一文件）。
 * 文件不存在时使用内置默认值，保证开箱即用。
 *
 * config.ini 示例：
 *   [FDBus]
 *   ServerName=driving_assistant
 *
 *   [Models]
 *   Dir=                        ; 留空则自动按 appDir/../resources/models 查找
 *   VehicleModel=yolov8n.onnx
 *   TrafficLightModel=trafficrules-yolo11n.onnx
 *
 *   [Storage]
 *   VideoDir=Videos/360DrivingAssistant   ; 相对于 home 目录
 */
class AppConfig
{
public:
    static AppConfig& instance();

    // ---- FDBus ----
    QString fdbusServerName() const;

    // ---- Models ----
    /// 模型目录（绝对路径）。配置为空时回退到 appDir/../resources/models
    QString modelDir() const;
    QString vehicleModelName() const;
    QString trafficLightModelName() const;
    /// 解析车辆模型完整路径（目录 + 文件名），不存在则回退候选路径列表
    QString resolveVehicleModelPath() const;
    QString resolveTrafficLightModelPath() const;

    // ---- Storage ----
    /// 视频保存目录（绝对路径 = home + VideoDir）
    QString videoDir() const;

    // ---- Resources ----
    /// 解析资源文件路径（如 images/warning.png）。
    /// 候选目录：appDir/../resources → appDir/../../client/resources
    /// 找到返回绝对路径，未找到返回空串
    QString resolveResource(const QString &relativePath) const;

private:
    AppConfig();
    QString configFilePath() const;

    QString m_fdbusServerName;
    QString m_modelDir;
    QString m_vehicleModelName;
    QString m_trafficLightModelName;
    QString m_videoSubDir;  // 相对于 home 的子目录
};

#endif // APPCONFIG_H
