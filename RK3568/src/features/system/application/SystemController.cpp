/**
 * @file SystemController.cpp
 * @brief 系统设置页面控制器实现
 */
#include "features/system/application/SystemController.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSerialPortInfo>
#include <QTimer>
#include <QDebug>

#include "core/ConfigManager.h"
#include "features/auth/infra/password/authservice.h"

namespace
{
int parseBusinessStationNo(const QString &text, int fallback = 0)
{
    bool ok = false;
    const int value = text.trimmed().toInt(&ok);
    return (ok && value >= 0) ? value : fallback;
}

QString firstNonEmptyValue(const QJsonObject &obj, const char *const *keys, int count)
{
    for (int index = 0; index < count; ++index)
    {
        const QString value = obj.value(QLatin1String(keys[index])).toString().trimmed();
        if (!value.isEmpty())
        {
            return value;
        }
    }
    return QString();
}

SystemIdentityUserDto parseUserItem(const QJsonValue &value)
{
    SystemIdentityUserDto user;
    if (!value.isObject())
    {
        if (value.isString())
        {
            user.account = value.toString().trimmed();
        }
        return user;
    }

    const QJsonObject obj = value.toObject();
    static const char *const kUserIdKeys[] = {"userId", "user_id", "id"};
    static const char *const kAccountKeys[] = {
        "account", "userName", "user_name", "username", "loginName", "name", "userCode", "accountNo"
    };
    static const char *const kRealNameKeys[] = {
        "realName", "real_name", "name", "nickName", "nick_name", "user_name", "userName"
    };
    static const char *const kCardNoKeys[] = {"cardNo", "card_no"};
    static const char *const kFingerprintKeys[] = {
        "fingerprint", "fingerPrint", "fingerTemplate", "finger_template", "fingerData", "finger_data"
    };
    static const char *const kRoleNameKeys[] = {"roleName", "role_name", "roleAlias", "role_alias"};

    user.userId = firstNonEmptyValue(obj, kUserIdKeys, sizeof(kUserIdKeys) / sizeof(kUserIdKeys[0]));
    user.account = firstNonEmptyValue(obj, kAccountKeys, sizeof(kAccountKeys) / sizeof(kAccountKeys[0]));
    user.realName = firstNonEmptyValue(obj, kRealNameKeys, sizeof(kRealNameKeys) / sizeof(kRealNameKeys[0]));
    user.cardNo = firstNonEmptyValue(obj, kCardNoKeys, sizeof(kCardNoKeys) / sizeof(kCardNoKeys[0]));
    user.fingerprint = firstNonEmptyValue(obj, kFingerprintKeys, sizeof(kFingerprintKeys) / sizeof(kFingerprintKeys[0]));
    user.roleName = firstNonEmptyValue(obj, kRoleNameKeys, sizeof(kRoleNameKeys) / sizeof(kRoleNameKeys[0]));

    if (user.realName.isEmpty())
    {
        user.realName = user.account;
    }

    return user;
}
}

SystemController::SystemController(QObject *parent)
    : QObject(parent)
    , m_networkManager(AuthService::instance()->networkAccessManager())
    , m_pendingUserListReply(nullptr)
    , m_pendingUpdateCardReply(nullptr)
    , m_userListLoading(false)
    , m_cardUpdateInProgress(false)
{
}

SystemSettingsDto SystemController::loadSettings() const
{
    ConfigManager *config = ConfigManager::instance();
    SystemSettingsDto dto;
    dto.homeUrl = config->homeUrl();
    dto.apiUrl = config->apiUrl();
    dto.stationId = config->stationId();
    dto.tenantCode = config->tenantCode();
    dto.keySerialPort = config->keySerialPort();
    dto.cardSerialPort = config->cardSerialPort();
    dto.baudRate = config->baudRate();
    dto.dataBits = config->dataBits();
    return dto;
}

void SystemController::saveSettings(const SystemSettingsDto &settings) const
{
    ConfigManager *config = ConfigManager::instance();
    config->setHomeUrl(settings.homeUrl);
    config->setApiUrl(settings.apiUrl);
    config->setStationId(settings.stationId);
    // Keep legacy provisioning config in sync while system/stationId becomes the single UI truth.
    config->setValue(QStringLiteral("key/backendStationNo"),
                     parseBusinessStationNo(settings.stationId, 0));
    config->setTenantCode(settings.tenantCode);
    config->setKeySerialPort(settings.keySerialPort);
    config->setCardSerialPort(settings.cardSerialPort);
    config->setBaudRate(settings.baudRate);
    config->setDataBits(settings.dataBits);
    config->sync();
}

QStringList SystemController::availableSerialPorts() const
{
    QStringList list;
    for (const QSerialPortInfo &info : QSerialPortInfo::availablePorts()) {
        list << info.portName();
    }

#ifdef Q_OS_LINUX
    // 补充硬件固定局路径：即使暂未注册，也让用户能在下拉中看到
    // /dev/ttyS4=钥匙柜, /dev/ttyS3=读卡器（见 CODEMAP 串口红线）
    static const QStringList kLinuxHints = {
        "/dev/ttyS3", "/dev/ttyS4",
        "/dev/ttyUSB0", "/dev/ttyUSB1",
        "/dev/ttyACM0"
    };
    for (const QString &path : kLinuxHints) {
        if (!list.contains(path)) {
            list << path;
        }
    }
#endif

    return list;
}

QList<SystemIdentityUserDto> SystemController::users() const
{
    return m_users;
}

void SystemController::requestUserList()
{
    if (m_userListLoading)
    {
        return;
    }

    const QString requestUrl = resolveUserListUrl();
    if (requestUrl.isEmpty())
    {
        emit userListLoadFailed(QStringLiteral("未配置用户列表接口"));
        return;
    }

    QNetworkRequest request{QUrl(requestUrl)};
    applyCommonHeaders(&request);
    qDebug() << "[SystemController] 用户列表请求 URL:" << requestUrl
             << "hasToken:" << !AuthService::instance()->getAccessToken().trimmed().isEmpty();

    QNetworkReply *reply = m_networkManager->post(request, QByteArray());
    m_pendingUserListReply = reply;
    m_userListLoading = true;
    emit userListLoadingChanged(true);

    int timeoutMs = ConfigManager::instance()->connectionTimeout() * 1000;
    if (timeoutMs < 3000)
    {
        timeoutMs = 3000;
    }

    QTimer::singleShot(timeoutMs, this, [this, reply]()
                       {
        if (m_pendingUserListReply != reply || !reply->isRunning()) {
            return;
        }
        reply->setProperty("timeoutAbort", true);
        reply->abort(); });

    connect(reply, &QNetworkReply::finished, this, [this, reply]()
            {
        reply->deleteLater();

        if (m_pendingUserListReply != reply) {
            return;
        }

        const QByteArray responseData = reply->readAll();

        if (reply->error() != QNetworkReply::NoError) {
            const bool timeoutAbort = reply->property("timeoutAbort").toBool();
            const QString errorMessage = timeoutAbort
                    ? QStringLiteral("用户列表请求超时")
                    : extractApiErrorMessage(responseData,
                                             QStringLiteral("加载用户列表失败：%1").arg(reply->errorString()));
            finishUserListRequest(reply);
            emit userListLoadFailed(errorMessage);
            return;
        }

        const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (httpStatus < 200 || httpStatus >= 300) {
            finishUserListRequest(reply);
            emit userListLoadFailed(extractApiErrorMessage(
                responseData, QStringLiteral("用户列表接口返回异常：HTTP %1").arg(httpStatus)));
            return;
        }

        QString errorMessage;
        const QList<SystemIdentityUserDto> loadedUsers = parseUserList(responseData, &errorMessage);
        finishUserListRequest(reply);

        if (loadedUsers.isEmpty()) {
            emit userListLoadFailed(errorMessage.isEmpty()
                                        ? QStringLiteral("未获取到可采集用户")
                                        : errorMessage);
            return;
        }

        m_users = loadedUsers;
        emit userListChanged(); });
}

void SystemController::updateUserCardNo(const QString &userId, const QString &cardNo)
{
    submitUserCardNo(userId, cardNo);
}

void SystemController::clearUserCardNo(const QString &userId)
{
    submitUserCardNo(userId, QString());
}

bool SystemController::isUserListLoading() const
{
    return m_userListLoading;
}

bool SystemController::isCardUpdateInProgress() const
{
    return m_cardUpdateInProgress;
}

QString SystemController::resolveTenantId() const
{
    QString tenantId = ConfigManager::instance()->tenantCode().trimmed();
    if (tenantId.isEmpty())
    {
        tenantId = QStringLiteral("000000");
    }
    return tenantId;
}

QString SystemController::resolveUserListUrl() const
{
    QString requestUrl = ConfigManager::instance()->value(QStringLiteral("system/userListUrl"), QString()).toString().trimmed();
    if (!requestUrl.isEmpty())
    {
        return requestUrl;
    }

    QString apiBaseUrl = ConfigManager::instance()->apiUrl().trimmed();
    if (apiBaseUrl.isEmpty())
    {
        apiBaseUrl = QStringLiteral("http://localhost/api/kids-outage/third-api");
    }
    while (apiBaseUrl.endsWith('/'))
    {
        apiBaseUrl.chop(1);
    }
    return apiBaseUrl + QStringLiteral("/list-user");
}

QString SystemController::resolveUpdateCardNoUrl() const
{
    QString requestUrl = ConfigManager::instance()->value(QStringLiteral("system/updateCardNoUrl"), QString()).toString().trimmed();
    if (!requestUrl.isEmpty())
    {
        return requestUrl;
    }

    QString apiBaseUrl = ConfigManager::instance()->apiUrl().trimmed();
    if (apiBaseUrl.isEmpty())
    {
        apiBaseUrl = QStringLiteral("http://localhost/api/kids-outage/third-api");
    }
    while (apiBaseUrl.endsWith('/'))
    {
        apiBaseUrl.chop(1);
    }
    return apiBaseUrl + QStringLiteral("/update-card-no");
}

void SystemController::applyCommonHeaders(QNetworkRequest *request) const
{
    if (!request)
    {
        return;
    }

    request->setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request->setRawHeader("kids-requested-with", "KidsHttpRequest");

    const QString accessToken = AuthService::instance()->getAccessToken().trimmed();
    if (!accessToken.isEmpty())
    {
        // 第三方业务接口当前按后端文档使用 kids-auth。
        // 该头要求固定前缀 "bearer " + accessToken。
        request->setRawHeader("kids-auth", QByteArray("bearer ") + accessToken.toUtf8());
    }
}

QString SystemController::extractApiErrorMessage(const QByteArray &responseData, const QString &fallbackMessage) const
{
    QJsonParseError parseError;
    const QJsonDocument responseDoc = QJsonDocument::fromJson(responseData, &parseError);
    if (!responseDoc.isNull() && responseDoc.isObject())
    {
        const QJsonObject responseObj = responseDoc.object();
        const QStringList messageKeys = {
            QStringLiteral("msg"),
            QStringLiteral("message"),
            QStringLiteral("error_description"),
            QStringLiteral("error")
        };
        for (const QString &key : messageKeys)
        {
            const QString message = responseObj.value(key).toString().trimmed();
            if (!message.isEmpty())
            {
                return message;
            }
        }
    }

    const QString plainText = QString::fromUtf8(responseData).trimmed();
    if (!plainText.isEmpty())
    {
        return plainText;
    }

    return fallbackMessage;
}

QList<SystemIdentityUserDto> SystemController::parseUserList(const QByteArray &responseData, QString *errorMessage) const
{
    QJsonParseError parseError;
    const QJsonDocument responseDoc = QJsonDocument::fromJson(responseData, &parseError);
    if (responseDoc.isNull())
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("用户列表响应格式错误：%1").arg(parseError.errorString());
        }
        return QList<SystemIdentityUserDto>();
    }

    QJsonArray userArray;
    if (responseDoc.isArray())
    {
        userArray = responseDoc.array();
    }
    else if (responseDoc.isObject())
    {
        const QJsonObject responseObj = responseDoc.object();
        const int code = responseObj.value(QStringLiteral("code")).toInt();
        const bool success = responseObj.value(QStringLiteral("success")).toBool() || code == 0 || code == 200;
        if (!success)
        {
            if (errorMessage)
            {
                *errorMessage = responseObj.value(QStringLiteral("msg")).toString().trimmed();
                if (errorMessage->isEmpty())
                {
                    *errorMessage = QStringLiteral("用户列表接口返回失败");
                }
            }
            return QList<SystemIdentityUserDto>();
        }

        if (responseObj.value(QStringLiteral("data")).isArray())
        {
            userArray = responseObj.value(QStringLiteral("data")).toArray();
        }
        else if (responseObj.value(QStringLiteral("data")).isObject())
        {
            const QJsonObject dataObj = responseObj.value(QStringLiteral("data")).toObject();
            if (dataObj.value(QStringLiteral("list")).isArray())
            {
                userArray = dataObj.value(QStringLiteral("list")).toArray();
            }
            else if (dataObj.value(QStringLiteral("records")).isArray())
            {
                userArray = dataObj.value(QStringLiteral("records")).toArray();
            }
            else if (dataObj.value(QStringLiteral("rows")).isArray())
            {
                userArray = dataObj.value(QStringLiteral("rows")).toArray();
            }
            else if (dataObj.value(QStringLiteral("items")).isArray())
            {
                userArray = dataObj.value(QStringLiteral("items")).toArray();
            }
        }
        else if (responseObj.value(QStringLiteral("list")).isArray())
        {
            userArray = responseObj.value(QStringLiteral("list")).toArray();
        }
    }

    QList<SystemIdentityUserDto> result;
    for (const QJsonValue &value : userArray)
    {
        const SystemIdentityUserDto user = parseUserItem(value);
        if (user.isValid())
        {
            result.append(user);
        }
    }

    if (result.isEmpty() && errorMessage)
    {
        *errorMessage = QStringLiteral("用户列表为空，或接口未返回用户详情字段");
    }
    return result;
}

void SystemController::finishUserListRequest(QNetworkReply *reply)
{
    if (m_pendingUserListReply == reply)
    {
        m_pendingUserListReply = nullptr;
    }
    if (m_userListLoading)
    {
        m_userListLoading = false;
        emit userListLoadingChanged(false);
    }
}

void SystemController::finishCardUpdateRequest(QNetworkReply *reply)
{
    if (m_pendingUpdateCardReply == reply)
    {
        m_pendingUpdateCardReply = nullptr;
    }
    if (m_cardUpdateInProgress)
    {
        m_cardUpdateInProgress = false;
        emit cardNoUpdateStateChanged(false);
    }
}

void SystemController::submitUserCardNo(const QString &userId, const QString &cardNo)
{
    const QString normalizedUserId = userId.trimmed();
    const QString normalizedCardNo = cardNo.trimmed().toUpper();
    if (normalizedUserId.isEmpty())
    {
        emit cardNoUpdateFailed(QStringLiteral("用户ID为空，无法保存卡号"));
        return;
    }
    if (m_cardUpdateInProgress)
    {
        emit cardNoUpdateFailed(QStringLiteral("正在保存卡号，请稍候"));
        return;
    }

    const QString requestUrl = resolveUpdateCardNoUrl();
    if (requestUrl.isEmpty())
    {
        emit cardNoUpdateFailed(QStringLiteral("未配置卡号保存接口"));
        return;
    }

    const QJsonObject requestBody{
        {QStringLiteral("userId"), normalizedUserId},
        {QStringLiteral("cardNo"), normalizedCardNo}
    };

    QNetworkRequest request{QUrl(requestUrl)};
    applyCommonHeaders(&request);
    qDebug() << "[SystemController] 保存卡号请求 URL:" << requestUrl
             << "userId:" << normalizedUserId
             << "cardNo:" << normalizedCardNo
             << "hasToken:" << !AuthService::instance()->getAccessToken().trimmed().isEmpty();

    QNetworkReply *reply = m_networkManager->post(
        request, QJsonDocument(requestBody).toJson(QJsonDocument::Compact));
    m_pendingUpdateCardReply = reply;
    m_cardUpdateInProgress = true;
    emit cardNoUpdateStateChanged(true);

    int timeoutMs = ConfigManager::instance()->connectionTimeout() * 1000;
    if (timeoutMs < 3000)
    {
        timeoutMs = 3000;
    }

    QTimer::singleShot(timeoutMs, this, [this, reply]()
                       {
        if (m_pendingUpdateCardReply != reply || !reply->isRunning()) {
            return;
        }
        reply->setProperty("timeoutAbort", true);
        reply->abort(); });

    connect(reply, &QNetworkReply::finished, this, [this, reply, normalizedUserId, normalizedCardNo]()
            {
        reply->deleteLater();

        if (m_pendingUpdateCardReply != reply) {
            return;
        }

        const QByteArray responseData = reply->readAll();

        if (reply->error() != QNetworkReply::NoError) {
            const bool timeoutAbort = reply->property("timeoutAbort").toBool();
            const QString errorMessage = timeoutAbort
                    ? QStringLiteral("保存卡号请求超时")
                    : extractApiErrorMessage(responseData,
                                             QStringLiteral("保存卡号失败：%1").arg(reply->errorString()));
            finishCardUpdateRequest(reply);
            emit cardNoUpdateFailed(errorMessage);
            return;
        }

        const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (httpStatus < 200 || httpStatus >= 300) {
            finishCardUpdateRequest(reply);
            emit cardNoUpdateFailed(extractApiErrorMessage(
                responseData, QStringLiteral("保存卡号接口返回异常：HTTP %1").arg(httpStatus)));
            return;
        }

        const QJsonDocument responseDoc = QJsonDocument::fromJson(responseData);
        if (responseDoc.isNull() || !responseDoc.isObject()) {
            finishCardUpdateRequest(reply);
            emit cardNoUpdateFailed(QStringLiteral("保存卡号响应格式错误"));
            return;
        }

        const QJsonObject responseObj = responseDoc.object();
        const int code = responseObj.value(QStringLiteral("code")).toInt();
        const bool success = responseObj.value(QStringLiteral("success")).toBool();
        const QString message = responseObj.value(QStringLiteral("msg")).toString().trimmed();

        if (!success || (code != 0 && code != 200)) {
            finishCardUpdateRequest(reply);
            emit cardNoUpdateFailed(message.isEmpty() ? QStringLiteral("保存卡号失败") : message);
            return;
        }

        for (SystemIdentityUserDto &user : m_users)
        {
            if (user.userId == normalizedUserId)
            {
                user.cardNo = normalizedCardNo;
                break;
            }
        }

        finishCardUpdateRequest(reply);
        // 页面会在 cardNoUpdated 回调里刷新本地表格，
        // 这里不再额外发 userListChanged，避免覆盖成功提示。
        emit cardNoUpdated(normalizedUserId, normalizedCardNo); });
}
