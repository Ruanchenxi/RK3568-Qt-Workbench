/**
 * @file TicketReturnHttpClient.cpp
 * @brief 钥匙回传 HTTP 客户端实现
 */
#include "features/key/application/TicketReturnHttpClient.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

#include "core/ConfigManager.h"

TicketReturnHttpClient::TicketReturnHttpClient(QObject *parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
{
}

bool TicketReturnHttpClient::isConfigured() const
{
    const QUrl url = QUrl::fromUserInput(configuredUrl());
    return url.isValid() && !url.scheme().isEmpty() && !url.host().isEmpty();
}

QString TicketReturnHttpClient::configuredUrl() const
{
    const QString explicitUrl = ConfigManager::instance()->value("ticket/httpReturnUrl", "").toString().trimmed();
    if (!explicitUrl.isEmpty()) {
        return explicitUrl;
    }

    const QString apiBase = ConfigManager::instance()->apiUrl().trimmed();
    if (apiBase.isEmpty()) {
        return QString();
    }

    if (apiBase.endsWith('/')) {
        return apiBase + QStringLiteral("finish-step-batch");
    }
    return apiBase + QStringLiteral("/finish-step-batch");
}

void TicketReturnHttpClient::uploadReturnPayload(const QJsonObject &payload)
{
    const QString taskId = payload.value("taskId").toString();
    const int stationNo = payload.value("stationNo").toInt();
    const QString urlText = configuredUrl();
    const QUrl url = QUrl::fromUserInput(urlText);

    emit logAppended(QStringLiteral("[%1] 发送请求数据：\n正在回传 %2 号座钥匙任务数据。。。")
                         .arg(nowText())
                         .arg(stationNo));

    if (!isConfigured()) {
        const QString reason = QStringLiteral("回传接口未配置或地址无效");
        emit logAppended(QStringLiteral("[%1] 收到响应数据：\n%2").arg(nowText(), reason));
        emit uploadFailed(taskId, reason);
        return;
    }

    const QByteArray body = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    emit logAppended(QStringLiteral("[%1] 发送请求数据：\n%2")
                         .arg(nowText(), prettyJson(body)));

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Accept", "application/json");

    QNetworkReply *reply = m_network->post(request, body);
    connect(reply, &QNetworkReply::finished, this, [this, reply, taskId]() {
        const QByteArray responseBody = reply->readAll();
        const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QString responseText = responseBody.isEmpty()
                ? QStringLiteral("<empty>")
                : prettyJson(responseBody);
        emit logAppended(QStringLiteral("[%1] 收到响应数据：\n%2").arg(nowText(), responseText));

        QString reason;
        bool success = false;
        if (reply->error() == QNetworkReply::NoError && httpStatus >= 200 && httpStatus < 300) {
            QJsonParseError parseError;
            const QJsonDocument doc = QJsonDocument::fromJson(responseBody, &parseError);
            if (!doc.isNull() && doc.isObject()) {
                const QJsonObject obj = doc.object();
                success = obj.value("success").toBool(false) || obj.value("code").toInt() == 200;
                if (!success) {
                    reason = obj.value("msg").toString();
                }
            } else {
                // 对于未来可能返回纯文本的接口，2xx 视为成功。
                success = true;
            }
        } else {
            reason = reply->errorString();
            if (reason.trimmed().isEmpty()) {
                reason = QStringLiteral("HTTP %1").arg(httpStatus);
            }
        }

        reply->deleteLater();
        if (success) {
            emit uploadSucceeded(taskId);
            return;
        }

        if (reason.trimmed().isEmpty()) {
            reason = QStringLiteral("回传接口返回失败");
        }
        emit uploadFailed(taskId, reason);
    });
}

QString TicketReturnHttpClient::nowText() const
{
    return QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
}

QString TicketReturnHttpClient::prettyJson(const QByteArray &body) const
{
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(body, &parseError);
    if (doc.isNull()) {
        return QString::fromUtf8(body);
    }
    return QString::fromUtf8(doc.toJson(QJsonDocument::Indented)).trimmed();
}
