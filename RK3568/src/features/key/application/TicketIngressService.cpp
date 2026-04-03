#include "features/key/application/TicketIngressService.h"

#include <QTcpServer>
#include <QTcpSocket>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QCoreApplication>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>

#include "core/ConfigManager.h"

static QByteArray corsHeaders()
{
    QByteArray h;
    h.append("Access-Control-Allow-Origin: *\r\n");
    h.append("Access-Control-Allow-Methods: POST, GET, OPTIONS\r\n");
    h.append("Access-Control-Allow-Headers: Content-Type, Authorization, X-Requested-With\r\n");
    h.append("Access-Control-Max-Age: 86400\r\n");
    return h;
}

TicketIngressService::TicketIngressService(QObject *parent)
    : QObject(parent)
    , m_server(new QTcpServer(this))
{
    connect(m_server, &QTcpServer::newConnection,
            this, &TicketIngressService::onNewConnection);
}

void TicketIngressService::setIngressHandler(IngressHandler handler)
{
    m_ingressHandler = std::move(handler);
}

void TicketIngressService::setCancelHandler(CancelHandler handler)
{
    m_cancelHandler = std::move(handler);
}

bool TicketIngressService::start()
{
    const QHostAddress host(ConfigManager::instance()->value("ticket/httpIngressHost", "127.0.0.1").toString());
    const quint16 port = static_cast<quint16>(ConfigManager::instance()->value("ticket/httpIngressPort", 8888).toUInt());
    const QString path = ConfigManager::instance()
            ->value("ticket/httpIngressPath", "/ywticket/WebApi/transTicket").toString();

    if (m_server->isListening())
        return true;

    if (!m_server->listen(host, port)) {
        m_lastError = QString::fromUtf8("HTTP接收服务启动失败: %1").arg(m_server->errorString());
        return false;
    }

    m_listenHost = host.toString();
    m_listenPort = port;
    m_listenPath = path;
    m_cancelPath = ConfigManager::instance()
            ->value("ticket/httpCancelPath", "/ywticket/WebApi/cancelTicket").toString();

    emit httpServerLogAppended(QString::fromUtf8(
        "[HTTP-INGRESS] listening host=%1 port=%2 transTicket=%3 cancelTicket=%4")
        .arg(m_listenHost)
        .arg(m_listenPort)
        .arg(m_listenPath)
        .arg(m_cancelPath));
    return true;
}

void TicketIngressService::onNewConnection()
{
    while (QTcpSocket *sock = m_server->nextPendingConnection()) {
        connect(sock, &QTcpSocket::readyRead, this, &TicketIngressService::onReadyRead);
        connect(sock, &QTcpSocket::disconnected, this, &TicketIngressService::onDisconnected);
    }
}

void TicketIngressService::onReadyRead()
{
    QTcpSocket *sock = qobject_cast<QTcpSocket *>(sender());
    if (!sock)
        return;

    m_buffers[sock].append(sock->readAll());
    tryProcessRequest(sock);
}

void TicketIngressService::onDisconnected()
{
    QTcpSocket *sock = qobject_cast<QTcpSocket *>(sender());
    if (!sock)
        return;
    m_buffers.remove(sock);
    sock->deleteLater();
}

void TicketIngressService::tryProcessRequest(QTcpSocket *socket)
{
    QByteArray &buf = m_buffers[socket];
    const int headerEnd = buf.indexOf("\r\n\r\n");
    if (headerEnd < 0)
        return;

    const QByteArray headerBlock = buf.left(headerEnd);
    const int bodyOffset = headerEnd + 4;
    const int firstLineEnd = headerBlock.indexOf("\r\n");
    const QByteArray requestLine = (firstLineEnd > 0) ? headerBlock.left(firstLineEnd) : headerBlock;
    const QList<QByteArray> parts = requestLine.split(' ');
    if (parts.size() < 3) {
        emit httpServerLogAppended(QString::fromUtf8(
            "[%1] malformed request line -> 400 Bad Request")
            .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz")));
        sendResponse(socket, 400, "Bad Request",
                     buildJsonResponseBody(false, QStringLiteral("bad request line"), QString(), 400));
        return;
    }

    const QByteArray method = parts[0];
    const QByteArray path = parts[1];
    const QString methodText = QString::fromLatin1(method).toUpper();
    const QString pathText = QString::fromLatin1(path);

    bool contentLengthSeen = false;
    bool contentLengthOk = true;
    int contentLength = 0;
    const QList<QByteArray> lines = headerBlock.mid(firstLineEnd + 2).split('\n');
    for (const QByteArray &line : lines) {
        const QByteArray trimmed = line.trimmed();
        const int colon = trimmed.indexOf(':');
        if (colon < 0)
            continue;
        const QByteArray key = trimmed.left(colon).trimmed().toLower();
        const QByteArray val = trimmed.mid(colon + 1).trimmed();
        if (key == "content-length") {
            contentLengthSeen = true;
            bool ok = false;
            contentLength = val.toInt(&ok);
            contentLengthOk = ok && contentLength >= 0;
        }
    }

    const bool isTransTicket = (pathText == m_listenPath);
    const bool isCancelTicket = (pathText == m_cancelPath);
    if (!isTransTicket && !isCancelTicket) {
        emit httpServerLogAppended(QString::fromUtf8(
            "[%1] %2 %3 -> 404 Not Found")
            .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz"))
            .arg(methodText)
            .arg(pathText));
        sendResponse(socket, 404, "Not Found",
                     buildJsonResponseBody(false, QStringLiteral("ingress path not found"), QString(), 404));
        return;
    }

    const int bodyAvailable = buf.size() - bodyOffset;
    if ((method == "POST" || method == "OPTIONS") && contentLengthSeen && bodyAvailable < contentLength)
        return;

    if (method == "OPTIONS") {
        emit httpServerLogAppended(QString::fromUtf8(
            "[%1] OPTIONS %2 -> 200 OK")
            .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz"))
            .arg(pathText));
        buf.clear();
        sendCorsPreflightResponse(socket);
        return;
    }

    if (method != "POST") {
        emit httpServerLogAppended(QString::fromUtf8(
            "[%1] %2 %3 -> 405 Method Not Allowed")
            .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz"))
            .arg(methodText)
            .arg(pathText));
        sendResponse(socket, 405, "Method Not Allowed",
                     buildJsonResponseBody(false, QStringLiteral("method not allowed"), QString(), 405));
        return;
    }

    if (!contentLengthSeen || !contentLengthOk) {
        emit httpServerLogAppended(QString::fromUtf8(
            "[%1] POST %2 -> 400 Bad Request (invalid Content-Length)")
            .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz"))
            .arg(pathText));
        sendResponse(socket, 400, "Bad Request",
                     buildJsonResponseBody(false, QStringLiteral("invalid content-length"), QString(), 400));
        return;
    }

    const QByteArray body = buf.mid(bodyOffset, contentLength);
    buf.clear();

    const QJsonDocument doc = QJsonDocument::fromJson(body);
    QString pretty = QString::fromUtf8(body);
    if (!doc.isNull())
        pretty = QString::fromUtf8(doc.toJson(QJsonDocument::Indented));

    // ── cancelTicket 路径：轻量处理，不落盘 ──
    if (isCancelTicket) {
        if (!m_cancelHandler) {
            sendResponse(socket, 500, "Internal Server Error",
                         buildJsonResponseBody(false, QStringLiteral("cancel handler missing"), QString(), 500));
            return;
        }
        const QJsonObject cancelRoot = doc.isNull() ? QJsonObject() : doc.object();
        const QString taskId = cancelRoot.value(QStringLiteral("taskId")).toString().trimmed();
        const IngressResult result = m_cancelHandler(taskId);
        const int sc = result.statusCode > 0 ? result.statusCode : 200;

        emit httpServerLogAppended(QString::fromUtf8(
            "[%1] POST %2 -> %3 %4 taskId=%5\n%6")
            .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz"),
                 pathText,
                 QString::number(sc),
                 result.accepted ? QStringLiteral("CANCEL_OK") : QStringLiteral("CANCEL_REJECTED"),
                 taskId.isEmpty() ? QStringLiteral("<empty>") : taskId,
                 pretty));

        sendResponse(socket, sc, "OK",
                     buildJsonResponseBody(result.accepted,
                                           result.message.isEmpty()
                                               ? QStringLiteral("撤销票成功")
                                               : result.message,
                                           result.taskId,
                                           sc));
        return;
    }

    // ── transTicket 路径：原有逻辑 ──
    if (!m_ingressHandler) {
        emit httpServerLogAppended(QString::fromUtf8(
            "[%1] POST %2 -> 500 Internal Server Error (ingress handler missing)")
            .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz"))
            .arg(pathText));
        sendResponse(socket, 500, "Internal Server Error",
                     buildJsonResponseBody(false, QStringLiteral("ingress handler missing"), QString(), 500));
        return;
    }

    QString savedPath;
    bool bodyPersisted = false;
    const PersistBodyFn persistBody = [this, body, &savedPath, &bodyPersisted]() -> QString {
        if (!bodyPersisted) {
            savedPath = saveBodyToFile(body);
            bodyPersisted = true;
        }
        return savedPath;
    };

    const IngressResult result = m_ingressHandler(body, persistBody);
    const int statusCode = result.statusCode > 0 ? result.statusCode : (result.accepted ? 200 : 400);
    const QByteArray statusText = (statusCode >= 200 && statusCode < 300)
            ? QByteArrayLiteral("OK")
            : (statusCode == 404 ? QByteArrayLiteral("Not Found")
                                 : (statusCode == 405 ? QByteArrayLiteral("Method Not Allowed")
                                                      : (statusCode == 500 ? QByteArrayLiteral("Internal Server Error")
                                                                           : QByteArrayLiteral("Bad Request"))));
    const QString acceptanceText = result.accepted
            ? QStringLiteral("ACCEPTED_ASYNC")
            : QStringLiteral("REJECTED");
    const QString persistenceText = result.accepted
            ? (bodyPersisted
               ? QStringLiteral("请求体已保存到: %1（异步发送已启动）").arg(savedPath)
               : QStringLiteral("请求体未落盘（accepted path unexpected）"))
            : QStringLiteral("未保存请求体");

    emit httpServerLogAppended(QString::fromUtf8(
        "[%1] POST %2 -> %3 %4%5\n%6\n%7")
        .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz"))
        .arg(pathText)
        .arg(statusCode)
        .arg(acceptanceText)
        .arg(result.taskId.isEmpty() ? QString() : QStringLiteral(" taskId=%1").arg(result.taskId))
        .arg(persistenceText)
        .arg(pretty));

    if (!result.accepted && !result.message.trimmed().isEmpty()) {
        emit httpServerLogAppended(QString::fromUtf8(
            "[HTTP-INGRESS] business reject reason: %1")
            .arg(result.message));
    }

    if (result.accepted) {
        emit jsonReceived(body, savedPath);
    }

    sendResponse(socket, statusCode, statusText,
                 buildJsonResponseBody(result.accepted,
                                       result.message.isEmpty()
                                           ? (result.accepted ? QStringLiteral("传票成功")
                                                              : QStringLiteral("ticket ingest failed"))
                                           : result.message,
                                       result.taskId,
                                       statusCode));
}

void TicketIngressService::sendResponse(QTcpSocket *socket, int statusCode,
                                        const QByteArray &statusText,
                                        const QByteArray &body)
{
    QByteArray resp;
    resp.append("HTTP/1.1 ");
    resp.append(QByteArray::number(statusCode));
    resp.append(' ');
    resp.append(statusText);
    resp.append("\r\n");
    resp.append("Content-Type: application/json; charset=utf-8\r\n");
    resp.append(corsHeaders());
    resp.append("Content-Length: ");
    resp.append(QByteArray::number(body.size()));
    resp.append("\r\n");
    resp.append("Connection: close\r\n\r\n");
    resp.append(body);
    socket->write(resp);
    socket->flush();
    socket->disconnectFromHost();
}

void TicketIngressService::sendCorsPreflightResponse(QTcpSocket *socket)
{
    sendResponse(socket, 200, "OK",
                 buildJsonResponseBody(true, QStringLiteral("preflight ok")));
}

QByteArray TicketIngressService::buildJsonResponseBody(bool success,
                                                       const QString &message,
                                                       const QString &taskId,
                                                       int statusCode) const
{
    QJsonObject obj;
    obj.insert(QStringLiteral("success"), success);
    obj.insert(QStringLiteral("status"), statusCode);
    obj.insert(QStringLiteral("msg"), message);
    if (!taskId.trimmed().isEmpty()) {
        obj.insert(QStringLiteral("taskId"), taskId);
    }
    return QJsonDocument(obj).toJson(QJsonDocument::Compact);
}

QString TicketIngressService::saveBodyToFile(const QByteArray &body) const
{
    const QString logsDir = QCoreApplication::applicationDirPath() + "/logs";
    QDir().mkpath(logsDir);
    const QString filePath = logsDir + "/http_ticket_"
            + QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss_zzz") + ".json";
    QFile f(filePath);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        f.write(body);
        f.close();
    }
    return filePath;
}
