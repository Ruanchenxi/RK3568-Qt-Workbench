/**
 * @file AuthGatewayAdapter.cpp
 * @brief 认证网关实现
 */
#include "features/auth/infra/http/AuthGatewayAdapter.h"

#include "features/auth/infra/password/AuthServiceAdapter.h"
#include "shared/contracts/IAuthService.h"

namespace {
QString preferredName(const QJsonObject &user)
{
    const QString userName = user.value("user_name").toString().trimmed();
    if (!userName.isEmpty()) {
        return userName;
    }

    const QString account = user.value("account").toString().trimmed();
    if (!account.isEmpty()) {
        return account;
    }

    return user.value("username").toString().trimmed();
}

QString preferredRole(const QJsonObject &user)
{
    const QString roleName = user.value("role_name").toString().trimmed();
    if (!roleName.isEmpty()) {
        return roleName;
    }
    return user.value("role").toString().trimmed();
}
}

AuthGatewayAdapter::AuthGatewayAdapter(IAuthService *auth, QObject *parent)
    : IAuthGateway(parent)
    , m_auth(auth)
{
    if (!m_auth) {
        m_auth = new AuthServiceAdapter(this);
    }

    connect(m_auth, &IAuthService::loginSuccess,
            this, &AuthGatewayAdapter::onAuthLoginSuccess);
    connect(m_auth, &IAuthService::loginFailed,
            this, &AuthGatewayAdapter::loginFailed);
    connect(m_auth, &IAuthService::loginStateChanged,
            this, &AuthGatewayAdapter::loginStateChanged);
}

void AuthGatewayAdapter::loginByPassword(const PasswordCredential &credential)
{
    if (credential.userName.trimmed().isEmpty() || credential.password.isEmpty()) {
        emit loginFailed(QStringLiteral("用户名或密码不能为空"));
        return;
    }

    QString tenantId = credential.tenantId.trimmed();
    if (tenantId.isEmpty()) {
        tenantId = QStringLiteral("000000");
    }
    m_auth->login(credential.userName, credential.password, tenantId);
}

void AuthGatewayAdapter::loginByCard(const CardCredential &credential)
{
    const QString cardNo = credential.cardNo.trimmed();
    if (cardNo.isEmpty()) {
        emit loginFailed(QStringLiteral("未检测到有效卡号"));
        return;
    }

    QString tenantId = credential.tenantId.trimmed();
    if (tenantId.isEmpty()) {
        tenantId = QStringLiteral("000000");
    }

    m_auth->loginByCard(cardNo, tenantId);
}

void AuthGatewayAdapter::loginByFingerprint(const FingerprintCredential &credential)
{
    Q_UNUSED(credential)
    emit loginFailed(QStringLiteral("指纹登录暂未启用"));
}

void AuthGatewayAdapter::onAuthLoginSuccess(const QJsonObject &userInfo)
{
    AuthResult result;
    result.ok = true;
    result.userInfo = userInfo;
    result.userName = preferredName(userInfo);
    result.roleName = preferredRole(userInfo);
    if (m_auth) {
        result.accessToken = m_auth->accessToken();
        result.refreshToken = m_auth->refreshToken();
    }
    emit loginSucceeded(result);
}
