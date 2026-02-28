/**
 * @file AuthService.h
 * @brief 认证服务 - 处理登录、登出、权限验证
 * @author RK3568项目组
 * @date 2026-02-01
 */

#ifndef AUTHSERVICE_H
#define AUTHSERVICE_H

#include <QObject>
#include <QString>
#include <QCryptographicHash>
#include "../core/AppContext.h"

/**
 * @brief 登录结果结构体
 */
struct LoginResult
{
    bool success;
    QString errorMessage;
    UserInfo userInfo;

    LoginResult() : success(false) {}
};

/**
 * @class AuthService
 * @brief 认证服务类
 *
 * 处理所有认证相关的业务逻辑：
 * - 用户名密码登录
 * - 刷卡登录
 * - 指纹登录
 * - 登出
 * - 密码加密
 */
class AuthService : public QObject
{
    Q_OBJECT

private:
    explicit AuthService(QObject *parent = nullptr);
    ~AuthService();

    AuthService(const AuthService &) = delete;
    AuthService &operator=(const AuthService &) = delete;

public:
    static AuthService *instance();

    /**
     * @brief 用户名密码登录
     * @param username 用户名
     * @param password 密码（明文，内部会加密处理）
     * @return 登录结果
     */
    LoginResult loginWithPassword(const QString &username, const QString &password);

    /**
     * @brief 刷卡登录
     * @param cardId 卡号
     * @return 登录结果
     */
    LoginResult loginWithCard(const QString &cardId);

    /**
     * @brief 指纹登录
     * @param fingerprintId 指纹ID
     * @return 登录结果
     */
    LoginResult loginWithFingerprint(const QString &fingerprintId);

    /**
     * @brief 登出
     */
    void logout();

    /**
     * @brief 修改密码
     * @param oldPassword 旧密码
     * @param newPassword 新密码
     * @return 是否成功
     */
    bool changePassword(const QString &oldPassword, const QString &newPassword);

    /**
     * @brief 验证密码强度
     * @param password 密码
     * @return 错误信息，空表示通过
     */
    QString validatePasswordStrength(const QString &password);

    /**
     * @brief 检查是否需要重新登录
     */
    bool needRelogin() const;

signals:
    /**
     * @brief 登录成功信号
     */
    void loginSucceeded(const UserInfo &user);

    /**
     * @brief 登录失败信号
     */
    void loginFailed(const QString &error);

    /**
     * @brief 登出信号
     */
    void loggedOut();

private:
    /**
     * @brief 加密密码
     */
    QString hashPassword(const QString &password, const QString &salt = "RK3568_SALT");

    /**
     * @brief 验证用户凭据（实际项目中应调用后端API）
     */
    LoginResult verifyCredentials(const QString &username, const QString &passwordHash);

    /**
     * @brief 从服务器获取用户信息
     */
    UserInfo fetchUserInfo(const QString &userId);

    static AuthService *s_instance;
};

#endif // AUTHSERVICE_H
