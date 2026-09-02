#include "SystemViewModel.h"
#include <QCoreApplication>
#include <QSettings>
#include <QStandardPaths>
#include <QRegularExpression>
#include <QSet>
#include <QDebug>

// nmcli 扫描/连接命令
static const char *kWifiScanCmd = "nmcli";
static const char *kWifiDevice = "wlan0";

SystemViewModel::SystemViewModel(QObject *parent)
    : QObject(parent)
    , m_configPath(QCoreApplication::applicationDirPath() + "/config.ini")
{
}

// ============================================================================
// 启动外部应用
// ============================================================================
void SystemViewModel::launchNavigation()
{
    // 缺省：浏览器打开高德导航网页版；车机可在 config.ini 配置原生 App 命令
    QString cmd = readCommand("NavigationCommand", "xdg-open https://www.amap.com");
    QString program;
    QStringList args;
    splitCommand(cmd, program, args);

    if (program.isEmpty()) {
        qWarning() << "[System] 导航启动命令为空";
        return;
    }
    if (!QProcess::startDetached(program, args)) {
        qWarning() << "[System] 启动导航失败:" << program << args;
    } else {
        qDebug() << "[System] 启动导航:" << program << args;
    }
}

void SystemViewModel::launchMusic()
{
    // 缺省：浏览器打开网易云音乐网页版
    QString cmd = readCommand("MusicCommand", "xdg-open https://music.163.com");
    QString program;
    QStringList args;
    splitCommand(cmd, program, args);

    if (program.isEmpty()) {
        qWarning() << "[System] 音乐启动命令为空";
        return;
    }
    if (!QProcess::startDetached(program, args)) {
        qWarning() << "[System] 启动音乐失败:" << program << args;
    } else {
        qDebug() << "[System] 启动音乐:" << program << args;
    }
}

// ============================================================================
// WiFi 扫描
// ============================================================================
void SystemViewModel::refreshWifiNetworks()
{
    if (m_wifiScanning) return;  // 防止重复扫描

    if (!m_scanProcess) {
        m_scanProcess = new QProcess(this);
        connect(m_scanProcess,
                QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, &SystemViewModel::onWifiScanFinished);
    }

    m_wifiScanning = true;
    emit wifiScanningChanged();

    // -t: 不分列对齐(冒号分隔)  -f SSID: 只要SSID  --rescan yes: 强制重新扫描
    m_scanProcess->start(kWifiScanCmd,
                         QStringList{"-t", "-f", "SSID", "dev", "wifi", "list", "--rescan", "yes"});
}

void SystemViewModel::onWifiScanFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    Q_UNUSED(exitStatus)
    m_wifiScanning = false;
    emit wifiScanningChanged();

    if (exitCode != 0) {
        qWarning() << "[System] WiFi扫描失败, exitCode=" << exitCode
                   << " stderr=" << m_scanProcess->readAllStandardError();
        m_wifiNetworks.clear();
        emit wifiNetworksChanged();
        return;
    }

    QString output = QString::fromLocal8Bit(m_scanProcess->readAllStandardOutput());
    m_wifiNetworks = parseWifiList(output);
    qDebug() << "[System] 扫描到" << m_wifiNetworks.size() << "个WiFi";
    emit wifiNetworksChanged();
}

// ============================================================================
// WiFi 连接
// ============================================================================
void SystemViewModel::connectWifi(const QString &ssid, const QString &password)
{
    if (ssid.isEmpty()) {
        emit wifiConnectResult(false, QStringLiteral("请选择一个WiFi"));
        return;
    }
    if (m_connectProcess && m_connectProcess->state() != QProcess::NotRunning) {
        emit wifiConnectResult(false, QStringLiteral("正在连接中，请稍候"));
        return;
    }

    if (!m_connectProcess) {
        m_connectProcess = new QProcess(this);
        connect(m_connectProcess,
                QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, &SystemViewModel::onWifiConnectFinished);
    }

    // 以参数形式传递，避免 shell 注入和空格/特殊字符问题
    QStringList args{"dev", "wifi", "connect", ssid, "password", password};
    // 若有多个无线网卡，指定 wlan0；不存在时 nmcli 会忽略
    args << "ifname" << kWifiDevice;

    m_connectProcess->start(kWifiScanCmd, args);
    qDebug() << "[System] 连接WiFi:" << ssid;
}

void SystemViewModel::onWifiConnectFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    Q_UNUSED(exitStatus)
    QString stderrOut = QString::fromLocal8Bit(m_connectProcess->readAllStandardError()).trimmed();
    QString stdoutOut = QString::fromLocal8Bit(m_connectProcess->readAllStandardOutput()).trimmed();

    if (exitCode == 0) {
        qDebug() << "[System] WiFi连接成功";
        emit wifiConnectResult(true, QStringLiteral("连接成功"));
    } else {
        QString msg = stderrOut.isEmpty() ? QStringLiteral("连接失败 (exit=%1)").arg(exitCode) : stderrOut;
        qWarning() << "[System] WiFi连接失败:" << msg;
        emit wifiConnectResult(false, msg);
    }
}

// ============================================================================
// 辅助函数
// ============================================================================
QString SystemViewModel::configPath() const
{
    return m_configPath;
}

QString SystemViewModel::readCommand(const QString &key, const QString &defaultValue) const
{
    QSettings settings(m_configPath, QSettings::IniFormat);
    settings.beginGroup("External");
    QString val = settings.value(key, defaultValue).toString();
    settings.endGroup();
    return val;
}

void SystemViewModel::splitCommand(const QString &cmd, QString &program, QStringList &args)
{
    // 简单按空白拆分；对 "xdg-open https://..." 这类无空格URL足够
    // 复杂命令（带引号路径）可在 config.ini 中改用脚本
    QStringList parts = cmd.split(QRegularExpression("\\s+"), QString::SkipEmptyParts);
    if (parts.isEmpty()) {
        program.clear();
        args.clear();
        return;
    }
    program = parts.takeFirst();
    args = parts;
}

QStringList SystemViewModel::parseWifiList(const QString &output)
{
    QStringList result;
    QSet<QString> seen;  // 去重
    const auto lines = output.split('\n', QString::SkipEmptyParts);
    for (const QString &line : lines) {
        QString ssid = line.trimmed();
        // nmcli -t 用冒号转义SSID中的冒号为 "\:"，这里简单还原
        ssid.replace("\\:", ":");
        if (ssid.isEmpty()) continue;          // 跳过隐藏网络
        if (ssid.startsWith("--")) continue;    // 跳过占位符
        if (seen.contains(ssid)) continue;
        seen.insert(ssid);
        result.append(ssid);
    }
    return result;
}
