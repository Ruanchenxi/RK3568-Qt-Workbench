/**
 * @file AuthTypes.h
 * @brief 认证领域模型定义
 */
#ifndef AUTH_TYPES_H
#define AUTH_TYPES_H

#include <QByteArray>
#include <QJsonObject>
#include <QString>

enum class AuthMode
{
    Password,
    Card,
    Fingerprint
};

struct PasswordCredential
{
    QString userName;
    QString password;
    QString tenantId;
};

struct CardCredential
{
    QString tenantId;
    QString cardNo;
};

struct FingerprintCredential
{
    QString tenantId;
    QString fingerId;
    QByteArray templateData;
};

struct AuthResult
{
    bool ok = false;
    QString accessToken;
    QString refreshToken;
    QString userName;
    QString roleName;
    QString errorMessage;
    QJsonObject userInfo;
};

Q_DECLARE_METATYPE(PasswordCredential)
Q_DECLARE_METATYPE(CardCredential)
Q_DECLARE_METATYPE(FingerprintCredential)
Q_DECLARE_METATYPE(AuthResult)

#endif // AUTH_TYPES_H
