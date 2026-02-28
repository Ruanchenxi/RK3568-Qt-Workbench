/**
 * @file AppContext.cpp
 * @brief 全局应用上下文实现
 */

#include "AppContext.h"
#include <QSet>

// 静态成员初始化
AppContext *AppContext::s_instance = nullptr;

AppContext::AppContext(QObject *parent)
    : QObject(parent), m_sessionTimeout(30 * 60) // 默认30分钟
{
}

AppContext::~AppContext()
{
}

AppContext *AppContext::instance()
{
    if (!s_instance)
    {
        s_instance = new AppContext();
    }
    return s_instance;
}

void AppContext::setCurrentUser(const UserInfo &user)
{
    if (!user.isValid())
    {
        return;
    }

    m_currentUser = user;
    m_currentUser.loginTime = QDateTime::currentDateTime();
    m_lastActivityTime = QDateTime::currentDateTime();

    emit userLoggedIn(m_currentUser);
}

const UserInfo &AppContext::currentUser() const
{
    return m_currentUser;
}

void AppContext::clearCurrentUser()
{
    m_currentUser.clear();
    m_lastActivityTime = QDateTime();
    emit userLoggedOut();
}

bool AppContext::isLoggedIn() const
{
    return m_currentUser.isValid();
}

bool AppContext::hasPermission(const QString &permission) const
{
    // 简单权限检查，后续可扩展为完整的RBAC
    if (!isLoggedIn())
    {
        return false;
    }

    // 管理员拥有所有权限
    if (m_currentUser.role == "管理员" || m_currentUser.role == "admin")
    {
        return true;
    }

    // 操作员权限
    if (m_currentUser.role == "操作员")
    {
        static const QSet<QString> kOperatorPermissions = {
            "power_off", "power_on", "view_keys", "view_logs"};
        return kOperatorPermissions.contains(permission);
    }

    // 审核员权限
    if (m_currentUser.role == "审核员")
    {
        static const QSet<QString> kReviewerPermissions = {
            "review_power_off", "review_power_on", "view_logs"};
        return kReviewerPermissions.contains(permission);
    }

    return false;
}

qint64 AppContext::getSessionDuration() const
{
    if (!isLoggedIn())
    {
        return 0;
    }
    return m_currentUser.loginTime.secsTo(QDateTime::currentDateTime());
}

void AppContext::refreshSession()
{
    m_lastActivityTime = QDateTime::currentDateTime();
}

bool AppContext::isSessionExpired() const
{
    if (!isLoggedIn())
    {
        return true;
    }

    qint64 idleTime = m_lastActivityTime.secsTo(QDateTime::currentDateTime());
    return idleTime > m_sessionTimeout;
}

void AppContext::setSessionTimeout(int seconds)
{
    // 会话超时最小值保护：低于60秒会导致用户体验异常。
    if (seconds < 60)
    {
        m_sessionTimeout = 60;
        return;
    }
    m_sessionTimeout = seconds;
}
