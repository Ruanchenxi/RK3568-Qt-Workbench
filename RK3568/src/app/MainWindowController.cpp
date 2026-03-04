/**
 * @file MainWindowController.cpp
 * @brief 主窗口控制器实现
 */
#include "app/MainWindowController.h"

#include "features/auth/infra/password/AuthServiceAdapter.h"

MainWindowController::MainWindowController(IAuthService *auth, QObject *parent)
    : QObject(parent)
    , m_auth(auth)
{
    if (!m_auth) {
        m_auth = new AuthServiceAdapter(this);
    }

    connect(m_auth, &IAuthService::loginSuccess, this, [this](const QJsonObject &) {
        emitUserStateChanged();
    });
    connect(m_auth, &IAuthService::loggedOut, this, [this]() {
        emitUserStateChanged();
    });
}

bool MainWindowController::isLoggedIn() const
{
    return !m_auth->accessToken().trimmed().isEmpty();
}

QString MainWindowController::currentUsername() const
{
    if (!isLoggedIn()) {
        return QStringLiteral("未登录");
    }

    const QString name = preferredUsername();
    return name.isEmpty() ? QStringLiteral("已登录") : name;
}

QString MainWindowController::currentRole() const
{
    if (!isLoggedIn()) {
        return QStringLiteral("游客");
    }

    const QString role = preferredRole();
    return role.isEmpty() ? QStringLiteral("用户") : role;
}

void MainWindowController::logout()
{
    m_auth->clearToken();
}

void MainWindowController::notifyStateChanged()
{
    emitUserStateChanged();
}

void MainWindowController::emitUserStateChanged()
{
    emit userStateChanged(isLoggedIn(), currentUsername(), currentRole());
}

QString MainWindowController::preferredUsername() const
{
    const QJsonObject user = m_auth->userInfo();
    QString name = user.value("user_name").toString().trimmed();
    if (!name.isEmpty()) {
        return name;
    }

    name = user.value("account").toString().trimmed();
    if (!name.isEmpty()) {
        return name;
    }

    return user.value("username").toString().trimmed();
}

QString MainWindowController::preferredRole() const
{
    const QJsonObject user = m_auth->userInfo();
    QString role = user.value("role_name").toString().trimmed();
    if (!role.isEmpty()) {
        return role;
    }

    return user.value("role").toString().trimmed();
}
