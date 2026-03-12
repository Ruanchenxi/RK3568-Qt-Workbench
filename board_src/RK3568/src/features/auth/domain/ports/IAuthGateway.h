/**
 * @file IAuthGateway.h
 * @brief 认证网关抽象（负责调用后端登录 API）
 */
#ifndef I_AUTH_GATEWAY_H
#define I_AUTH_GATEWAY_H

#include <QObject>

#include "features/auth/domain/AuthTypes.h"

class IAuthGateway : public QObject
{
    Q_OBJECT
public:
    explicit IAuthGateway(QObject *parent = nullptr)
        : QObject(parent)
    {
    }
    ~IAuthGateway() override = default;

    virtual void loginByPassword(const PasswordCredential &credential) = 0;
    virtual void loginByCard(const CardCredential &credential) = 0;
    virtual void loginByFingerprint(const FingerprintCredential &credential) = 0;

signals:
    void loginSucceeded(const AuthResult &result);
    void loginFailed(const QString &errorMessage);
    void loginStateChanged(bool inProgress);
};

#endif // I_AUTH_GATEWAY_H
