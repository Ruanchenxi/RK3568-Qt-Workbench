#include "features/key/application/TicketReturnUploadService.h"

#include "features/key/application/TicketReturnHttpClient.h"
#include "platform/logging/BusinessLogService.h"

namespace
{
void writeReturnUploadEvent(const QString &phase,
                            const QString &result,
                            const QString &taskId,
                            int keySessionId,
                            const QString &reason = QString(),
                            BusinessLogSeverity severity = BusinessLogSeverity::Info)
{
    BusinessLogEvent event;
    event.flow = QStringLiteral("return");
    event.phase = phase;
    event.result = result;
    event.taskId = taskId;
    event.slot = QStringLiteral("A");
    event.keySessionId = keySessionId;
    event.reason = reason;
    BusinessLogService::write(event, severity);
}
}

TicketReturnUploadService::TicketReturnUploadService(QObject *parent)
    : QObject(parent)
    , m_client(new TicketReturnHttpClient(this))
{
    connect(m_client, &TicketReturnHttpClient::logAppended,
            this, &TicketReturnUploadService::logAppended);
    connect(m_client, &TicketReturnHttpClient::uploadSucceeded,
            this, &TicketReturnUploadService::handleSucceeded);
    connect(m_client, &TicketReturnHttpClient::uploadFailed,
            this, &TicketReturnUploadService::handleFailed);
}

bool TicketReturnUploadService::isConfigured() const
{
    return m_client->isConfigured();
}

void TicketReturnUploadService::upload(const QJsonObject &payload,
                                       int keySessionId)
{
    const QString taskId = payload.value(QStringLiteral("taskId")).toString().trimmed();
    writeReturnUploadEvent(QStringLiteral("backend-request"),
                           QStringLiteral("started"),
                           taskId,
                           keySessionId);
    m_client->uploadReturnPayload(payload, keySessionId);
}

void TicketReturnUploadService::handleSucceeded(const QString &taskId,
                                                  int keySessionId)
{
    writeReturnUploadEvent(QStringLiteral("backend-response"),
                           QStringLiteral("completed"),
                           taskId,
                           keySessionId);
    emit uploadSucceeded(taskId, keySessionId);
}

void TicketReturnUploadService::handleFailed(const QString &taskId,
                                               int keySessionId,
                                               const QString &reason)
{
    writeReturnUploadEvent(QStringLiteral("backend-response"),
                           QStringLiteral("failed"),
                           taskId,
                           keySessionId,
                           reason,
                           BusinessLogSeverity::Error);
    emit uploadFailed(taskId, keySessionId, reason);
}
