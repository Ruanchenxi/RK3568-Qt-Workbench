/**
 * @file TicketIngressService.h
 * @brief 主程序内本地 HTTP 接收服务（工作台 JSON 输入源）
 */
#ifndef TICKETINGRESSSERVICE_H
#define TICKETINGRESSSERVICE_H

#include <functional>
#include <QObject>
#include <QHash>
#include <QString>
#include <QTimer>

class QTcpServer;
class QTcpSocket;

class TicketIngressService : public QObject
{
    Q_OBJECT
public:
    struct IngressResult
    {
        bool accepted = false;
        bool deferred = false;   // true：socket 挂起，等 completeDeferredResponse() 回包
        int statusCode = 400;
        QString message;
        QString taskId;
    };

    using PersistBodyFn = std::function<QString()>;
    using IngressHandler = std::function<IngressResult(const QByteArray &jsonBytes,
                                                       const PersistBodyFn &persistBody)>;
    using CancelHandler = std::function<IngressResult(const QString &taskId)>;

    explicit TicketIngressService(QObject *parent = nullptr);

    bool start();
    QString lastError() const { return m_lastError; }
    void setIngressHandler(IngressHandler handler);
    void setCancelHandler(CancelHandler handler);

    // 串口确认后回包给挂起的 socket；success=false 时立即返回失败
    void completeDeferredResponse(const QString &taskId, bool success, const QString &message);

signals:
    void httpServerLogAppended(const QString &text);
    void jsonReceived(const QByteArray &jsonBytes, const QString &savedPath);

private slots:
    void onNewConnection();
    void onReadyRead();
    void onDisconnected();

private:
    struct DeferredRequest {
        QTcpSocket *socket = nullptr;
        QTimer     *timer  = nullptr;
    };

    void tryProcessRequest(QTcpSocket *socket);
    void sendResponse(QTcpSocket *socket, int statusCode,
                      const QByteArray &statusText,
                      const QByteArray &body);
    void sendCorsPreflightResponse(QTcpSocket *socket);
    QByteArray buildJsonResponseBody(bool success,
                                     const QString &message,
                                     const QString &taskId = QString(),
                                     int statusCode = 200) const;
    QString saveBodyToFile(const QByteArray &body) const;
    // 超时触发，向工作台返回传票确认超时
    void expireDeferredRequest(const QString &taskId);

    QTcpServer *m_server;
    QHash<QTcpSocket *, QByteArray> m_buffers;
    QHash<QString, DeferredRequest> m_deferredRequests;  // taskId → 挂起的 socket
    QString m_lastError;
    QString m_listenHost;
    quint16 m_listenPort = 0;
    QString m_listenPath;
    QString m_cancelPath;
    IngressHandler m_ingressHandler;
    CancelHandler m_cancelHandler;
};

#endif // TICKETINGRESSSERVICE_H
