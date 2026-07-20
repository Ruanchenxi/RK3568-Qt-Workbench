/**
 * @file AuthFlowCoordinator.h
 * @brief 认证流程编排器（当前仅启用密码分支）
 */
#ifndef AUTH_FLOW_COORDINATOR_H
#define AUTH_FLOW_COORDINATOR_H

#include <QObject>

#include "features/auth/domain/AuthTypes.h"

class IAuthGateway;

class AuthFlowCoordinator : public QObject
{
    Q_OBJECT
public:
    explicit AuthFlowCoordinator(IAuthGateway *gateway = nullptr, QObject *parent = nullptr);
    ~AuthFlowCoordinator() override = default;

    void loginByPassword(const QString &userName, const QString &password, const QString &tenantId);
    void loginByCard(const CardCredential &credential);
    void loginByFingerprint(const FingerprintCredential &credential);

signals:
    void loginSucceeded(const QString &userName, const QString &roleName);
    void loginFailed(const QString &errorMessage);
    void loginStateChanged(bool inProgress);

private slots:
    void onGatewayLoginSucceeded(const AuthResult &result);

private:
    IAuthGateway *m_gateway;
};

#endif // AUTH_FLOW_COORDINATOR_H
