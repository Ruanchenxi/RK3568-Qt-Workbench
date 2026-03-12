/**
 * @file AuthServiceAdapter.h
 * @brief AuthService 的接口适配器
 */
#ifndef AUTHSERVICEADAPTER_H
#define AUTHSERVICEADAPTER_H

#include "shared/contracts/IAuthService.h"

class AuthServiceAdapter : public IAuthService
{
    Q_OBJECT
public:
    explicit AuthServiceAdapter(QObject *parent = nullptr);
    ~AuthServiceAdapter() override = default;

    void login(const QString &userName, const QString &password, const QString &tenantId) override;
    QJsonObject userInfo() const override;
    QString accessToken() const override;
    QString refreshToken() const override;
    void clearToken() override;
};

#endif // AUTHSERVICEADAPTER_H
