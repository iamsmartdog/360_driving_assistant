#ifndef NETWORKVIEWMODEL_H
#define NETWORKVIEWMODEL_H

#include <QObject>
#include <QTimer>

class FdbusClientService;

/**
 * @brief 网络连接视图模型
 * 通过 FDBus 的 name_server + host_server 自动发现服务
 * 客户端和服务端在同一台机器上运行（先虚拟机开发调试，最终整体打包到板子运行）
 * 客户端用 svc://driving_assistant 自动发现服务端，无需配置IP地址
 */
class NetworkViewModel : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool isConnecting READ isConnecting NOTIFY connectingChanged)
    Q_PROPERTY(bool isConnected READ isConnected NOTIFY isConnectedChanged)

public:
    explicit NetworkViewModel(QObject *parent = nullptr);

    // 通过FDBus自动发现服务端（svc://）
    // serverName 为空时从 config.ini [FDBus] ServerName 读取
    Q_INVOKABLE void connectToServer(const QString &serverName = QString());

    // 断开连接
    Q_INVOKABLE void disconnectFromServer();

    // 是否已连接
    bool isConnected() const;
    Q_INVOKABLE bool isConnectedQml() const;

    // 是否正在连接中
    bool isConnecting() const;

signals:
    void connectionSuccess();
    void connectionFailed(const QString &errorMessage);
    void connectingChanged();
    void isConnectedChanged();

private slots:
    void onFdbusOnline();
    void onFdbusOffline();
    void onConnectTimeout();

private:
    void resetConnectingState();

    QTimer *m_connectTimer;
    bool m_isConnecting = false;
    bool m_isConnected = false;
};

#endif // NETWORKVIEWMODEL_H
