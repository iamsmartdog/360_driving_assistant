#include "NetworkViewModel.h"
#include "FdbusClientService.h"
#include <QDebug>

NetworkViewModel::NetworkViewModel(QObject *parent)
    : QObject(parent)
    , m_connectTimer(new QTimer(this))
{
    // FDBus连接信号
    auto &fdbus = FdbusClientService::instance();
    connect(&fdbus, &FdbusClientService::serverOnline,
            this, &NetworkViewModel::onFdbusOnline);
    connect(&fdbus, &FdbusClientService::serverOffline,
            this, &NetworkViewModel::onFdbusOffline);

    // 连接超时定时器
    m_connectTimer->setSingleShot(true);
    connect(m_connectTimer, &QTimer::timeout,
            this, &NetworkViewModel::onConnectTimeout);
}

// 通过FDBus自动发现服务端
void NetworkViewModel::connectToServer(const QString &serverName)
{
    if (m_isConnecting) return;
    m_isConnecting = true;
    emit connectingChanged();

    // 15秒超时
    m_connectTimer->start(15000);

    qDebug() << "NetworkViewModel: 通过FDBus连接服务端 -" << serverName;
    FdbusClientService::instance().connectByName(serverName);
}

void NetworkViewModel::disconnectFromServer()
{
    resetConnectingState();
    FdbusClientService::instance().disconnectFromServer();
}

bool NetworkViewModel::isConnected() const
{
    return m_isConnected;
}

bool NetworkViewModel::isConnectedQml() const
{
    return m_isConnected;
}

bool NetworkViewModel::isConnecting() const
{
    return m_isConnecting;
}

void NetworkViewModel::resetConnectingState()
{
    m_connectTimer->stop();
    if (m_isConnecting) {
        m_isConnecting = false;
        emit connectingChanged();
    }
}

// FDBus连接成功
void NetworkViewModel::onFdbusOnline()
{
    qDebug() << "NetworkViewModel: FDBus连接成功";
    m_isConnected = true;
    emit isConnectedChanged();
    resetConnectingState();
    emit connectionSuccess();
}

// FDBus连接断开
void NetworkViewModel::onFdbusOffline()
{
    qDebug() << "NetworkViewModel: FDBus连接断开";
    m_isConnected = false;
    emit isConnectedChanged();
    if (m_isConnecting) {
        resetConnectingState();
        emit connectionFailed("未连接到网络");
    }
}

// 连接超时
void NetworkViewModel::onConnectTimeout()
{
    qWarning() << "NetworkViewModel: 连接超时";
    FdbusClientService::instance().disconnectFromServer();
    resetConnectingState();
    emit connectionFailed("未连接到网络");
}
