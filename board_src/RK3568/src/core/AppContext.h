/**
 * @file AppContext.h
 * @brief 全局应用上下文 - 单例模式管理全局状态
 * @author RK3568项目组
 * @date 2026-02-01
 */

#ifndef APPCONTEXT_H
#define APPCONTEXT_H

#include <QObject>
#include <QString>
#include <QDateTime>

/**
 * @brief 用户信息结构体
 */
struct UserInfo
{
    QString userId;      // 用户ID
    QString username;    // 用户名
    QString realName;    // 真实姓名
    QString role;        // 角色
    QString department;  // 部门
    QDateTime loginTime; // 登录时间

    bool isValid() const { return !userId.isEmpty(); }
    void clear()
    {
        userId.clear();
        username.clear();
        realName.clear();
        role.clear();
        department.clear();
        loginTime = QDateTime();
    }
};

/**
 * @class AppContext
 * @brief 全局应用上下文单例类
 *
 * 管理全局状态，包括：
 * - 当前登录用户信息
 * - 登录状态
 * - 应用级别的共享数据
 */
class AppContext : public QObject
{
    Q_OBJECT

private:
    explicit AppContext(QObject *parent = nullptr);
    ~AppContext();

    // 禁止拷贝
    AppContext(const AppContext &) = delete;
    AppContext &operator=(const AppContext &) = delete;

public:
    /**
     * @brief 获取单例实例
     */
    static AppContext *instance();

    // ========== 用户状态管理 ==========

    /**
     * @brief 设置当前用户（登录成功后调用）
     */
    void setCurrentUser(const UserInfo &user);

    /**
     * @brief 获取当前用户信息
     */
    const UserInfo &currentUser() const;

    /**
     * @brief 清除用户信息（登出时调用）
     */
    void clearCurrentUser();

    /**
     * @brief 是否已登录
     */
    bool isLoggedIn() const;

    /**
     * @brief 检查用户是否有指定权限
     */
    bool hasPermission(const QString &permission) const;

    // ========== 会话管理 ==========

    /**
     * @brief 获取登录时长（秒）
     */
    qint64 getSessionDuration() const;

    /**
     * @brief 刷新会话（防止超时）
     */
    void refreshSession();

    /**
     * @brief 检查会话是否过期
     */
    bool isSessionExpired() const;

    /**
     * @brief 设置会话超时时间（秒），默认30分钟
     */
    void setSessionTimeout(int seconds);

signals:
    /**
     * @brief 用户登录信号
     */
    void userLoggedIn(const UserInfo &user);

    /**
     * @brief 用户登出信号
     */
    void userLoggedOut();

    /**
     * @brief 会话即将过期信号（提前5分钟）
     */
    void sessionAboutToExpire();

    /**
     * @brief 会话已过期信号
     */
    void sessionExpired();

private:
    static AppContext *s_instance;
    UserInfo m_currentUser;
    QDateTime m_lastActivityTime;
    int m_sessionTimeout; // 会话超时时间（秒）
};

#endif // APPCONTEXT_H
