/**
 * @file WorkbenchController.cpp
 * @brief 工作台页面控制器实现
 */
#include "features/workbench/application/WorkbenchController.h"

#include <QUrlQuery>

#include "core/ConfigManager.h"
#include "features/auth/infra/password/AuthServiceAdapter.h"

WorkbenchController::WorkbenchController(IAuthService *auth, QObject *parent)
    : QObject(parent)
    , m_auth(auth)
    , m_hasLoaded(false)
    , m_lastSignature()
{
    if (!m_auth) {
        m_auth = new AuthServiceAdapter(this);
    }
}

bool WorkbenchController::shouldLoadNow()
{
    const QString signature = currentAuthSignature();
    if (!m_hasLoaded || m_lastSignature != signature) {
        m_hasLoaded = true;
        m_lastSignature = signature;
        return true;
    }
    return false;
}

bool WorkbenchController::buildTargetUrl(QUrl *url, QString *safeSummary, QString *error) const
{
    if (!url) {
        if (error) {
            *error = QStringLiteral("url 输出参数为空");
        }
        return false;
    }

    const QString accessToken = m_auth->accessToken();
    const QString refreshToken = m_auth->refreshToken();
    const QString tenantId = ConfigManager::instance()->tenantCode();
    const QString baseUrlText = ConfigManager::instance()->homeUrl().trimmed();

    if (baseUrlText.isEmpty()) {
        if (error) {
            *error = QStringLiteral("未配置工作台网址");
        }
        return false;
    }

    const QUrl baseUrl = QUrl::fromUserInput(baseUrlText);
    if (!baseUrl.isValid() || baseUrl.scheme().isEmpty() || baseUrl.host().isEmpty()) {
        if (error) {
            *error = QStringLiteral("工作台网址无效: %1").arg(baseUrlText);
        }
        return false;
    }

     if (!accessToken.isEmpty() && !refreshToken.isEmpty()) {
        *url = baseUrl;
        url->setPath(QStringLiteral("/pad/pages/oauth/third-login"));
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("accessToken"), accessToken);
        query.addQueryItem(QStringLiteral("refreshToken"), refreshToken);
        query.addQueryItem(QStringLiteral("tenantId"), tenantId);
        url->setQuery(query);

        if (safeSummary) {
            *safeSummary = QStringLiteral("%1://%2:%3/pad/pages/oauth/third-login")
                               .arg(url->scheme())
                               .arg(url->host())
                               .arg(url->port(url->scheme() == QStringLiteral("https") ? 443 : 80));
        }
    } else {
        *url = baseUrl;
        if (safeSummary) {
            *safeSummary = url->toString();
        }
    }

    if (error) {
        error->clear();
    }
    return true;
}

QString WorkbenchController::currentAuthSignature() const
{
    return QString("%1|%2|%3")
        .arg(m_auth->accessToken())
        .arg(m_auth->refreshToken())
        .arg(ConfigManager::instance()->tenantCode());
}
