/**
 * @file AuthServiceAdapter.cpp
 * @brief AuthService 的接口适配器
 */
#include "features/auth/infra/password/AuthServiceAdapter.h"

#include "features/auth/infra/password/authservice.h"

AuthServiceAdapter::AuthServiceAdapter(QObject *parent)
    : IAuthService(parent)
{
    connect(AuthService::instance(), &AuthService::loginSuccess,
            this, &AuthServiceAdapter::loginSuccess);
    connect(AuthService::instance(), &AuthService::loginFailed,
            this, &AuthServiceAdapter::loginFailed);
    connect(AuthService::instance(), &AuthService::loginStateChanged,
            this, &AuthServiceAdapter::loginStateChanged);
    connect(AuthService::instance(), &AuthService::loggedOut,
            this, &AuthServiceAdapter::loggedOut);
}

void AuthServiceAdapter::login(const QString &userName, const QString &password, const QString &tenantId)
{
    AuthService::instance()->login(userName, password, tenantId);
}

QJsonObject AuthServiceAdapter::userInfo() const
{
    return AuthService::instance()->getUserInfo();
}

QString AuthServiceAdapter::accessToken() const
{
    return AuthService::instance()->getAccessToken();
}

QString AuthServiceAdapter::refreshToken() const
{
    return AuthService::instance()->getRefreshToken();
}

void AuthServiceAdapter::clearToken()
{
    AuthService::instance()->clearToken();
}
