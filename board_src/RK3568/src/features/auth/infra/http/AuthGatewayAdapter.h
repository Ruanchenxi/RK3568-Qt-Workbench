/**
 * @file AuthGatewayAdapter.h
 * @brief 认证网关实现（当前复用 IAuthService 账号密码链路）
 */
#ifndef AUTH_GATEWAY_ADAPTER_H
#define AUTH_GATEWAY_ADAPTER_H

#include <QPointer>

#include "features/auth/domain/ports/IAuthGateway.h"

class IAuthService;

class AuthGatewayAdapter : public IAuthGateway
{
    Q_OBJECT
public:
    explicit AuthGatewayAdapter(IAuthService *auth = nullptr, QObject *parent = nullptr);
    ~AuthGatewayAdapter() override = default;

    void loginByPassword(const PasswordCredential &credential) override;
    void loginByCard(const CardCredential &credential) override;
    void loginByFingerprint(const FingerprintCredential &credential) override;

private slots:
    void onAuthLoginSuccess(const QJsonObject &userInfo);

private:
    QPointer<IAuthService> m_auth;
};

#endif // AUTH_GATEWAY_ADAPTER_H
