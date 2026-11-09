#ifndef TICKETTERMINATIONCLIENT_H
#define TICKETTERMINATIONCLIENT_H

#include <QObject>
#include <QString>

class QNetworkAccessManager;

class TicketTerminationClient : public QObject
{
    Q_OBJECT
public:
    explicit TicketTerminationClient(QObject *parent = nullptr);

    void terminate(const QString &taskId, const QString &accessToken);

signals:
    void logAppended(const QString &text);
    void terminationSucceeded(const QString &taskId);
    void terminationFailed(const QString &taskId, const QString &reason);

private:
    QNetworkAccessManager *m_network;
};

#endif // TICKETTERMINATIONCLIENT_H
