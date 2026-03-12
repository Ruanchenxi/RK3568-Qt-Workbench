/**
 * @file AuthFlowCoordinator.cpp
 * @brief 认证流程编排实现
 */
#include "features/auth/application/AuthFlowCoordinator.h"

#include "features/auth/domain/ports/IAuthGateway.h"
#include "features/auth/infra/http/AuthGatewayAdapter.h"
#include <QMetaType>

AuthFlowCoordinator::AuthFlowCoordinator(IAuthGateway *gateway, QObject *parent)
    : QObject(parent)
    , m_gateway(gateway)
{
    qRegisterMetaType<AuthResult>("AuthResult");

    if (!m_gateway) {
        m_gateway = new AuthGatewayAdapter(nullptr, this);
    }

    connect(m_gateway, &IAuthGateway::loginSucceeded,
            this, &AuthFlowCoordinator::onGatewayLoginSucceeded);
    connect(m_gateway, &IAuthGateway::loginFailed,
            this, &AuthFlowCoordinator::loginFailed);
    connect(m_gateway, &IAuthGateway::loginStateChanged,
            this, &AuthFlowCoordinator::loginStateChanged);
}

void AuthFlowCoordinator::loginByPassword(const QString &userName, const QString &password, const QString &tenantId)
{
    PasswordCredential credential;
    credential.userName = userName;
    credential.password = password;
    credential.tenantId = tenantId;
    m_gateway->loginByPassword(credential);
}

void AuthFlowCoordinator::loginByCard(const CardCredential &credential)
{
    m_gateway->loginByCard(credential);
}

void AuthFlowCoordinator::loginByFingerprint(const FingerprintCredential &credential)
{
    m_gateway->loginByFingerprint(credential);
}

void AuthFlowCoordinator::onGatewayLoginSucceeded(const AuthResult &result)
{
    if (!result.ok) {
        emit loginFailed(result.errorMessage.isEmpty() ? QStringLiteral("登录失败") : result.errorMessage);
        return;
    }
    emit loginSucceeded(result.userName, result.roleName);
}
