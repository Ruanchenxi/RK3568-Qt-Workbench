#include "features/key/application/TicketIngressService.h"

#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QDateTime>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>

#include "core/ConfigManager.h"

/*
 * 工作台 HTTP 接入边界：AppComposition 创建服务，KeyManageController
 * 注册业务回调并在 start() 中启动监听。每个 TCP 连接独立累计请求字节，
 * 按请求头和 Content-Length 收齐 body 后，才将原始 JSON 交给业务层。
 * 接入层不决定设备传票是否成功；业务结果与原请求的响应由后续流程衔接。
 */

static QByteArray corsHeaders()
{
    QByteArray h;
    h.append("Access-Control-Allow-Origin: *\r\n");
    h.append("Access-Control-Allow-Methods: POST, GET, OPTIONS\r\n");
    h.append("Access-Control-Allow-Headers: Content-Type, Authorization, X-Requested-With\r\n");
    h.append("Access-Control-Max-Age: 86400\r\n");
    return h;
}

// 创建接入服务并登记连接通知；监听由 Controller 启动时显式开启。
TicketIngressService::TicketIngressService(QObject *parent)
    : QObject(parent)
    , m_server(new QTcpServer(this))
{
    connect(m_server, &QTcpServer::newConnection,
            this, &TicketIngressService::onNewConnection);
}

// 保存 Controller 注册的处理函数，供完整 transTicket 请求到达时调用。
void TicketIngressService::setIngressHandler(IngressHandler handler)
{
    m_ingressHandler = std::move(handler);
}

// [CANCEL][DEFER] 取消票据流程的入口，学习 Cancel 时再回来。
void TicketIngressService::setCancelHandler(CancelHandler handler)
{
    m_cancelHandler = std::move(handler);
}

// 使用配置启动 TCP 监听；true 仅表示服务已在监听，不表示已接收或完成传票。
bool TicketIngressService::start()
{
    // 默认仅接收本机 127.0.0.1:8888；已有配置可覆盖地址、端口和 Transfer 路径。
    const QHostAddress host(ConfigManager::instance()->value("ticket/httpIngressHost", "127.0.0.1").toString());
    const quint16 port = static_cast<quint16>(ConfigManager::instance()->value("ticket/httpIngressPort", 8888).toUInt());
    const QString path = ConfigManager::instance()
            ->value("ticket/httpIngressPath", "/ywticket/WebApi/transTicket").toString();

    // 已经处于监听状态时直接成功返回，避免重复 listen()。
    if (m_server->isListening())
        return true;

    // TCP 按地址和端口监听；HTTP 路径在收齐请求头后另行匹配。
    if (!m_server->listen(host, port)) {
        m_lastError = QString::fromUtf8("HTTP接收服务启动失败: %1").arg(m_server->errorString());
        return false;
    }

    // 监听成功后保存接入配置；tryProcessRequest() 用路径区分 Transfer 与 Cancel。
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

// 为每条新 TCP 连接登记读取和断开处理；请求字节按对应 socket 隔离。
void TicketIngressService::onNewConnection()
{
    while (QTcpSocket *sock = m_server->nextPendingConnection()) {
        connect(sock, &QTcpSocket::readyRead, this, &TicketIngressService::onReadyRead);
        connect(sock, &QTcpSocket::disconnected, this, &TicketIngressService::onDisconnected);
    }
}

// 按连接累计字节后尝试处理请求；一次可读通知不保证 HTTP 请求完整。
void TicketIngressService::onReadyRead()
{
    QTcpSocket *sock = qobject_cast<QTcpSocket *>(sender());
    if (!sock)
        return;

    // 首次读取时建立该连接的缓冲，后续继续追加，供完整性检查使用，避免混入其他连接的数据。
    m_buffers[sock].append(sock->readAll());
    tryProcessRequest(sock);
}

// [TRANSFER][RETURN LATER] 客户端提前断开时，清理与该 socket 关联的等待状态。
void TicketIngressService::onDisconnected()
{
    QTcpSocket *sock = qobject_cast<QTcpSocket *>(sender());
    if (!sock)
        return;
    m_buffers.remove(sock);

    // 客户端主动断线时，清理挂起的延迟响应（定时器停掉避免访问已销毁 socket）
    for (auto it = m_deferredRequests.begin(); it != m_deferredRequests.end(); ) {
        if (it->socket == sock) {
            if (it->timer) {
                it->timer->stop();
                it->timer->deleteLater();
            }
            it = m_deferredRequests.erase(it);
        } else {
            ++it;
        }
    }

    for (auto it = m_deferredCancelRequests.begin(); it != m_deferredCancelRequests.end(); ) {
        if (it->socket == sock) {
            const QString taskId = it.key();
            if (it->timer) {
                it->timer->stop();
                it->timer->deleteLater();
            }
            it = m_deferredCancelRequests.erase(it);
            emit httpServerLogAppended(QString::fromUtf8(
                "[HTTP-INGRESS] cancel deferred socket disconnected taskId=%1").arg(taskId));
            emit cancelDeferredRequestExpired(taskId);
        } else {
            ++it;
        }
    }
    sock->deleteLater();
}

// 校验路径、方法和请求长度；transTicket 收齐 body 后调用业务回调。
// 请求不完整时保留缓冲等待后续字节，HTTP 完整性不等于票据业务有效。
void TicketIngressService::tryProcessRequest(QTcpSocket *socket)
{
    QByteArray &buf = m_buffers[socket];
    // 未收到头体分隔符时不能确定 body 起点，保留该连接的累计数据。
    const int headerEnd = buf.indexOf("\r\n\r\n");
    if (headerEnd < 0)
        return;

    const QByteArray headerBlock = buf.left(headerEnd);
    // 跳过四字节头体分隔符，后续长度判断与 body 提取共用此起点。
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

    // Content-Length 按字节声明 body 长度；后续据此等待完整数据，不能按 JSON 字符数判断。
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

    // 路径精确匹配监听时保存的配置；Transfer 与 Cancel 共用 HTTP 解析入口。
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

    // 请求头虽完整，body 仍可能只到达一部分；不足时保留 buffer，等待下一次 readyRead。
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

    // 合法 POST 的 body 已收齐；保留独立字节值供业务回调使用，再释放接收累计数据。
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
        const QString cancelResultText = !result.accepted
                ? QStringLiteral("CANCEL_REJECTED")
                : (result.deferred ? QStringLiteral("CANCEL_DEFERRED")
                                   : QStringLiteral("CANCEL_OK"));

        emit httpServerLogAppended(QString::fromUtf8(
            "[%1] POST %2 -> %3 %4 taskId=%5\n%6")
            .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz"),
                 pathText,
                 QString::number(sc),
                 cancelResultText,
                 taskId.isEmpty() ? QStringLiteral("<empty>") : taskId,
                 pretty));

        if (result.accepted && result.deferred && !result.taskId.trimmed().isEmpty()) {
            const QString tid = result.taskId.trimmed();
            auto *timer = new QTimer(this);
            timer->setSingleShot(true);
            connect(timer, &QTimer::timeout, this, [this, tid]() {
                expireDeferredCancelRequest(tid);
            });
            m_deferredCancelRequests.insert(tid, DeferredRequest{socket, timer});
            timer->start(15000);
            emit httpServerLogAppended(QString::fromUtf8(
                "[HTTP-INGRESS] cancel taskId=%1 socket held, waiting for delete confirm (15 s timeout)")
                .arg(tid));
            return;
        }

        sendResponse(socket, sc, "OK",
                     buildJsonResponseBody(result.accepted,
                                           result.message.isEmpty()
                                               ? QStringLiteral("撤销票成功")
                                               : result.message,
                                           result.taskId,
                                           sc));
        return;
    }

    // Transfer 将完整原始 body 交给 Controller，不在 HTTP 层剥离 data 包装或判定设备就绪。
    if (!m_ingressHandler) {
        emit httpServerLogAppended(QString::fromUtf8(
            "[%1] POST %2 -> 500 Internal Server Error (ingress handler missing)")
            .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz"))
            .arg(pathText));
        sendResponse(socket, 500, "Internal Server Error",
                     buildJsonResponseBody(false, QStringLiteral("ingress handler missing"), QString(), 500));
        return;
    }

    // 此回调由 Controller 注册，转入 handleTicketIngress；返回受理结果，不等于设备传票成功。
    const IngressResult result = m_ingressHandler(body);
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
            ? QString::fromUtf8("请求体仅在内存中处理（未落盘）")
            : QString::fromUtf8("未保存请求体");

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

    // ── 延迟响应：先保留本次 HTTP socket；等设备侧 Q_TASK 最终确认后再回复工作台 ──
    if (result.accepted && result.deferred && !result.taskId.trimmed().isEmpty()) {
        const QString tid = result.taskId.trimmed();
        auto *timer = new QTimer(this);
        timer->setSingleShot(true);
        connect(timer, &QTimer::timeout, this, [this, tid]() {
            expireDeferredRequest(tid);
        });
        m_deferredRequests.insert(tid, DeferredRequest{socket, timer});
        timer->start(8000);
        emit httpServerLogAppended(QString::fromUtf8(
            "[HTTP-INGRESS] taskId=%1 socket held, waiting for serial confirm (8 s timeout)")
            .arg(tid));
        return;
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

// [LEARNING][SKIP / MAP] 把状态码、响应头和响应体统一写回 socket。
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

// [LEARNING][SKIP] 浏览器跨域预检响应，与 Transfer 核心业务无关。
void TicketIngressService::sendCorsPreflightResponse(QTcpSocket *socket)
{
    sendResponse(socket, 200, "OK",
                 buildJsonResponseBody(true, QStringLiteral("preflight ok")));
}

// [LEARNING][SKIP / MAP] 统一生成 HTTP JSON 响应体。
QByteArray TicketIngressService::buildJsonResponseBody(bool success,
                                                       const QString &message,
                                                       const QString &taskId,
                                                       int statusCode) const
{
    QJsonObject obj;
    obj.insert(QStringLiteral("success"), success);
    obj.insert(QStringLiteral("status"), success ? 0 : statusCode); // 0=成功（Java API 约定），与前端对齐
    obj.insert(QStringLiteral("msg"), message);
    if (!taskId.trimmed().isEmpty()) {
        obj.insert(QStringLiteral("taskId"), taskId);
    }
    return QJsonDocument(obj).toJson(QJsonDocument::Compact);
}

// [TRANSFER][RETURN LATER] 设备侧最终确认后，用 taskId 找回并完成此前暂缓的 HTTP 响应。
bool TicketIngressService::completeDeferredResponse(const QString &taskId,
                                                    bool success,
                                                    const QString &message)
{
    auto it = m_deferredRequests.find(taskId);
    if (it == m_deferredRequests.end())
        return false;

    DeferredRequest req = it.value();
    m_deferredRequests.erase(it);

    if (req.timer) {
        req.timer->stop();
        req.timer->deleteLater();
    }

    if (!req.socket || req.socket->state() != QAbstractSocket::ConnectedState)
        return false;

    const QString msg = message.isEmpty()
            ? (success ? QStringLiteral("传票成功，已确认钥匙中存在任务")
                       : QStringLiteral("传票失败"))
            : message;

    emit httpServerLogAppended(QString::fromUtf8(
        "[HTTP-INGRESS] deferred response taskId=%1 success=%2 msg=%3")
        .arg(taskId,
             success ? QStringLiteral("true") : QStringLiteral("false"),
             msg));

    sendResponse(req.socket, 200, QByteArrayLiteral("OK"),
                 buildJsonResponseBody(success, msg, taskId, 200));
    return true;
}

// [CANCEL][DEFER] 取消流程最终完成时回复此前暂缓的 HTTP 请求。
bool TicketIngressService::completeDeferredCancelResponse(const QString &taskId,
                                                          bool success,
                                                          const QString &message)
{
    auto it = m_deferredCancelRequests.find(taskId);
    if (it == m_deferredCancelRequests.end())
        return false;

    DeferredRequest req = it.value();
    m_deferredCancelRequests.erase(it);

    if (req.timer) {
        req.timer->stop();
        req.timer->deleteLater();
    }

    if (!req.socket || req.socket->state() != QAbstractSocket::ConnectedState)
        return false;

    const QString msg = message.isEmpty()
            ? (success ? QStringLiteral("撤销票成功")
                       : QStringLiteral("撤销票失败"))
            : message;

    emit httpServerLogAppended(QString::fromUtf8(
        "[HTTP-INGRESS] cancel deferred response taskId=%1 success=%2 msg=%3")
        .arg(taskId,
             success ? QStringLiteral("true") : QStringLiteral("false"),
             msg));

    sendResponse(req.socket, 200, QByteArrayLiteral("OK"),
                 buildJsonResponseBody(success, msg, taskId, 200));
    return true;
}

// [CANCEL][DEFER] 取消流程等待超时的处理。
void TicketIngressService::expireDeferredCancelRequest(const QString &taskId)
{
    auto it = m_deferredCancelRequests.find(taskId);
    if (it == m_deferredCancelRequests.end())
        return;

    DeferredRequest req = it.value();
    m_deferredCancelRequests.erase(it);

    if (req.timer) {
        req.timer->stop();
        req.timer->deleteLater();
    }

    emit httpServerLogAppended(QString::fromUtf8(
        "[HTTP-INGRESS] cancel deferred request expired taskId=%1").arg(taskId));

    if (req.socket && req.socket->state() == QAbstractSocket::ConnectedState) {
        const QString msg = QStringLiteral("撤销确认超时，请确认钥匙状态后重试");
        sendResponse(req.socket, 200, QByteArrayLiteral("OK"),
                     buildJsonResponseBody(false, msg, taskId, 200));
    }

    emit cancelDeferredRequestExpired(taskId);
}

// [TRANSFER][RETURN LATER] Transfer 等待设备最终确认超时的处理。
void TicketIngressService::expireDeferredRequest(const QString &taskId)
{
    auto it = m_deferredRequests.find(taskId);
    if (it == m_deferredRequests.end())
        return;

    DeferredRequest req = it.value();
    m_deferredRequests.erase(it);

    if (req.timer) {
        req.timer->stop();
        req.timer->deleteLater();
    }

    emit httpServerLogAppended(QString::fromUtf8(
        "[HTTP-INGRESS] deferred request expired taskId=%1").arg(taskId));

    if (req.socket && req.socket->state() == QAbstractSocket::ConnectedState) {
        const QString msg = QStringLiteral("传票确认超时（8秒内未收到钥匙确认），请检查钥匙状态后重试");
        sendResponse(req.socket, 200, QByteArrayLiteral("OK"),
                     buildJsonResponseBody(false, msg, taskId, 200));
    }
}
