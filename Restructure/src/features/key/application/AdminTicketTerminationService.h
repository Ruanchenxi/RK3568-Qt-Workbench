#ifndef ADMINTICKETTERMINATIONSERVICE_H
#define ADMINTICKETTERMINATIONSERVICE_H

#include <functional>

#include <QObject>
#include <QHash>
#include <QSet>
#include <QString>

class TicketStore;
class TicketTerminationClient;

class AdminTicketTerminationService : public QObject
{
    Q_OBJECT
public:
    using AccessTokenProvider = std::function<QString()>;

    explicit AdminTicketTerminationService(TicketStore *ticketStore,
                                           AccessTokenProvider tokenProvider,
                                           QObject *parent = nullptr);

    // Completes only the Admin-owned backend step after the shared device
    // removal proof has succeeded. H5 termination never calls this service.
    void terminate(const QString &taskId, int keySessionId);

signals:
    void logAppended(const QString &text);
    void statusMessage(const QString &text);
    void terminationCompleted(const QString &taskId);

private:
    // Commits the Admin workflow only after the backend confirms termination.
    void handleSucceeded(const QString &taskId);

    // Retains a retryable record when the backend step fails after key cleanup.
    void handleFailed(const QString &taskId, const QString &reason);

    TicketStore *m_ticketStore;
    TicketTerminationClient *m_client;
    AccessTokenProvider m_tokenProvider;
    QSet<QString> m_inFlightTaskIds;
    QHash<QString, int> m_keySessionIds;
};

#endif // ADMINTICKETTERMINATIONSERVICE_H
