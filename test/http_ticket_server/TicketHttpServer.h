#ifndef TICKETHTTPSERVER_H
#define TICKETHTTPSERVER_H

#include <QObject>
#include <QTcpServer>
#include <QHash>

class QTcpSocket;

/// 极简 HTTP Server —— 仅服务 POST /ywticket/WebApi/transTicket
/// Step2: 增加日志信号 + 文件落盘
class TicketHttpServer : public QObject
{
    Q_OBJECT
public:
    explicit TicketHttpServer(QObject *parent = nullptr);

    /// 开始监听；成功返回 true
    bool start(const QHostAddress &address, quint16 port);

signals:
    /// 每收到一条有效的 POST 请求时发出（供 LogWindow 展示）
    void requestReceived(const QString &formattedLog);
    /// HTTP JSON 成功落盘后发出（供 Replay 面板跟随最新样本）
    void jsonBodySaved(const QString &filePath);

private slots:
    void onNewConnection();
    void onReadyRead();
    void onDisconnected();

private:
    /// 尝试从已收到的数据中解析完整 HTTP 请求并处理
    void tryProcessRequest(QTcpSocket *socket);

    /// 处理 transTicket 请求：解析JSON、落盘、发信号、回响应
    void handleTransTicket(QTcpSocket *socket,
                           const QByteArray &body,
                           const QString &clientAddr,
                           int contentLength,
                           const QString &origin,
                           const QString &userAgent);

    /// 将原始 body 保存到 logs/ 目录
    QString saveBodyToFile(const QByteArray &body);

    /// 构造并发送 HTTP 响应
    void sendResponse(QTcpSocket *socket, int statusCode,
                      const QByteArray &statusText,
                      const QByteArray &body);

    /// 发送 CORS pre-flight (OPTIONS) 响应
    void sendCorsPreflightResponse(QTcpSocket *socket);

    QTcpServer *m_server;

    /// 每个 socket 的累积接收缓冲
    QHash<QTcpSocket *, QByteArray> m_buffers;
};

#endif // TICKETHTTPSERVER_H
