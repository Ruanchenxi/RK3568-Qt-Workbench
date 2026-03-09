/**
 * @file TicketIngressService.h
 * @brief 主程序内本地 HTTP 接收服务（工作台 JSON 输入源）
 */
#ifndef TICKETINGRESSSERVICE_H
#define TICKETINGRESSSERVICE_H

#include <QObject>
#include <QHash>

class QTcpServer;
class QTcpSocket;

class TicketIngressService : public QObject
{
    Q_OBJECT
public:
    explicit TicketIngressService(QObject *parent = nullptr);

    bool start();
    QString lastError() const { return m_lastError; }

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
    QString saveBodyToFile(const QByteArray &body) const;

    QTcpServer *m_server;
    QHash<QTcpSocket *, QByteArray> m_buffers;
    QString m_lastError;
};

#endif // TICKETINGRESSSERVICE_H
