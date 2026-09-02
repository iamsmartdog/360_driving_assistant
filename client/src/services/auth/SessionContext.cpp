#include "SessionContext.h"

SessionContext& SessionContext::instance()
{
    static SessionContext inst;
    return inst;
}

SessionContext::SessionContext(QObject *parent)
    : QObject(parent)
{
}

QString SessionContext::username() const
{
    return m_username;
}

QString SessionContext::nickname() const
{
    return m_nickname;
}

bool SessionContext::isLoggedIn() const
{
    return !m_username.isEmpty();
}

void SessionContext::setLoginInfo(const QString &username, const QString &nickname)
{
    bool usernameChanged = (m_username != username);
    bool nicknameChanged = (m_nickname != nickname);

    m_username = username;
    m_nickname = nickname;

    if (usernameChanged) emit this->usernameChanged();
    if (nicknameChanged) emit this->nicknameChanged();
    emit loggedIn(username, nickname);
}

void SessionContext::clear()
{
    if (!m_username.isEmpty() || !m_nickname.isEmpty()) {
        m_username.clear();
        m_nickname.clear();
        emit usernameChanged();
        emit nicknameChanged();
        emit loggedOut();
    }
}
