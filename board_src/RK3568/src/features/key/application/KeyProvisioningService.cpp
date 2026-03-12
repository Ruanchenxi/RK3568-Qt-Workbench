/**
 * @file KeyProvisioningService.cpp
 * @brief 初始化/RFID 后端取数与 payload 组装服务实现
 */
#include "features/key/application/KeyProvisioningService.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

#include "core/ConfigManager.h"
#include "features/auth/infra/password/AuthServiceAdapter.h"
#include "features/key/protocol/InitPayloadEncoder.h"
#include "features/key/protocol/RfidPayloadEncoder.h"

KeyProvisioningService::KeyProvisioningService(IAuthService *auth, QObject *parent)
    : QObject(parent)
    , m_auth(auth)
    , m_network(new QNetworkAccessManager(this))
    , m_requestInFlight(false)
{
    if (!m_auth) {
        m_auth = new AuthServiceAdapter(this);
    }
}

void KeyProvisioningService::requestInitPayload(int stationNo)
{
    startRequest(RequestKind::Init, stationNo);
}

void KeyProvisioningService::requestRfidPayload(int stationNo)
{
    startRequest(RequestKind::Rfid, stationNo);
}

QString KeyProvisioningService::endpointUrl(RequestKind kind) const
{
    QString apiBase = ConfigManager::instance()->apiUrl().trimmed();
    while (apiBase.endsWith('/')) {
        apiBase.chop(1);
    }
    if (kind == RequestKind::Init) {
        return apiBase + QStringLiteral("/package-data");
    }
    return apiBase + QStringLiteral("/rfid-data");
}

void KeyProvisioningService::startRequest(RequestKind kind, int stationNo)
{
    if (m_requestInFlight) {
        emit requestFailed(QStringLiteral("后端取数进行中，请稍后再试"));
        return;
    }

    const QString accessToken = m_auth->accessToken().trimmed();
    if (accessToken.isEmpty()) {
        emit requestFailed(QStringLiteral("未登录或 accessToken 为空，无法获取钥匙数据"));
        return;
    }

    const QUrl url = QUrl::fromUserInput(endpointUrl(kind));
    if (!url.isValid() || url.scheme().isEmpty() || url.host().isEmpty()) {
        emit requestFailed(QStringLiteral("钥匙数据接口地址无效: %1").arg(url.toString()));
        return;
    }

    QJsonObject bodyObj;
    bodyObj.insert(QStringLiteral("stationNo"), stationNo);
    const QByteArray body = QJsonDocument(bodyObj).toJson(QJsonDocument::Compact);

    const QString intro = (kind == RequestKind::Init)
            ? QStringLiteral("初始化钥匙，正在获取初始化数据。。。")
            : QStringLiteral("下载RFID到钥匙，正在获取RFID数据。。。");
    emit logAppended(QStringLiteral("[%1] 收到响应数据:\n%2").arg(nowText(), intro));
    emit logAppended(QStringLiteral("[%1] 发送请求数据:\n%2")
                         .arg(nowText(), prettyJson(body)));

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("kids-auth",
                         QStringLiteral("bearer %1").arg(accessToken).toUtf8());
    request.setRawHeader("kids-requested-with", "KidsHttpRequest");

    m_requestInFlight = true;
    QNetworkReply *reply = m_network->post(request, body);
    connect(reply, &QNetworkReply::finished, this, [this, reply, kind, stationNo]() {
        finishRequest(reply, kind, stationNo);
    });
}

void KeyProvisioningService::finishRequest(QNetworkReply *reply, RequestKind kind, int stationNo)
{
    const QByteArray responseBody = reply->readAll();
    emit logAppended(QStringLiteral("[%1] 收到响应数据:\n%2")
                         .arg(nowText(), prettyJson(responseBody)));
    m_requestInFlight = false;

    const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (reply->error() != QNetworkReply::NoError || httpStatus < 200 || httpStatus >= 300) {
        const QString reason = reply->errorString().trimmed().isEmpty()
                ? QStringLiteral("HTTP %1").arg(httpStatus)
                : reply->errorString();
        reply->deleteLater();
        emit requestFailed(QStringLiteral("钥匙数据接口请求失败：%1").arg(reason));
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(responseBody, &parseError);
    if (doc.isNull() || !doc.isObject()) {
        reply->deleteLater();
        emit requestFailed(QStringLiteral("钥匙数据接口响应不是合法 JSON：%1").arg(parseError.errorString()));
        return;
    }

    const QJsonObject root = doc.object();
    const bool success = root.value("success").toBool(false)
            || root.value("code").toInt() == 200
            || root.value("code").toInt() == 0;
    if (!success || !root.value("data").isObject()) {
        reply->deleteLater();
        emit requestFailed(QStringLiteral("钥匙数据接口返回失败：%1").arg(root.value("msg").toString()));
        return;
    }

    const QJsonObject dataObj = root.value("data").toObject();
    if (kind == RequestKind::Init) {
        const InitPayloadEncoder::EncodeResult encoded = InitPayloadEncoder::encode(dataObj);
        if (!encoded.ok) {
            reply->deleteLater();
            emit requestFailed(QStringLiteral("初始化 payload 编码失败：%1").arg(encoded.errorMsg));
            return;
        }
        for (const QString &line : encoded.fieldLog) {
            emit logAppended(QStringLiteral("[%1] %2").arg(nowText(), line));
        }
        emit initPayloadReady(encoded.payload);
    } else {
        const RfidPayloadEncoder::EncodeResult encoded = RfidPayloadEncoder::encode(dataObj);
        if (!encoded.ok) {
            reply->deleteLater();
            emit requestFailed(QStringLiteral("RFID payload 编码失败：%1").arg(encoded.errorMsg));
            return;
        }
        for (const QString &line : encoded.fieldLog) {
            emit logAppended(QStringLiteral("[%1] %2").arg(nowText(), line));
        }
        emit rfidPayloadReady(encoded.payload);
    }

    reply->deleteLater();
    Q_UNUSED(stationNo);
}

QString KeyProvisioningService::nowText() const
{
    return QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
}

QString KeyProvisioningService::prettyJson(const QByteArray &json) const
{
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &parseError);
    if (doc.isNull()) {
        return QString::fromUtf8(json);
    }
    return QString::fromUtf8(doc.toJson(QJsonDocument::Indented)).trimmed();
}
