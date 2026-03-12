/**
 * @file mainwindow.h
 * @brief 主窗口头文件 - 数字化停送电安全管理系统
 * @author RK3568项目组
 * @date 2026-01-29
 *
 * 主窗口负责：
 * - 顶部导航栏管理（页面切换）
 * - 底部状态栏更新（时间、用户、设备状态）
 * - 用户登录状态管理
 * - 各子页面的协调与通信
 */

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMessageBox>
#include <QTimer>
#include <QDateTime>
#include "app/MainWindowController.h"

// 前向声明
class LoginPage;
class WorkbenchPage;
class KeyManagePage;
class SystemPage;
class LogPage;

namespace Ui
{
    class MainWindow;
}

/**
 * @class MainWindow
 * @brief 应用程序主窗口类
 *
 * 采用 QStackedWidget 实现多页面切换，
 * 每个功能模块对应一个独立页面。
 */
class MainWindow : public QMainWindow
{
Q_OBJECT // 让你的类能使用 Qt 的信号槽机制。

    public : explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    /**
     * @brief 页面索引枚举
     * 与 contentStackedWidget 中页面顺序一一对应
     */
    enum PageIndex
    {
        PAGE_LOGIN = 0,     ///< 登录页面
        PAGE_WORKBENCH = 1, ///< 工作台页面
        PAGE_KEYS = 2,      ///< 钥匙管理页面
        PAGE_SYSTEM = 3,    ///< 系统设置页面
        PAGE_LOG = 4,       ///< 服务日志页面
        PAGE_KEYBOARD = 5   ///< 虚拟键盘页面
    };

public slots:
    /**
     * @brief 处理用户登录成功
     * @param username 登录的用户名
     * @param role 用户角色
     */
    void onUserLoggedIn(const QString &username, const QString &role);

    /**
     * @brief 处理用户登出
     */
    void onUserLoggedOut();

private slots:
    // ========== 导航按钮槽函数 ==========
    void onBtnWorkbenchClicked(); ///< 工作台按钮点击
    void onBtnKeysClicked();      ///< 钥匙管理按钮点击
    void onBtnSystemClicked();    ///< 系统设置按钮点击
    void onBtnServiceClicked();   ///< 服务日志按钮点击
    void onBtnKeyboardClicked();  ///< 虚拟键盘按钮点击
    void onLogoutBtnClicked();    ///< 退出按钮点击

    // ========== 其他槽函数 ==========
    void updateTime(); ///< 更新时间显示

private:
    Ui::MainWindow *ui; ///< UI指针

    // ========== 页面指针 ==========
    LoginPage *m_loginPage;         ///< 登录页面
    WorkbenchPage *m_workbenchPage; ///< 工作台页面
    KeyManagePage *m_keyManagePage; ///< 钥匙管理页面
    SystemPage *m_systemPage;       ///< 系统设置页面
    LogPage *m_logPage;             ///< 服务日志页面
    MainWindowController *m_mainController; ///< 主窗口登录态控制器

    // ========== 状态变量 ==========
    // 登录态通过 MainWindowController 提供，主窗口不再直接读取全局上下文单例。
    QTimer *m_timeTimer; ///< 时间更新定时器

    // ========== 私有方法 ==========
    void setupPages();                       ///< 设置页面
    void setupConnections();                 ///< 设置信号槽连接
    void setupTimer();                       ///< 设置定时器
    void switchToPage(PageIndex page);       ///< 切换页面
    void updateNavButtonState();             ///< 更新导航按钮状态
    void updateUserDisplay();                ///< 更新用户显示信息
    void updateStatusBar();                  ///< 更新状态栏
    bool checkLoginRequired(PageIndex page); ///< 检查是否需要登录
};

#endif // MAINWINDOW_H
