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

class QTcpServer;
class QTcpSocket;

class TicketIngressService : public QObject
{
    Q_OBJECT
public:
    struct IngressResult
    {
        bool accepted = false;
        int statusCode = 400;
        QString message;
        QString taskId;
    };

    using IngressHandler = std::function<IngressResult(const QByteArray &jsonBytes,
                                                       const QString &savedPath)>;

    explicit TicketIngressService(QObject *parent = nullptr);

    bool start();
    QString lastError() const { return m_lastError; }
    void setIngressHandler(IngressHandler handler);

signals:
    void httpServerLogAppended(const QString &text);
    void jsonReceived(const QByteArray &jsonBytes, const QString &savedPath);

private slots:
    void onNewConnection();
    void onReadyRead();
    void onDisconnected();

private:
    void tryProcessRequest(QTcpSocket *socket);
    void sendResponse(QTcpSocket *socket, int statusCode,
                      const QByteArray &statusText,
                      const QByteArray &body);
    void sendCorsPreflightResponse(QTcpSocket *socket);
    QByteArray buildJsonResponseBody(bool success,
                                     int code,
                                     const QString &message,
                                     const QString &taskId = QString()) const;
    QString saveBodyToFile(const QByteArray &body) const;

    QTcpServer *m_server;
    QHash<QTcpSocket *, QByteArray> m_buffers;
    QString m_lastError;
    QString m_listenHost;
    quint16 m_listenPort = 0;
    QString m_listenPath;
    IngressHandler m_ingressHandler;
};

#endif // TICKETINGRESSSERVICE_H
