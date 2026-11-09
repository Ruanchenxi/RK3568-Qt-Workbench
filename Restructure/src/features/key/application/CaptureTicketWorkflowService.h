#ifndef CAPTURETICKETWORKFLOWSERVICE_H
#define CAPTURETICKETWORKFLOWSERVICE_H

#include <QObject>
#include <QJsonArray>
#include <QJsonObject>
#include <QHash>
#include <QList>
#include <QSet>
#include <QVariantList>

#include "features/key/application/CaptureTicketOperationContext.h"
#include "features/key/application/CaptureTicketUseCase.h"

enum class CaptureTicketWorkflowAction
{
    None,
    QueryTasks,
    ClaimTicket,
    SendTicket,
    ReadRfid,
    UploadResults,
    BeginVerifiedDelete,
    Complete,
    ReportFailure
};

struct CaptureTicketWorkflowEffect
{
    CaptureTicketWorkflowAction action = CaptureTicketWorkflowAction::None;
    int stationNo = 0;
    int operationId = -1;
    int keySessionId = 0;
    QString taskId;
    QByteArray taskIdRaw;
    QByteArray payload;
    QJsonArray taskLogItems;
    QString message;
    bool handled = false;
};

class CaptureTicketWorkflowService : public QObject
{
public:
    explicit CaptureTicketWorkflowService(QObject *parent = nullptr);

    // Acquires capture ownership and asks L1 for a fresh empty-key proof.
    CaptureTicketWorkflowEffect start(
            const CaptureTicketAdmissionRequest &request,
            int keySessionId,
            int operationId);

    // Applies every fresh Q_TASK observation, including disconnect recovery.
    CaptureTicketWorkflowEffect observeTasks(
            const QList<KeyTaskDto> &tasks,
            int currentKeySessionId);

    // Accepts the claimed JSON only after L4 encoding and L3 extraction succeed.
    CaptureTicketWorkflowEffect acceptClaim(
            const QJsonObject &claimData,
            const QString &taskId,
            const QByteArray &taskIdRaw,
            const QByteArray &payload,
            const QSet<int> &expectedInnerCodes,
            int operationId,
            int currentKeySessionId,
            bool keyReady);
    CaptureTicketWorkflowEffect rejectClaim(
            int operationId, const QString &reason);

    CaptureTicketWorkflowEffect finishDelivery(int currentKeySessionId);
    CaptureTicketWorkflowEffect failDelivery(const QString &reason);
    CaptureTicketWorkflowEffect acceptRfid(
            const QString &taskId,
            const QVariantList &records,
            int currentKeySessionId);
    CaptureTicketWorkflowEffect failRfid(
            const QString &taskId, const QString &reason);
    CaptureTicketWorkflowEffect finishUpload(
            const QString &taskId, int operationId);
    CaptureTicketWorkflowEffect failUpload(
            const QString &taskId, int operationId, const QString &reason);

    // Retains the claimed ticket while dropping all proof tied to the removed key.
    CaptureTicketWorkflowEffect interrupt(const QString &reason);
    CaptureTicketWorkflowEffect resumeAfterReconnect(int currentKeySessionId);
    CaptureTicketWorkflowEffect completeVerifiedDelete(
            const QString &taskId, bool success, const QString &reason = QString());

    bool hasActiveOperation() const;
    // Foreground ownership excludes parked tickets waiting for another key session.
    bool hasForegroundOperation() const;
    bool hasPendingStation(int stationNo) const;
    bool ownsTicketTransfer() const;
    bool isAwaitingDelete() const;
    bool ownsTask(const QString &taskId) const;

    // Forgets a manually deleted capture only after shared DEL/Q_TASK proof
    // confirms that the physical key no longer contains the task.
    bool forgetVerifiedDeletedTask(const QString &taskId);
    // Exposes process-local presentation state without creating recovery data.
    CaptureTicketPhase phaseForTask(const QString &taskId) const;
    QString activeTaskId() const;

private:
    CaptureTicketWorkflowEffect effect(
            CaptureTicketWorkflowAction action,
            const QString &message = QString()) const;
    QJsonArray accumulatedLogItems() const;
    void parkCurrent(CaptureTicketPhase phase);
    CaptureTicketWorkflowEffect resumeTrackedTask(
            const QList<KeyTaskDto> &tasks,
            int currentKeySessionId);
    bool hasUnreadTrackedTask(int currentKeySessionId) const;

    CaptureTicketOperationContext m_operation;
    QHash<QString, CaptureTicketOperationContext> m_trackedOperations;
};

#endif // CAPTURETICKETWORKFLOWSERVICE_H
