#include "TicketHttpServer.h"
#include "TicketFrameBuilder.h"
#include "TicketPayloadEncoder.h"

#include <QTcpSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QCoreApplication>

// ────────────────────────────────────────────────────────────
TicketHttpServer::TicketHttpServer(QObject *parent)
    : QObject(parent)
    , m_server(new QTcpServer(this))
{
    connect(m_server, &QTcpServer::newConnection,
            this, &TicketHttpServer::onNewConnection);
}

// ────────────────────────────────────────────────────────────
bool TicketHttpServer::start(const QHostAddress &address, quint16 port)
{
    if (!m_server->listen(address, port)) {
        qCritical("Failed to listen on %s:%u – %s",
                  qPrintable(address.toString()), port,
                  qPrintable(m_server->errorString()));
        return false;
    }
    qInfo("==================================================");
    qInfo("  Listening URL : http://%s:%u", qPrintable(address.toString()), port);
    qInfo("  Ticket endpoint: http://%s:%u/ywticket/WebApi/transTicket",
          qPrintable(address.toString()), port);
    qInfo("  CORS           : enabled (Access-Control-Allow-Origin: *)");
    qInfo("  Logs dir       : <exe_dir>/logs/");
    qInfo("==================================================");
    return true;
}

// ────────────────────────────────────────────────────────────
void TicketHttpServer::onNewConnection()
{
    while (QTcpSocket *sock = m_server->nextPendingConnection()) {
        connect(sock, &QTcpSocket::readyRead,
                this, &TicketHttpServer::onReadyRead);
        connect(sock, &QTcpSocket::disconnected,
                this, &TicketHttpServer::onDisconnected);
    }
}

// ────────────────────────────────────────────────────────────
void TicketHttpServer::onReadyRead()
{
    QTcpSocket *sock = qobject_cast<QTcpSocket *>(sender());
    if (!sock) return;

    m_buffers[sock].append(sock->readAll());
    tryProcessRequest(sock);
}

// ────────────────────────────────────────────────────────────
void TicketHttpServer::onDisconnected()
{
    QTcpSocket *sock = qobject_cast<QTcpSocket *>(sender());
    if (!sock) return;
    m_buffers.remove(sock);
    sock->deleteLater();
}

// ────────────────────────────────────────────────────────────
void TicketHttpServer::tryProcessRequest(QTcpSocket *socket)
{
    QByteArray &buf = m_buffers[socket];

    // ---- 1. 等到 header 结束 ----
    int headerEnd = buf.indexOf("\r\n\r\n");
    if (headerEnd < 0)
        return;

    QByteArray headerBlock = buf.left(headerEnd);
    int bodyOffset = headerEnd + 4;

    // ---- 2. 解析请求行 ----
    int firstLineEnd = headerBlock.indexOf("\r\n");
    QByteArray requestLine = (firstLineEnd > 0)
                             ? headerBlock.left(firstLineEnd)
                             : headerBlock;

    QList<QByteArray> parts = requestLine.split(' ');
    if (parts.size() < 3) {
        sendResponse(socket, 400, "Bad Request",
                     R"({"success":false,"msg":"bad request line"})");
        return;
    }
    QByteArray method = parts[0];
    QByteArray path   = parts[1];

    // ---- 3. 解析 headers ----
    int contentLength = 0;
    QString originHeader;
    QString userAgentHeader;
    {
        QList<QByteArray> lines = headerBlock.mid(firstLineEnd + 2).split('\n');
        for (const QByteArray &line : lines) {
            QByteArray trimmed = line.trimmed();
            if (trimmed.isEmpty()) continue;
            int colon = trimmed.indexOf(':');
            if (colon < 0) continue;
            QByteArray key = trimmed.left(colon).trimmed().toLower();
            QByteArray val = trimmed.mid(colon + 1).trimmed();
            if (key == "content-length") {
                contentLength = val.toInt();
            } else if (key == "origin") {
                originHeader = QString::fromUtf8(val);
            } else if (key == "user-agent") {
                userAgentHeader = QString::fromUtf8(val);
            }
        }
    }

    // ---- 4. 等 body 收齐 ----
    int bodyAvailable = buf.size() - bodyOffset;
    if (bodyAvailable < contentLength)
        return;

    QByteArray body = buf.mid(bodyOffset, contentLength);
    buf.clear();

    // ---- 5. 构造客户端地址 ----
    QString clientAddr = QString("%1:%2")
        .arg(socket->peerAddress().toString())
        .arg(socket->peerPort());

    // ---- 6. 路由 ----
    const QString targetPath = "/ywticket/WebApi/transTicket";

    if (method == "OPTIONS") {
        // 预检请求：记日志后回 200 + CORS 头
        QString log = QString::fromUtf8(
            "[%1] 🔍 PREFLIGHT OPTIONS %2 from %3\n"
            "     Origin: %4\n"
            "     User-Agent: %5\n"
            "     → 200 OK (CORS preflight passed)")
            .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz"),
                 QString::fromLatin1(path), clientAddr,
                 originHeader.isEmpty() ? QString::fromUtf8("(无)") : originHeader,
                 userAgentHeader.isEmpty() ? QString::fromUtf8("(无)") : userAgentHeader);
        emit requestReceived(log);
        sendCorsPreflightResponse(socket);
    } else if (method == "POST" && path == targetPath) {
        handleTransTicket(socket, body, clientAddr, contentLength,
                          originHeader, userAgentHeader);
    } else {
        // 非目标路由：返回 HTTP 200 + JSON（兼容前端读 success 字段）
        QString log = QString::fromUtf8("[%1] %2 %3 from %4 → not found")
            .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz"),
                 QString::fromLatin1(method), QString::fromLatin1(path), clientAddr);
        emit requestReceived(log);
        sendResponse(socket, 200, "OK",
                     R"({"success":false,"msg":"not found"})");
    }
}

// ────────────────────────────────────────────────────────────
void TicketHttpServer::handleTransTicket(QTcpSocket *socket,
                                         const QByteArray &body,
                                         const QString &clientAddr,
                                         int contentLength,
                                         const QString &origin,
                                         const QString &userAgent)
{
    QString now = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    QString separator = QString::fromUtf8(
        "────────────────────────────────────────────────────────────");

    // ---- 落盘 ----
    QString savedPath = saveBodyToFile(body);
    if (!savedPath.isEmpty() && QFile::exists(savedPath))
        emit jsonBodySaved(savedPath);

    // ---- JSON 解析 ----
    QJsonParseError parseErr;
    QJsonDocument doc = QJsonDocument::fromJson(body, &parseErr);

    QString logText;
    logText += QString::fromUtf8("[%1] ✅ POST /ywticket/WebApi/transTicket from %2 len=%3\n")
        .arg(now, clientAddr).arg(contentLength);
    if (!origin.isEmpty())
        logText += QString::fromUtf8("     Origin     : %1\n").arg(origin);
    if (!userAgent.isEmpty())
        logText += QString::fromUtf8("     User-Agent : %1\n").arg(userAgent);

    if (doc.isNull()) {
        // JSON 解析失败
        logText += QString::fromUtf8("  ⚠ JSON 解析失败: %1\n").arg(parseErr.errorString());
        logText += QString::fromUtf8("  原始 body:\n  %1\n").arg(QString::fromUtf8(body));
        logText += QString::fromUtf8("  已保存到: %1\n").arg(savedPath);
        logText += separator;

        emit requestReceived(logText);

        sendResponse(socket, 200, "OK",
                     R"({"success":false,"msg":"invalid json"})");
        return;
    }

    // ---- 识别业务 payload（兼容 {data:{...}} 包装） ----
    QJsonObject root = doc.object();
    QJsonDocument payloadDoc;
    QString payloadLabel;

    if (root.contains("data") && root["data"].isObject()) {
        payloadDoc = QJsonDocument(root["data"].toObject());
        payloadLabel = QString::fromUtf8("  业务 payload (data 内层):");
    } else {
        payloadDoc = doc;
        payloadLabel = QString::fromUtf8("  业务 payload:");
    }

    QString prettyJson = QString::fromUtf8(payloadDoc.toJson(QJsonDocument::Indented));
    // 给 pretty JSON 每行加缩进
    QStringList jsonLines = prettyJson.split('\n');
    QString indentedJson;
    for (const QString &line : jsonLines) {
        if (!line.trimmed().isEmpty())
            indentedJson += "    " + line + "\n";
    }

    logText += payloadLabel + "\n";
    logText += indentedJson;
    logText += QString::fromUtf8("  已保存到: %1\n").arg(savedPath);

    // ---- Step5: 真实 JSON -> payload -> 帧 ----
    {
        // 取业务 JSON 对象（兼容裸对象和 data 内层）
        QJsonObject ticketObj = root.contains("data") && root["data"].isObject()
                                ? root["data"].toObject()
                                : root;

        // 编码 payload
        TicketPayloadEncoder::EncodeResult enc =
            TicketPayloadEncoder::encode(ticketObj, 0x01,
                                         TicketPayloadEncoder::StringEncoding::GBK);

        // 输出字段调试日志
        logText += "\n";
        logText += enc.fieldLog.join('\n') + "\n";

        if (enc.ok) {
            // 封帧
            TicketFrameBuilder builder(0x01, 0x00, 0x00);  // Station=0x01, Device=0x00, KeyId=0x00
            QList<QByteArray> frames = builder.buildFrames(enc.payload);
            QStringList frameLogLines, hexLines;
            builder.formatFrameLog(frames, frameLogLines, hexLines);

            logText += "\n";
            logText += frameLogLines.join('\n') + "\n";

            // 落盘 .hex / .bin
            QString logsDir = QCoreApplication::applicationDirPath() + "/logs";
            QDir().mkpath(logsDir);
            QString ts = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss_zzz");
            for (int i = 0; i < frames.size(); ++i) {
                QString base = logsDir +
                    QString("/ticket_frame_%1_%2").arg(ts).arg(i);
                TicketFrameBuilder::saveFrameFiles(frames[i], base);
            }
        } else {
            logText += QString::fromUtf8("  [Encoder ERROR] %1\n").arg(enc.errorMsg);
        }
    }

    logText += separator;

    emit requestReceived(logText);

    sendResponse(socket, 200, "OK",
                 R"({"success":true,"msg":"ok"})");
}

// ────────────────────────────────────────────────────────────
QString TicketHttpServer::saveBodyToFile(const QByteArray &body)
{
    // logs/ 在可执行文件同级目录
    QString logsDir = QCoreApplication::applicationDirPath() + "/logs";
    QDir dir;
    if (!dir.exists(logsDir)) {
        dir.mkpath(logsDir);
    }

    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss_zzz");
    QString fileName = QString("http_ticket_%1.json").arg(timestamp);
    QString filePath = logsDir + "/" + fileName;

    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(body);
        file.close();
        qInfo("  Saved to: %s", qPrintable(filePath));
    } else {
        qWarning("  Failed to save: %s – %s",
                 qPrintable(filePath), qPrintable(file.errorString()));
    }

    return filePath;
}

// ────────────────────────────────────────────────────────────
static QByteArray corsHeaders()
{
    QByteArray h;
    h.append("Access-Control-Allow-Origin: *\r\n");
    h.append("Access-Control-Allow-Methods: POST, GET, OPTIONS\r\n");
    h.append("Access-Control-Allow-Headers: Content-Type, Authorization, X-Requested-With\r\n");
    h.append("Access-Control-Max-Age: 86400\r\n");
    return h;
}

// ────────────────────────────────────────────────────────────
void TicketHttpServer::sendCorsPreflightResponse(QTcpSocket *socket)
{
    // 返回 200（而非 204），确保各版本浏览器均接受
    QByteArray body = R"({"success":true,"msg":"ok"})";
    QByteArray resp;
    resp.append("HTTP/1.1 200 OK\r\n");
    resp.append("Content-Type: application/json; charset=utf-8\r\n");
    resp.append(corsHeaders());
    resp.append("Content-Length: ");
    resp.append(QByteArray::number(body.size()));
    resp.append("\r\n");
    resp.append("Connection: close\r\n");
    resp.append("\r\n");
    resp.append(body);

    socket->write(resp);
    socket->flush();
    socket->disconnectFromHost();
}

// ────────────────────────────────────────────────────────────
void TicketHttpServer::sendResponse(QTcpSocket *socket, int statusCode,
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
    resp.append("Connection: close\r\n");
    resp.append("\r\n");
    resp.append(body);

    socket->write(resp);
    socket->flush();
    socket->disconnectFromHost();
}
