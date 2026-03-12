#include "features/key/application/TicketIngressService.h"

#include <QTcpServer>
#include <QTcpSocket>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QCoreApplication>
#include <QHostAddress>
#include <QJsonDocument>

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

bool TicketIngressService::start()
{
    const QHostAddress host(ConfigManager::instance()->value("ticket/httpIngressHost", "127.0.0.1").toString());
    const quint16 port = static_cast<quint16>(ConfigManager::instance()->value("ticket/httpIngressPort", 8888).toUInt());

    if (m_server->isListening())
        return true;

    if (!m_server->listen(host, port)) {
        m_lastError = QString::fromUtf8("HTTP接收服务启动失败: %1").arg(m_server->errorString());
        return false;
    }

    emit httpServerLogAppended(QString::fromUtf8(
        "[HTTP-INGRESS] listening http://%1:%2/ywticket/WebApi/transTicket")
        .arg(host.toString()).arg(port));
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
        sendResponse(socket, 400, "Bad Request",
                     R"({"success":false,"msg":"bad request line"})");
        return;
    }

    const QByteArray method = parts[0];
    const QByteArray path = parts[1];
    int contentLength = 0;
    const QList<QByteArray> lines = headerBlock.mid(firstLineEnd + 2).split('\n');
    for (const QByteArray &line : lines) {
        const QByteArray trimmed = line.trimmed();
        const int colon = trimmed.indexOf(':');
        if (colon < 0)
            continue;
        const QByteArray key = trimmed.left(colon).trimmed().toLower();
        const QByteArray val = trimmed.mid(colon + 1).trimmed();
        if (key == "content-length")
            contentLength = val.toInt();
    }

    const int bodyAvailable = buf.size() - bodyOffset;
    if (bodyAvailable < contentLength)
        return;

    const QByteArray body = buf.mid(bodyOffset, contentLength);
    buf.clear();

    const QString targetPath = ConfigManager::instance()
            ->value("ticket/httpIngressPath", "/ywticket/WebApi/transTicket").toString();

    if (method == "OPTIONS") {
        emit httpServerLogAppended(QString::fromUtf8(
            "[%1] OPTIONS %2 -> 200 OK")
            .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz"))
            .arg(QString::fromLatin1(path)));
        sendCorsPreflightResponse(socket);
    } else if (method == "POST" && QString::fromLatin1(path) == targetPath) {
        const QString savedPath = saveBodyToFile(body);
        const QJsonDocument doc = QJsonDocument::fromJson(body);
        QString pretty = QString::fromUtf8(body);
        if (!doc.isNull())
            pretty = QString::fromUtf8(doc.toJson(QJsonDocument::Indented));

        emit httpServerLogAppended(QString::fromUtf8(
            "[%1] POST %2\n已保存到: %3\n%4")
            .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz"))
            .arg(targetPath)
            .arg(savedPath)
            .arg(pretty));
        emit jsonReceived(body, savedPath);
        sendResponse(socket, 200, "OK", R"({"success":true,"msg":"ok"})");
    } else {
        sendResponse(socket, 200, "OK", R"({"success":false,"msg":"not found"})");
    }
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
    sendResponse(socket, 200, "OK", R"({"success":true,"msg":"ok"})");
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
