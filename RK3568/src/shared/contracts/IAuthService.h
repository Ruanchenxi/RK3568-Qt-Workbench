/**
 * @file IAuthService.h
 * @brief 认证服务抽象接口
 *
 * 目标：
 * 1. 页面/控制器不直接依赖 AuthService 单例实现。
 * 2. 支持后续替换为 mock 实现做无网络测试。
 */
#ifndef IAUTHSERVICE_H
#define IAUTHSERVICE_H

#include <QObject>
#include <QJsonObject>
#include <QString>

class IAuthService : public QObject
{
    Q_OBJECT
public:
    explicit IAuthService(QObject *parent = nullptr) : QObject(parent) {}
    ~IAuthService() override = default;

    virtual void login(const QString &userName, const QString &password, const QString &tenantId = "000000") = 0;
    virtual void loginByCard(const QString &cardNo, const QString &tenantId = "000000") = 0;
    virtual QJsonObject userInfo() const = 0;
    virtual QString accessToken() const = 0;
    virtual QString refreshToken() const = 0;
    virtual void clearToken() = 0;

signals:
    void loginSuccess(const QJsonObject &userInfo);
    void loginFailed(const QString &errorMessage);
    void loginStateChanged(bool inProgress);
    void loggedOut();
};

#endif // IAUTHSERVICE_H
