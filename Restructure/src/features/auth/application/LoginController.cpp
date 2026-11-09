/**
 * @file LoginController.cpp
 * @brief 登录页面控制器实现
 */
#include "features/auth/application/LoginController.h"

#include "core/ConfigManager.h"
#include "features/auth/application/AuthFlowCoordinator.h"
#include "shared/contracts/IAuthService.h"

#include <QtGlobal>
LoginController::LoginController(AuthFlowCoordinator *flow, IAuthService *authService, QObject *parent)
    : QObject(parent)
    , m_flow(flow)
    , m_authService(authService)
{
    Q_ASSERT(m_flow);
    Q_ASSERT(m_authService);

    connect(m_flow, &AuthFlowCoordinator::loginSucceeded,
            this, &LoginController::loginSucceeded);
    connect(m_flow, &AuthFlowCoordinator::loginFailed,
            this, &LoginController::loginFailed);
    connect(m_flow, &AuthFlowCoordinator::loginStateChanged,
            this, &LoginController::loginStateChanged);
    connect(m_authService, &IAuthService::accountListReady,
            this, &LoginController::accountListReady);
    connect(m_authService, &IAuthService::accountListFailed,
            this, &LoginController::accountListFailed);
    connect(m_authService, &IAuthService::accountListStateChanged,
            this, &LoginController::accountListStateChanged);
}

void LoginController::login(const QString &username, const QString &password)
{
    QString tenantId = ConfigManager::instance()->tenantCode().trimmed();
    if (tenantId.isEmpty()) {
        tenantId = QStringLiteral("000000");
    }
    m_flow->loginByPassword(username, password, tenantId);
}

void LoginController::loginByCard(const QString &cardNo)
{
    QString tenantId = ConfigManager::instance()->tenantCode().trimmed();
    if (tenantId.isEmpty()) {
        tenantId = QStringLiteral("000000");
    }

    CardCredential credential;
    credential.tenantId = tenantId;
    credential.cardNo = cardNo.trimmed();
    m_flow->loginByCard(credential);
}

void LoginController::loginByFingerprint(const QByteArray &templateData)
{
    QString tenantId = ConfigManager::instance()->tenantCode().trimmed();
    if (tenantId.isEmpty()) {
        tenantId = QStringLiteral("000000");
    }

    FingerprintCredential credential;
    credential.tenantId = tenantId;
    credential.templateData = templateData;
    m_flow->loginByFingerprint(credential);
}

void LoginController::requestAccountList()
{
    QString tenantId = ConfigManager::instance()->tenantCode().trimmed();
    if (tenantId.isEmpty()) {
        tenantId = QStringLiteral("000000");
    }
    m_authService->fetchAccountList(tenantId);
}
