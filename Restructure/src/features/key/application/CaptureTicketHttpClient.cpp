#include "features/key/application/CaptureTicketHttpClient.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>

#include "core/ConfigManager.h"
#include "shared/contracts/IAuthService.h"

CaptureTicketHttpClient::CaptureTicketHttpClient(
        IAuthService *auth, QObject *parent)
    : QObject(parent)
    , m_auth(auth)
    , m_network(new QNetworkAccessManager(this))
{
    Q_ASSERT(m_auth != nullptr);
}

void CaptureTicketHttpClient::claim(int stationNo, int operationId)
{
    QJsonObject body;
    body.insert(QStringLiteral("stationNo"), stationNo);
    post(true, operationId, QString(), QStringLiteral("claim-capture-ticket"), body);
}

void CaptureTicketHttpClient::finish(
        const QString &taskId,
        const QJsonArray &taskLogItems,
        int operationId)
{
    QJsonObject body;
    body.insert(QStringLiteral("taskId"), taskId);
    body.insert(QStringLiteral("taskLogItems"), taskLogItems);
    post(false, operationId, taskId,
         QStringLiteral("finish-capture-batch"), body);
}

void CaptureTicketHttpClient::post(
        bool claimRequest,
        int operationId,
        const QString &taskId,
        const QString &path,
        const QJsonObject &body)
{
    const QUrl url(endpoint(path));
    const QString token = m_auth->accessToken().trimmed();
    const auto fail = [this, claimRequest, operationId, taskId](const QString &reason) {
        if (claimRequest) emit claimFailed(operationId, reason);
        else emit finishFailed(operationId, taskId, reason);
    };
    if (!url.isValid() || url.scheme().isEmpty() || url.host().isEmpty()) {
        fail(QStringLiteral("\u91c7\u7801\u7968\u63a5\u53e3\u5730\u5740\u65e0\u6548"));
        return;
    }
    if (token.isEmpty()) {
        fail(QStringLiteral("\u5c1a\u672a\u767b\u5f55\uff0c\u65e0\u6cd5\u8c03\u7528\u91c7\u7801\u7968\u63a5\u53e3"));
        return;
    }

    const QByteArray requestBody =
            QJsonDocument(body).toJson(QJsonDocument::Compact);
    emit logAppended(QStringLiteral("[%1] [CAPTURE] POST %2\n%3")
                     .arg(QDateTime::currentDateTime().toString(
                              QStringLiteral("yyyy-MM-dd hh:mm:ss")),
                          url.toString(),
                          QString::fromUtf8(
                              QJsonDocument(body).toJson(QJsonDocument::Indented))));

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/json"));
    request.setRawHeader("Accept", "application/json");
    request.setRawHeader("Kids-Auth",
                         QStringLiteral("Bearer %1").arg(token).toUtf8());
    request.setRawHeader("Kids-Requested-With", "KidsHttpRequest");
    QNetworkReply *reply = m_network->post(request, requestBody);
    QTimer *timer = new QTimer(reply);
    timer->setSingleShot(true);
    timer->setInterval(10000);
    connect(timer, &QTimer::timeout, reply, [reply]() {
        reply->setProperty("captureTimeout", true);
        reply->abort();
    });
    timer->start();

    connect(reply, &QNetworkReply::finished, this,
            [this, reply, timer, claimRequest, operationId, taskId]() {
        timer->stop();
        const int status = reply->attribute(
                    QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray response = reply->readAll();
        QString reason;
        QJsonParseError parseError;
        const QJsonDocument document =
                QJsonDocument::fromJson(response, &parseError);
        const QJsonObject root = document.object();
        const bool timedOut = reply->property("captureTimeout").toBool();
        const bool success = !timedOut
                && reply->error() == QNetworkReply::NoError
                && status >= 200 && status < 300
                && root.value(QStringLiteral("success")).toBool(false)
                && root.value(QStringLiteral("code")).toInt() == 200;
        emit logAppended(QStringLiteral("[CAPTURE] HTTP %1\n%2")
                         .arg(status)
                         .arg(response.isEmpty()
                              ? QStringLiteral("<empty>")
                              : QString::fromUtf8(
                                  document.isObject()
                                    ? document.toJson(QJsonDocument::Indented)
                                    : response)));
        if (!success) {
            reason = timedOut
                    ? QStringLiteral("\u91c7\u7801\u7968 HTTP \u8bf7\u6c42\u8d85\u65f6")
                    : root.value(QStringLiteral("msg")).toString().trimmed();
            if (reason.isEmpty()) reason = reply->errorString();
            if (reason.isEmpty()) reason = QStringLiteral("HTTP %1").arg(status);
            if (claimRequest) emit claimFailed(operationId, reason);
            else emit finishFailed(operationId, taskId, reason);
            reply->deleteLater();
            return;
        }
        if (claimRequest) {
            const QJsonValue dataValue = root.value(QStringLiteral("data"));
            if (!dataValue.isObject()) {
                emit claimFailed(operationId,
                                 QStringLiteral("\u91c7\u7801\u7968\u63a5\u53e3\u672a\u8fd4\u56de\u6709\u6548 data"));
            } else {
                emit claimSucceeded(operationId, dataValue.toObject());
            }
        } else {
            emit finishSucceeded(operationId, taskId);
        }
        reply->deleteLater();
    });
}

QString CaptureTicketHttpClient::endpoint(const QString &path) const
{
    QString base = ConfigManager::instance()->apiUrl().trimmed();
    while (base.endsWith(QLatin1Char('/'))) base.chop(1);
    return base.isEmpty() ? QString() : base + QLatin1Char('/') + path;
}
