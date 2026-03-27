/**
 * @file loginpage.h
 * @brief 登录页面头文件 - 账号密码登录主线
 */

#ifndef LOGINPAGE_H
#define LOGINPAGE_H

#include <QWidget>
#include <QHideEvent>
#include <QShowEvent>
#include <QTimer>
#include <QUrl>
#include <QStringList>
#include <QVariant>
#include "features/auth/application/LoginController.h"
#include "features/auth/domain/AuthTypes.h"

// 前向声明UI类
namespace Ui
{
    class LoginPage;
}

class CardSerialSource;
class QNetworkAccessManager;
class QNetworkReply;

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
    void cardReaderStatusChanged(int status, const QString &message);

public slots:
    void onKeyboardVisibilityChanged(bool visible, int height);

private slots:
    void onLoginButtonClicked();
    void onSelectAccountClicked();
    void onCardCaptured(const CardCredential &credential);
    void onCardSourceError(const QString &message);
    void onConfigChanged(const QString &key, const QVariant &value);
    void refreshServiceReadyState();

    // 登录响应回调
    void onLoginSucceeded(const QString &username, const QString &roleName);
    void onLoginFailed(const QString &errorMessage);
    void onLoginStateChanged(bool inProgress);
    void onAccountListReady(const QStringList &accounts);
    void onAccountListFailed(const QString &errorMessage);
    void onAccountListStateChanged(bool inProgress);

private:
    // 当前账号列表按配置维度做缓存，避免重复请求和过时回填
    QString currentAccountListCacheKey() const;
    void updateInteractionState();
    void updateAccountRowHighlight();
    void setLoginInProgress(bool inProgress);
    void setAccountListLoading(bool inProgress);
    void setServiceReady(bool ready, const QString &message = QString());
    void startServiceReadyProbe();
    void cancelServiceReadyProbe();
    void showAccountDropdown();
    void invalidateAccountListCache();

    Ui::LoginPage *ui;
    bool m_loginInProgress;
    bool m_accountListLoading;
    bool m_pendingAccountPopup;
    // 账号列表缓存及其对应的请求键
    QStringList m_cachedAccounts;
    QString m_cachedAccountListKey;
    QString m_pendingAccountListKey;
    LoginController *m_controller;
    CardSerialSource *m_cardSource;
    QNetworkAccessManager *m_probeManager;
    QNetworkReply *m_probeReply;
    bool m_probeInFlight;
    QTimer *m_serviceReadyTimer;
    bool m_serviceReady;
    QString m_serviceReadyMessage;

protected:
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
};

#endif // LOGINPAGE_H
