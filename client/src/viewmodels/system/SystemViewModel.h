#ifndef SYSTEMVIEWMODEL_H
#define SYSTEMVIEWMODEL_H

#include <QObject>
#include <QStringList>
#include <QProcess>

/**
 * @brief 系统集成视图模型
 * 暴露给 QML，负责：
 *   - 启动外部应用（高德导航 / 网易云音乐）
 *   - WiFi 扫描与连接（连接手机热点等）
 *
 * 启动命令从 config.ini 的 [External] 段读取，便于在车机（原生 App）
 * 与开发机（Web URL）之间切换；缺省走浏览器打开网页版。
 */
class SystemViewModel : public QObject
{
    Q_OBJECT

    // 扫描到的 WiFi SSID 列表
    Q_PROPERTY(QStringList wifiNetworks READ wifiNetworks NOTIFY wifiNetworksChanged)
    // 是否正在扫描
    Q_PROPERTY(bool wifiScanning READ wifiScanning NOTIFY wifiScanningChanged)

public:
    explicit SystemViewModel(QObject *parent = nullptr);

    // 启动高德导航
    Q_INVOKABLE void launchNavigation();
    // 启动网易云音乐
    Q_INVOKABLE void launchMusic();

    // 扫描周围 WiFi（异步，完成后 wifiNetworksChanged）
    Q_INVOKABLE void refreshWifiNetworks();
    // 连接指定 WiFi（如手机热点），异步，结果由 wifiConnectResult 通知
    Q_INVOKABLE void connectWifi(const QString &ssid, const QString &password);

    QStringList wifiNetworks() const { return m_wifiNetworks; }
    bool wifiScanning() const { return m_wifiScanning; }

signals:
    void wifiNetworksChanged();
    void wifiScanningChanged();
    // 连接结果：success + 提示信息
    void wifiConnectResult(bool success, const QString &message);

private slots:
    void onWifiScanFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onWifiConnectFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    QString configPath() const;
    // 读取 [External] 段的启动命令，缺省返回 defaultValue
    QString readCommand(const QString &key, const QString &defaultValue) const;
    // 把 "prog arg1 arg2" 拆成程序+参数列表
    static void splitCommand(const QString &cmd, QString &program, QStringList &args);
    // 从 nmcli 输出中解析去重后的 SSID 列表
    static QStringList parseWifiList(const QString &output);

    QString m_configPath;
    QStringList m_wifiNetworks;
    bool m_wifiScanning = false;
    QProcess *m_scanProcess = nullptr;
    QProcess *m_connectProcess = nullptr;
};

#endif // SYSTEMVIEWMODEL_H
