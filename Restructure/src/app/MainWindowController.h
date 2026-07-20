/**
 * @file MainWindowController.h
 * @brief 主窗口控制器（登录态与用户展示信息编排）
 */
#ifndef MAINWINDOWCONTROLLER_H
#define MAINWINDOWCONTROLLER_H

#include <QObject>

#include "shared/contracts/IAuthService.h"

class MainWindowController : public QObject
{
    Q_OBJECT
public:
    explicit MainWindowController(IAuthService *auth = nullptr, QObject *parent = nullptr);
    ~MainWindowController() override = default;

    bool isLoggedIn() const;
    QString currentUsername() const;
    QString currentRole() const;
    void logout();
    void notifyStateChanged();

signals:
    void userStateChanged(bool loggedIn, const QString &username, const QString &role);

private:
    void emitUserStateChanged();
    QString preferredUsername() const;
    QString preferredRole() const;

    IAuthService *m_auth;
};

#endif // MAINWINDOWCONTROLLER_H
