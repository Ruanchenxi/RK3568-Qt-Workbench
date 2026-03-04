/**
 * @file loginpage.h
 * @brief 登录页面头文件 - 方案C 智能检测式
 *
 * 支持三种登录方式：
 * 1. 刷卡登录 - 系统自动检测
 * 2. 指纹登录 - 系统自动检测
 * 3. 账号密码登录 - 手动输入
 */

#ifndef LOGINPAGE_H
#define LOGINPAGE_H

#include <QWidget>
#include "features/auth/application/LoginController.h"

// 前向声明UI类
namespace Ui
{
    class LoginPage;
}

/**
 * @class LoginPage
 * @brief 登录页面类
 */
class LoginPage : public QWidget
{
Q_OBJECT // 启用信号槽机制

    public : explicit LoginPage(QWidget *parent = nullptr);
    ~LoginPage();

signals:
    /**
     * @brief 登录成功信号
     * @param username 用户名
     * @param role 用户角色
     */
    void loginSuccess(const QString &username, const QString &role);

private slots:
    void onShowAccountLogin();
    void onShowDeviceLogin();
    void onLoginButtonClicked();

    // 登录响应回调
    void onLoginSucceeded(const QString &username, const QString &roleName);
    void onLoginFailed(const QString &errorMessage);
    void onLoginStateChanged(bool inProgress);

private:
    void setLoginInProgress(bool inProgress);

    Ui::LoginPage *ui;
    bool m_loginInProgress;
    LoginController *m_controller;
};

#endif // LOGINPAGE_H
