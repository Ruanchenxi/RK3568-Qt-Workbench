/**
 * @file AuthFlowCoordinator.cpp
 * @brief 认证流程编排实现
 */
#include "features/auth/application/AuthFlowCoordinator.h"

#include "features/auth/domain/ports/IAuthGateway.h"
#include "platform/logging/LogService.h"
#include <QMetaType>
#include <QtGlobal>

AuthFlowCoordinator::AuthFlowCoordinator(IAuthGateway *gateway, QObject *parent)
    : QObject(parent)
    , m_gateway(gateway)
{
    qRegisterMetaType<AuthResult>("AuthResult");

    Q_ASSERT(m_gateway);

    connect(m_gateway, &IAuthGateway::loginSucceeded,
            this, &AuthFlowCoordinator::onGatewayLoginSucceeded);
    connect(m_gateway, &IAuthGateway::loginFailed, this, [this](const QString &message) {
        LOG_WARNING(QStringLiteral("AUTH"), QStringLiteral("login failed"));
        emit loginFailed(message);
    });
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
    LOG_INFO(QStringLiteral("AUTH"), QStringLiteral("login succeeded"));
    emit loginSucceeded(result.userName, result.roleName);
}
