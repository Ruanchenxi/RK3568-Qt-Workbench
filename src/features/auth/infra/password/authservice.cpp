#include "features/auth/infra/password/authservice.h"
#include "core/ConfigManager.h"
#include "core/AppContext.h"
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QCryptographicHash>
#include <QDebug>

namespace
{
QString extractResponseMessage(const QByteArray &responseData)
{
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(responseData, &parseError);
    if (doc.isNull() || !doc.isObject())
    {
        return QString();
    }

    const QJsonObject obj = doc.object();
    const QString msg = obj.value(QStringLiteral("msg")).toString().trimmed();
    if (!msg.isEmpty())
    {
        return msg;
    }

    const QJsonObject dataObj = obj.value(QStringLiteral("data")).toObject();
    return dataObj.value(QStringLiteral("msg")).toString().trimmed();
}

QString normalizePasswordLoginMessage(const QString &rawMessage)
{
    const QString message = rawMessage.trimmed();
    const QString lower = message.toLower();

    if (message.isEmpty()
        || message.contains(QStringLiteral("密码"))
        || message.contains(QStringLiteral("账号"))
        || message.contains(QStringLiteral("用户"))
        || lower.contains(QStringLiteral("password"))
        || lower.contains(QStringLiteral("bad credentials"))
        || lower.contains(QStringLiteral("bad request"))
        || lower.contains(QStringLiteral("username")))
    {
        return QStringLiteral("账号或密码错误，请重新输入");
    }

    return message;
}

QString normalizeCardLoginMessage(const QString &rawMessage)
{
    const QString message = rawMessage.trimmed();
    const QString lower = message.toLower();

    if (message.isEmpty()
        || message.contains(QStringLiteral("卡"))
        || message.contains(QStringLiteral("账号"))
        || message.contains(QStringLiteral("用户"))
        || lower.contains(QStringLiteral("card"))
        || lower.contains(QStringLiteral("bad credentials"))
        || lower.contains(QStringLiteral("bad request")))
    {
        return QStringLiteral("刷卡登录失败，请确认卡号或联系管理员");
    }

    return message;
}

QString serviceUnavailableMessage(bool cardLogin)
{
    return cardLogin
        ? QStringLiteral("刷卡登录服务不可用，请稍后重试")
        : QStringLiteral("登录服务不可用，请稍后重试");
}

QString requestTimeoutMessage(bool cardLogin, int timeoutSeconds)
{
    return cardLogin
        ? QStringLiteral("刷卡登录请求超时(%1秒)，请稍后重试").arg(timeoutSeconds)
        : QStringLiteral("登录请求超时(%1秒)，请稍后重试").arg(timeoutSeconds);
}
}

AuthService *AuthService::m_instance = nullptr;

AuthService::AuthService(QObject *parent)
    : QObject(parent),
      m_networkManager(new QNetworkAccessManager(this)),
      m_pendingLoginReply(nullptr),
      m_loginInProgress(false),
      m_pendingAccountListReply(nullptr),
      m_accountListInProgress(false)
{
}

AuthService::~AuthService()
{
}

AuthService *AuthService::instance()
{
    if (!m_instance)
    {
        m_instance = new AuthService();
    }
    return m_instance;
}

void AuthService::login(const QString &userName, const QString &password, const QString &tenantId)
{
    if (userName.isEmpty() || password.isEmpty())
    {
        emit loginFailed("用户名或密码不能为空");
        return;
    }
    if (m_loginInProgress)
    {
        emit loginFailed("登录请求处理中，请稍候...");
        return;
    }

    // MD5 加密密码
    QString encryptedPassword = encryptPassword(password);

    // 发送登录请求
    sendLoginRequest(userName, encryptedPassword, tenantId);
}

void AuthService::loginByCard(const QString &cardNo, const QString &tenantId)
{
    const QString normalizedCardNo = cardNo.trimmed().toLower();
    if (normalizedCardNo.isEmpty())
    {
        emit loginFailed(QStringLiteral("卡号不能为空"));
        return;
    }
    if (m_loginInProgress)
    {
        emit loginFailed(QStringLiteral("登录请求处理中，请稍候..."));
        return;
    }

    QString normalizedTenantId = tenantId.trimmed();
    if (normalizedTenantId.isEmpty())
    {
        normalizedTenantId = QStringLiteral("000000");
    }

    sendCardLoginRequest(normalizedCardNo, normalizedTenantId);
}

void AuthService::fetchAccountList(const QString &tenantId)
{
    if (m_accountListInProgress)
    {
        emit accountListFailed("账号列表请求处理中，请稍候...");
        return;
    }

    QString normalizedTenantId = tenantId.trimmed();
    if (normalizedTenantId.isEmpty())
    {
        normalizedTenantId = QStringLiteral("000000");
    }

    sendAccountListRequest(normalizedTenantId);
}

void AuthService::saveToken(const QString &accessToken, const QString &refreshToken, const QJsonObject &userInfo)
{
    m_accessToken = accessToken;
    m_refreshToken = refreshToken;
    m_userInfo = userInfo;
    m_userName = userInfo.value("user_name").toString();

    // 认证成功后，统一写入全局上下文。
    // 这样 UI、日志、权限检查都从同一来源读取登录状态。
    UserInfo user;
    user.userId = userInfo.value("user_id").toString();
    user.username = userInfo.value("user_name").toString();
    user.realName = userInfo.value("real_name").toString();
    user.role = userInfo.value("role_name").toString();
    user.department = userInfo.value("department").toString();

    // 兼容后端字段不完整的情况，至少保证登录态可用。
    if (user.userId.isEmpty())
    {
        user.userId = user.username;
    }
    if (user.username.isEmpty())
    {
        user.username = m_userName;
    }

    if (user.isValid())
    {
        AppContext::instance()->setCurrentUser(user);
    }

    qDebug() << "[AuthService] Token 已保存，用户:" << m_userName;
}

QString AuthService::getAccessToken() const
{
    return m_accessToken;
}

QString AuthService::getRefreshToken() const
{
    return m_refreshToken;
}

QJsonObject AuthService::getUserInfo() const
{
    return m_userInfo;
}

QString AuthService::getUserName() const
{
    return m_userName;
}

void AuthService::clearToken()
{
    if (m_pendingLoginReply && m_pendingLoginReply->isRunning())
    {
        m_pendingLoginReply->abort();
    }
    m_pendingLoginReply = nullptr;
    if (m_pendingAccountListReply && m_pendingAccountListReply->isRunning())
    {
        m_pendingAccountListReply->abort();
    }
    m_pendingAccountListReply = nullptr;
    if (m_loginInProgress)
    {
        m_loginInProgress = false;
        emit loginStateChanged(false);
    }
    if (m_accountListInProgress)
    {
        m_accountListInProgress = false;
        emit accountListStateChanged(false);
    }

    m_accessToken.clear();
    m_refreshToken.clear();
    m_userInfo = QJsonObject();
    m_userName.clear();

    // 注销时同步清理全局登录上下文，避免“UI已登出但上下文仍登录”。
    AppContext::instance()->clearCurrentUser();

    qDebug() << "[AuthService] Token 已清除";
    emit loggedOut();
}

bool AuthService::isLoggedIn() const
{
    return !m_accessToken.isEmpty();
}

QString AuthService::getAuthorizationHeader() const
{
    if (m_accessToken.isEmpty())
    {
        return QString();
    }
    return QString("Bearer %1").arg(m_accessToken);
}

QNetworkAccessManager *AuthService::networkAccessManager() const
{
    return m_networkManager;
}

QString AuthService::encryptPassword(const QString &password) const
{
    // 使用 MD5 加密密码
    QByteArray passwordBytes = password.toUtf8();
    QByteArray hash = QCryptographicHash::hash(passwordBytes, QCryptographicHash::Md5);
    return QString(hash.toHex());
}

void AuthService::sendLoginRequest(const QString &userName, const QString &encryptedPassword, const QString &tenantId)
{
    // 从配置读取接口基础地址（使用 system/apiUrl 配置项）
    QString apiBaseUrl = ConfigManager::instance()->apiUrl().trimmed();

    // 如果配置为空，使用默认值
    if (apiBaseUrl.isEmpty())
    {
        apiBaseUrl = "http://localhost/api/kids-outage/third-api";
    }

    while (apiBaseUrl.endsWith('/'))
    {
        apiBaseUrl.chop(1);
    }

    // 拼接完整的登录接口地址
    QString loginUrl = apiBaseUrl + "/login-by-password";

    qDebug() << "[AuthService] 登录请求 URL:" << loginUrl;
    qDebug() << "[AuthService] 用户名:" << userName;

    // 构造请求体
    QJsonObject requestBody;
    requestBody["tenantId"] = tenantId;
    requestBody["userName"] = userName;
    requestBody["password"] = encryptedPassword;

    QJsonDocument doc(requestBody);
    QByteArray requestData = doc.toJson();

    // 创建网络请求
    QNetworkRequest request;
    request.setUrl(QUrl(loginUrl));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    // 添加客户端认证 Header (固定值,用于客户端认证)
    // rider:rider_secret 的 Base64 编码
    request.setRawHeader("Authorization", "Basic cmlkZXI6cmlkZXJfc2VjcmV0");

    // 发送 POST 请求
    QNetworkReply *reply = m_networkManager->post(request, requestData);
    m_pendingLoginReply = reply;
    m_loginInProgress = true;
    emit loginStateChanged(true);

    // 登录请求超时保护，避免网络异常时 UI 长时间无反馈。
    int timeoutMs = ConfigManager::instance()->connectionTimeout() * 1000;
    if (timeoutMs < 3000)
    {
        timeoutMs = 3000;
    }
    QTimer::singleShot(timeoutMs, this, [this, reply]()
                       {
        if (m_pendingLoginReply != reply || !reply->isRunning()) {
            return;
        }
        reply->setProperty("timeoutAbort", true);
        reply->abort(); });

    // 处理响应
    connect(reply, &QNetworkReply::finished, this, [this, reply, userName]()
            {
        reply->deleteLater();

        // 若该回复已经不是当前挂起请求（例如被新请求替换），忽略即可。
        if (m_pendingLoginReply != reply) {
            return;
        }

        // 检查网络错误
        if (reply->error() != QNetworkReply::NoError) {
            const bool timeoutAbort = reply->property("timeoutAbort").toBool();
            const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const QByteArray responseData = reply->readAll();
            QString errorMsg;

            if (timeoutAbort) {
                errorMsg = requestTimeoutMessage(false, ConfigManager::instance()->connectionTimeout());
            } else if (httpStatus == 400 || httpStatus == 401 || httpStatus == 403) {
                errorMsg = normalizePasswordLoginMessage(extractResponseMessage(responseData));
            } else if (httpStatus >= 500) {
                errorMsg = QStringLiteral("登录服务异常，请稍后重试");
            } else {
                errorMsg = serviceUnavailableMessage(false);
            }

            finishLoginRequest(reply);
            qDebug() << "[AuthService] 登录失败:" << errorMsg;
            emit loginFailed(errorMsg);
            return;
        }

        // 检查 HTTP 状态码，避免将网关错误页当作正常 JSON 解析。
        const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (httpStatus < 200 || httpStatus >= 300) {
            const QString errorMsg = (httpStatus == 400 || httpStatus == 401 || httpStatus == 403)
                    ? normalizePasswordLoginMessage(extractResponseMessage(reply->readAll()))
                    : QStringLiteral("登录服务异常，请稍后重试");
            finishLoginRequest(reply);
            qDebug() << "[AuthService] 登录失败:" << errorMsg;
            emit loginFailed(errorMsg);
            return;
        }

        // 读取响应数据
        QByteArray responseData = reply->readAll();
        QJsonDocument responseDoc = QJsonDocument::fromJson(responseData);

        if (responseDoc.isNull() || !responseDoc.isObject()) {
            finishLoginRequest(reply);
            qDebug() << "[AuthService] 登录失败: 响应数据格式错误";
            emit loginFailed("服务器响应格式错误");
            return;
        }

        QJsonObject responseObj = responseDoc.object();
        int code = responseObj.value("code").toInt();
        bool success = responseObj.value("success").toBool();
        QString msg = responseObj.value("msg").toString();

        qDebug() << "[AuthService] 登录响应 - code:" << code << "success:" << success << "msg:" << msg;

        // 检查响应状态
        if (!success || code != 200) {
            QString errorMsg = msg.isEmpty() ? "登录失败" : msg;
            finishLoginRequest(reply);
            qDebug() << "[AuthService] 登录失败:" << errorMsg;
            emit loginFailed(errorMsg);
            return;
        }

        // 提取 token 和用户信息
        QJsonObject data = responseObj.value("data").toObject();
        QString accessToken = data.value("access_token").toString();
        QString refreshToken = data.value("refresh_token").toString();

        if (accessToken.isEmpty()) {
            finishLoginRequest(reply);
            qDebug() << "[AuthService] 登录失败: 未返回 access_token";
            emit loginFailed("服务器未返回访问令牌");
            return;
        }

        // 保存 token 和用户信息
        saveToken(accessToken, refreshToken, data);
        finishLoginRequest(reply);

        qDebug() << "[AuthService] 登录成功，用户:" << userName;
            emit loginSuccess(data); });
}

void AuthService::sendCardLoginRequest(const QString &cardNo, const QString &tenantId)
{
    QString loginUrl = ConfigManager::instance()
            ->value("auth/cardLoginUrl", "")
            .toString()
            .trimmed();

    if (loginUrl.isEmpty())
    {
        QString apiBaseUrl = ConfigManager::instance()->apiUrl().trimmed();
        if (apiBaseUrl.isEmpty())
        {
            apiBaseUrl = QStringLiteral("http://localhost/api/kids-outage/third-api");
        }

        while (apiBaseUrl.endsWith('/'))
        {
            apiBaseUrl.chop(1);
        }

        loginUrl = apiBaseUrl + QStringLiteral("/login-by-card");
    }

    qDebug() << "[AuthService] 刷卡登录请求 URL:" << loginUrl;
    qDebug() << "[AuthService] 卡号:" << cardNo;

    QJsonObject requestBody;
    requestBody["cardNo"] = cardNo;
    requestBody["tenantId"] = tenantId;

    const QByteArray requestData = QJsonDocument(requestBody).toJson(QJsonDocument::Compact);

    QNetworkRequest request;
    request.setUrl(QUrl(loginUrl));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Authorization", "Basic cmlkZXI6cmlkZXJfc2VjcmV0");

    QNetworkReply *reply = m_networkManager->post(request, requestData);
    m_pendingLoginReply = reply;
    m_loginInProgress = true;
    emit loginStateChanged(true);

    int timeoutMs = ConfigManager::instance()->connectionTimeout() * 1000;
    if (timeoutMs < 3000)
    {
        timeoutMs = 3000;
    }
    QTimer::singleShot(timeoutMs, this, [this, reply]()
                       {
        if (m_pendingLoginReply != reply || !reply->isRunning()) {
            return;
        }
        reply->setProperty("timeoutAbort", true);
        reply->abort(); });

    connect(reply, &QNetworkReply::finished, this, [this, reply, cardNo]()
            {
        reply->deleteLater();

        if (m_pendingLoginReply != reply) {
            return;
        }

        if (reply->error() != QNetworkReply::NoError) {
            const bool timeoutAbort = reply->property("timeoutAbort").toBool();
            const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const QByteArray responseData = reply->readAll();
            QString errorMsg;

            if (timeoutAbort) {
                errorMsg = requestTimeoutMessage(true, ConfigManager::instance()->connectionTimeout());
            } else if (httpStatus == 400 || httpStatus == 401 || httpStatus == 403) {
                errorMsg = normalizeCardLoginMessage(extractResponseMessage(responseData));
            } else if (httpStatus >= 500) {
                errorMsg = QStringLiteral("刷卡登录服务异常，请稍后重试");
            } else {
                errorMsg = serviceUnavailableMessage(true);
            }

            finishLoginRequest(reply);
            qDebug() << "[AuthService] 刷卡登录失败:" << errorMsg;
            emit loginFailed(errorMsg);
            return;
        }

        const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (httpStatus < 200 || httpStatus >= 300) {
            const QString errorMsg = (httpStatus == 400 || httpStatus == 401 || httpStatus == 403)
                    ? normalizeCardLoginMessage(extractResponseMessage(reply->readAll()))
                    : QStringLiteral("刷卡登录服务异常，请稍后重试");
            finishLoginRequest(reply);
            qDebug() << "[AuthService] 刷卡登录失败:" << errorMsg;
            emit loginFailed(errorMsg);
            return;
        }

        const QByteArray responseData = reply->readAll();
        const QJsonDocument responseDoc = QJsonDocument::fromJson(responseData);
        if (responseDoc.isNull() || !responseDoc.isObject()) {
            finishLoginRequest(reply);
            qDebug() << "[AuthService] 刷卡登录失败: 响应数据格式错误";
            emit loginFailed(QStringLiteral("服务器响应格式错误"));
            return;
        }

        const QJsonObject responseObj = responseDoc.object();
        const int code = responseObj.value("code").toInt();
        const bool success = responseObj.value("success").toBool();
        const QString msg = responseObj.value("msg").toString();

        qDebug() << "[AuthService] 刷卡登录响应 - code:" << code << "success:" << success << "msg:" << msg;

        if (!success || (code != 0 && code != 200)) {
            const QString errorMsg = msg.isEmpty() ? QStringLiteral("刷卡登录失败") : msg;
            finishLoginRequest(reply);
            qDebug() << "[AuthService] 刷卡登录失败:" << errorMsg;
            emit loginFailed(errorMsg);
            return;
        }

        const QJsonObject data = responseObj.value("data").toObject();
        const QString accessToken = data.value("access_token").toString();
        const QString refreshToken = data.value("refresh_token").toString();

        if (accessToken.isEmpty()) {
            finishLoginRequest(reply);
            qDebug() << "[AuthService] 刷卡登录失败: 未返回 access_token";
            emit loginFailed(QStringLiteral("服务器未返回访问令牌"));
            return;
        }

        saveToken(accessToken, refreshToken, data);
        finishLoginRequest(reply);

        qDebug() << "[AuthService] 刷卡登录成功，卡号:" << cardNo;
        emit loginSuccess(data); });
}

void AuthService::sendAccountListRequest(const QString &tenantId)
{
    QString accountListUrl = ConfigManager::instance()
            ->value("auth/accountListUrl", "")
            .toString()
            .trimmed();

    if (accountListUrl.isEmpty())
    {
        QString apiBaseUrl = ConfigManager::instance()->apiUrl().trimmed();
        if (apiBaseUrl.isEmpty())
        {
            apiBaseUrl = "http://localhost/api/kids-outage/third-api";
        }

        while (apiBaseUrl.endsWith('/'))
        {
            apiBaseUrl.chop(1);
        }
        accountListUrl = apiBaseUrl + "/list-account";
    }

    qDebug() << "[AuthService] 账号列表请求 URL:" << accountListUrl
             << "tenantId:" << tenantId;

    QJsonObject requestBody;
    requestBody["tenantId"] = tenantId;

    const QByteArray requestData = QJsonDocument(requestBody).toJson(QJsonDocument::Compact);

    QNetworkRequest request;
    request.setUrl(QUrl(accountListUrl));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Authorization", "Basic cmlkZXI6cmlkZXJfc2VjcmV0");

    QNetworkReply *reply = m_networkManager->post(request, requestData);
    m_pendingAccountListReply = reply;
    m_accountListInProgress = true;
    emit accountListStateChanged(true);

    int timeoutMs = ConfigManager::instance()->connectionTimeout() * 1000;
    if (timeoutMs < 3000)
    {
        timeoutMs = 3000;
    }
    QTimer::singleShot(timeoutMs, this, [this, reply]()
                       {
        if (m_pendingAccountListReply != reply || !reply->isRunning()) {
            return;
        }
        reply->setProperty("timeoutAbort", true);
        reply->abort(); });

    connect(reply, &QNetworkReply::finished, this, [this, reply]()
            {
        reply->deleteLater();

        if (m_pendingAccountListReply != reply) {
            return;
        }

        if (reply->error() != QNetworkReply::NoError) {
            const bool timeoutAbort = reply->property("timeoutAbort").toBool();
            const QString errorMsg = timeoutAbort
                    ? QString("账号列表请求超时(%1秒)").arg(ConfigManager::instance()->connectionTimeout())
                    : QString("网络错误: %1").arg(reply->errorString());
            finishAccountListRequest(reply);
            emit accountListFailed(errorMsg);
            return;
        }

        const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (httpStatus < 200 || httpStatus >= 300) {
            finishAccountListRequest(reply);
            emit accountListFailed(QString("HTTP状态异常: %1").arg(httpStatus));
            return;
        }

        QString errorMessage;
        const QStringList accounts = parseAccountList(reply->readAll(), &errorMessage);
        finishAccountListRequest(reply);

        if (accounts.isEmpty()) {
            if (errorMessage.trimmed().isEmpty()) {
                errorMessage = QStringLiteral("未获取到可选账号");
            }
            emit accountListFailed(errorMessage);
            return;
        }

        emit accountListReady(accounts); });
}

void AuthService::finishLoginRequest(QNetworkReply *reply)
{
    if (m_pendingLoginReply == reply)
    {
        m_pendingLoginReply = nullptr;
    }
    if (m_loginInProgress)
    {
        m_loginInProgress = false;
        emit loginStateChanged(false);
    }
}

void AuthService::finishAccountListRequest(QNetworkReply *reply)
{
    if (m_pendingAccountListReply == reply)
    {
        m_pendingAccountListReply = nullptr;
    }
    if (m_accountListInProgress)
    {
        m_accountListInProgress = false;
        emit accountListStateChanged(false);
    }
}

QStringList AuthService::parseAccountList(const QByteArray &responseData, QString *errorMessage) const
{
    QJsonParseError parseError;
    const QJsonDocument responseDoc = QJsonDocument::fromJson(responseData, &parseError);
    if (responseDoc.isNull())
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("服务器响应格式错误: %1").arg(parseError.errorString());
        }
        return QStringList();
    }

    QJsonArray array;
    if (responseDoc.isArray())
    {
        array = responseDoc.array();
    }
    else if (responseDoc.isObject())
    {
        const QJsonObject responseObj = responseDoc.object();
        const int code = responseObj.value("code").toInt();
        const bool success = responseObj.value("success").toBool() || code == 200 || code == 0;
        if (!success)
        {
            if (errorMessage)
            {
                *errorMessage = responseObj.value("msg").toString().trimmed();
                if (errorMessage->isEmpty())
                {
                    *errorMessage = QStringLiteral("账号列表接口返回失败");
                }
            }
            return QStringList();
        }

        if (responseObj.value("data").isArray())
        {
            array = responseObj.value("data").toArray();
        }
        else if (responseObj.value("data").isObject())
        {
            const QJsonObject dataObj = responseObj.value("data").toObject();
            if (dataObj.value("list").isArray())
            {
                array = dataObj.value("list").toArray();
            }
            else if (dataObj.value("records").isArray())
            {
                array = dataObj.value("records").toArray();
            }
            else if (dataObj.value("rows").isArray())
            {
                array = dataObj.value("rows").toArray();
            }
            else if (dataObj.value("items").isArray())
            {
                array = dataObj.value("items").toArray();
            }
        }
        else if (responseObj.value("list").isArray())
        {
            array = responseObj.value("list").toArray();
        }
    }

    QStringList accounts;
    for (const QJsonValue &value : array)
    {
        QString account;
        if (value.isString())
        {
            account = value.toString().trimmed();
        }
        else if (value.isObject())
        {
            const QJsonObject obj = value.toObject();
            const char *keys[] = {
                "userName", "user_name", "username", "account",
                "accountName", "loginName", "name", "userCode", "accountNo"
            };
            for (const char *key : keys)
            {
                account = obj.value(QLatin1String(key)).toString().trimmed();
                if (!account.isEmpty())
                {
                    break;
                }
            }
        }

        if (!account.isEmpty() && !accounts.contains(account))
        {
            accounts.append(account);
        }
    }

    if (accounts.isEmpty() && errorMessage)
    {
        *errorMessage = QStringLiteral("未获取到可选账号");
    }
    return accounts;
}
