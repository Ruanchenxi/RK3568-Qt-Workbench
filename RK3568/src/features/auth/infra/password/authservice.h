#ifndef AUTHSERVICE_H
#define AUTHSERVICE_H

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QPointer>
class QNetworkReply;

/**
 * @brief 认证服务类
 * 负责用户登录、token管理和认证状态维护
 */
class AuthService : public QObject
{
    Q_OBJECT

public:
    // 获取单例
    static AuthService *instance();

    // 登录（发送登录请求）
    void login(const QString &userName, const QString &password, const QString &tenantId = "000000");

    // 保存 token 和用户信息
    void saveToken(const QString &accessToken, const QString &refreshToken, const QJsonObject &userInfo);

    // 获取 access token
    QString getAccessToken() const;

    // 获取 refresh token
    QString getRefreshToken() const;

    // 获取用户信息
    QJsonObject getUserInfo() const;

    // 获取用户名
    QString getUserName() const;

    // 清除 token（退出登录）
    void clearToken();

    // 检查是否已登录
    bool isLoggedIn() const;

    // 获取认证请求头（用于其他 API 请求）
    QString getAuthorizationHeader() const;

signals:
    // 登录成功信号
    void loginSuccess(const QJsonObject &userInfo);

    // 登录失败信号
    void loginFailed(const QString &errorMessage);

    // 登录请求状态变化（true=请求中，false=空闲）
    void loginStateChanged(bool inProgress);

    // 注销信号
    void loggedOut();

private:
    explicit AuthService(QObject *parent = nullptr);
    ~AuthService();

    // 禁用拷贝构造和赋值
    AuthService(const AuthService &) = delete;
    AuthService &operator=(const AuthService &) = delete;

    // MD5 加密密码
    QString encryptPassword(const QString &password) const;

    // 发送登录网络请求
    void sendLoginRequest(const QString &userName, const QString &encryptedPassword, const QString &tenantId);

    // 结束当前登录请求状态
    void finishLoginRequest(QNetworkReply *reply);

private:
    static AuthService *m_instance;

    QString m_accessToken;  // 访问令牌
    QString m_refreshToken; // 刷新令牌
    QJsonObject m_userInfo; // 用户信息
    QString m_userName;     // 用户名

    class QNetworkAccessManager *m_networkManager; // 网络请求管理器
    QPointer<QNetworkReply> m_pendingLoginReply;   // 当前登录请求句柄
    bool m_loginInProgress;                        // 是否正在执行登录请求
};

#endif // AUTHSERVICE_H
