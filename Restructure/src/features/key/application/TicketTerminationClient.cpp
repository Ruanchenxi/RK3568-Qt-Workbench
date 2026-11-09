#include "features/key/application/TicketTerminationClient.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrlQuery>

#include "core/ConfigManager.h"

namespace
{
QString timestamp()
{
    return QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd hh:mm:ss.zzz"));
}

QString responsePreview(const QByteArray &body)
{
    const QString text = QString::fromUtf8(body).trimmed();
    return text.size() <= 1000 ? text : text.left(1000) + QStringLiteral("...");
}
}

TicketTerminationClient::TicketTerminationClient(QObject *parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
{
}

void TicketTerminationClient::terminate(const QString &taskId, const QString &accessToken)
{
    const QString normalizedTaskId = taskId.trimmed();
    const QString normalizedToken = accessToken.trimmed();
    if (normalizedTaskId.isEmpty()) {
        emit terminationFailed(taskId, QStringLiteral("taskId 为空"));
        return;
    }
    if (normalizedToken.isEmpty()) {
        emit terminationFailed(normalizedTaskId, QStringLiteral("管理员登录令牌为空"));
        return;
    }

    QString apiBase = ConfigManager::instance()->apiUrl().trimmed();
    if (apiBase.endsWith(QLatin1String("/third-api"))) {
        apiBase.chop(10);
    }
    while (apiBase.endsWith(QLatin1Char('/'))) {
        apiBase.chop(1);
    }

    QUrl url(apiBase + QStringLiteral("/task/termination"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("id"), normalizedTaskId);
    query.addQueryItem(QStringLiteral("reason"), QStringLiteral("管理员手工删除钥匙任务"));
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("kids-auth", QStringLiteral("bearer %1").arg(normalizedToken).toUtf8());
    request.setRawHeader("kids-requested-with", "KidsHttpRequest");

    emit logAppended(QStringLiteral("[%1] POST %2\ntaskId=%3\nrequest={}")
                     .arg(timestamp(), url.toString(), normalizedTaskId));

    QNetworkReply *reply = m_network->post(request, QByteArrayLiteral("{}"));
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, normalizedTaskId, url]() {
        const QByteArray body = reply->readAll();
        const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const bool transportOk = reply->error() == QNetworkReply::NoError
                && httpStatus >= 200 && httpStatus < 300;
        QString errorText = reply->errorString();
        bool businessOk = false;

        if (transportOk) {
            QJsonParseError parseError;
            const QJsonDocument document = QJsonDocument::fromJson(body, &parseError);
            if (parseError.error == QJsonParseError::NoError && document.isObject()) {
                const QJsonObject object = document.object();
                businessOk = object.value(QStringLiteral("success")).toBool(false)
                        || object.value(QStringLiteral("code")).toInt(-1) == 200;
                if (!businessOk) {
                    const QString message = object.value(QStringLiteral("message")).toString().trimmed();
                    if (!message.isEmpty()) {
                        errorText = message;
                    }
                }
            } else if (body.trimmed().isEmpty()) {
                businessOk = true;
            } else {
                errorText = QStringLiteral("响应格式异常（非 JSON）");
            }
        }

        const QString preview = responsePreview(body);
        emit logAppended(QStringLiteral("[%1] POST %2 -> HTTP %3 %4\ntaskId=%5\nresponse=%6")
                         .arg(timestamp(),
                              url.toString(),
                              QString::number(httpStatus),
                              businessOk ? QStringLiteral("SUCCESS") : QStringLiteral("FAILED"),
                              normalizedTaskId,
                              preview.isEmpty() ? QStringLiteral("<empty>") : preview));

        reply->deleteLater();
        if (businessOk) {
            emit terminationSucceeded(normalizedTaskId);
            return;
        }
        if (errorText.trimmed().isEmpty()) {
            errorText = transportOk
                    ? QStringLiteral("终止系统票业务返回失败")
                    : QStringLiteral("网络或接口调用失败，HTTP %1").arg(httpStatus);
        }
        emit terminationFailed(normalizedTaskId, errorText);
    });
}
