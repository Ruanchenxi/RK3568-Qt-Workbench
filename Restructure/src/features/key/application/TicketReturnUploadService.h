#ifndef TICKETRETURNUPLOADSERVICE_H
#define TICKETRETURNUPLOADSERVICE_H

#include <QObject>
#include <QJsonObject>
#include <QString>

class TicketReturnHttpClient;

// Application boundary for uploading parsed key task logs. The controller
// decides when to upload; this service owns HTTP transport and boundary logs.
class TicketReturnUploadService : public QObject
{
    Q_OBJECT
public:
    explicit TicketReturnUploadService(QObject *parent = nullptr);

    bool isConfigured() const;

    // Correlates the backend result with the physical-key session that
    // produced the payload without persisting ticket data.
    void upload(const QJsonObject &payload, int keySessionId);

signals:
    void logAppended(const QString &text);
    void uploadSucceeded(const QString &taskId, int keySessionId);
    void uploadFailed(const QString &taskId, int keySessionId,
                      const QString &reason);

private:
    // Completes logging before forwarding the transport result to the workflow.
    void handleSucceeded(const QString &taskId, int keySessionId);
    void handleFailed(const QString &taskId, int keySessionId,
                      const QString &reason);

    TicketReturnHttpClient *m_client;
};

#endif // TICKETRETURNUPLOADSERVICE_H
