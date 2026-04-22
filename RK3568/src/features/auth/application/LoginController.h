/**
 * @file LoginController.h
 * @brief 登录页面控制器
 */
#ifndef LOGINCONTROLLER_H
#define LOGINCONTROLLER_H

#include <QObject>
#include <QString>
#include <QStringList>

class AuthFlowCoordinator;

class LoginController : public QObject
{
    Q_OBJECT
public:
    explicit LoginController(AuthFlowCoordinator *flow = nullptr, QObject *parent = nullptr);
    ~LoginController() override = default;

    void login(const QString &username, const QString &password);
    void loginByCard(const QString &cardNo);
    void loginByFingerprint(const QByteArray &templateData);
    void requestAccountList();

signals:
    void loginSucceeded(const QString &username, const QString &role);
    void loginFailed(const QString &errorMessage);
    void loginStateChanged(bool inProgress);
    void accountListReady(const QStringList &accounts);
    void accountListFailed(const QString &errorMessage);
    void accountListStateChanged(bool inProgress);

private:
    AuthFlowCoordinator *m_flow;
};

#endif // LOGINCONTROLLER_H
