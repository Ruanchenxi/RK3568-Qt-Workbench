/**
 * @file LoginController.cpp
 * @brief 登录页面控制器实现
 */
#include "features/auth/application/LoginController.h"

#include "core/ConfigManager.h"
#include "features/auth/application/AuthFlowCoordinator.h"
#include "features/auth/infra/password/authservice.h"

LoginController::LoginController(AuthFlowCoordinator *flow, QObject *parent)
    : QObject(parent)
    , m_flow(flow)
{
    if (!m_flow) {
        m_flow = new AuthFlowCoordinator(nullptr, this);
    }

    connect(m_flow, &AuthFlowCoordinator::loginSucceeded,
            this, &LoginController::loginSucceeded);
    connect(m_flow, &AuthFlowCoordinator::loginFailed,
            this, &LoginController::loginFailed);
    connect(m_flow, &AuthFlowCoordinator::loginStateChanged,
            this, &LoginController::loginStateChanged);
    connect(AuthService::instance(), &AuthService::accountListReady,
            this, &LoginController::accountListReady);
    connect(AuthService::instance(), &AuthService::accountListFailed,
            this, &LoginController::accountListFailed);
    connect(AuthService::instance(), &AuthService::accountListStateChanged,
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

void LoginController::requestAccountList()
{
    QString tenantId = ConfigManager::instance()->tenantCode().trimmed();
    if (tenantId.isEmpty()) {
        tenantId = QStringLiteral("000000");
    }
    AuthService::instance()->fetchAccountList(tenantId);
}
