#ifndef TICKETRETURNWORKFLOWSERVICE_H
#define TICKETRETURNWORKFLOWSERVICE_H

#include <QObject>
#include <QByteArray>
#include <QJsonObject>
#include <QList>
#include <QSet>
#include <QString>

#include "features/key/application/ProgressiveReturnOperationContext.h"
#include "features/key/application/TicketReturnOperationContext.h"
#include "features/key/application/TicketReturnUseCase.h"

class TicketStore;

enum class TicketReturnWorkflowAction
{
    None,
    SendSetCom,
    QueryTaskLog,
    UploadStandardLog,
    UploadProgressiveLog,
    ContinueProgressiveUploads,
    HandleProgressiveLog,
    BeginVerifiedDelete,
    AwaitVerifiedDelete,
    ContinueProgressiveSweep,
    StopProgressiveSweep,
    StopProgressiveSweepAndRequery,
    WaitForUpload
};

struct TicketReturnWorkflowEffect
{
    TicketReturnWorkflowAction action = TicketReturnWorkflowAction::None;
    QString taskId;
    QByteArray taskIdRaw;
    int keySessionId = 0;
    QJsonObject payload;
    QString statusMessage;
    bool handled = false;
};

struct TicketReturnWorkflowStartOutcome
{
    TicketReturnStartResult decision;
    TicketReturnWorkflowEffect effect;
};
// Owns all process-local state shared by ordinary and progressive return.
// L1 may ask business-named questions, but cannot mutate either context.
class TicketReturnWorkflowService : public QObject
{
public:
    explicit TicketReturnWorkflowService(TicketStore *store,
                                         QObject *parent = nullptr);

    // Evaluates admission without acquiring the return operation.
    TicketReturnStartResult evaluateStart(
            const TicketReturnStartRequest &request) const;
    // Keeps progressive and automatic-return decisions behind the L2 boundary.
    ProgressiveReturnSweepStartDecision evaluateProgressiveSweepStart(
            const ProgressiveReturnSweepStartRequest &request) const;
    ProgressiveReturnSweepNextDecision evaluateProgressiveSweepNext(
            const ProgressiveReturnSweepNextRequest &request) const;
    ProgressiveReturnCandidateDecision evaluateProgressiveCandidate(
            const ProgressiveReturnCandidateRequest &request) const;
    TicketReturnAutomaticCandidateDecision evaluateAutomaticCandidate(
            const TicketReturnAutomaticCandidateRequest &request) const;
    // Acquires one admitted ordinary return and selects its SET_COM effect.
    TicketReturnWorkflowStartOutcome startReturn(
            const TicketReturnStartRequest &request,
            int keySessionId);
    // Converts the correlated SET_COM ACK into a task-log query effect.
    TicketReturnWorkflowEffect acceptSetComAck(int currentKeySessionId);
    // Projects the I_TASK_LOG ACK only for the active physical-key session.
    TicketReturnWorkflowEffect acceptTaskLogAck(int currentKeySessionId);
    // Routes a parsed task log and acquires ordinary HTTP upload ownership.
    TicketReturnWorkflowEffect acceptTaskLog(
            const QJsonObject &payload,
            int currentKeySessionId);
    // Completes an ordinary upload only for its original task and key session.
    TicketReturnWorkflowEffect completeStandardUploadSuccess(
            const QString &taskId,
            int responseKeySessionId,
            int currentKeySessionId);
    TicketReturnWorkflowEffect completeStandardUploadFailure(
            const QString &taskId,
            int responseKeySessionId,
            const QString &reason);
    // Converts a link/protocol interruption into one explicit L1 action.
    TicketReturnWorkflowEffect interrupt(
            const QString &reason,
            int currentKeySessionId);
    // Reports whether an ordinary or progressive task currently owns return.
    bool hasActiveReturn() const;
    // Reports whether SET_COM confirmation is still pending for that task.
    bool hasPendingHandshake() const;
    // Reports whether an operation or progressive candidate sweep owns return.
    bool hasReturnWork() const;
    // Reports whether an HTTP task-log upload is in flight.
    bool hasStandardUpload() const;
    // Exposes operation identity only for typed command and log effects.
    QString activeTaskId() const;
    QByteArray activeTaskIdRaw() const;
    QString handshakeTaskId() const;
    QByteArray handshakeTaskIdRaw() const;
    int activeSessionId() const;
    QString chainTaskId() const;


    // Reports whether one progressive task or its candidate sweep is active.
    bool progressiveActive() const;
    bool progressiveUploadActive() const;
    bool progressiveCandidatesPending() const;
    int progressiveSweepSessionId() const;
    int progressiveTotalSteps() const;
    int progressiveStationNo() const;
    QString progressiveUploadTaskId() const;
    int progressiveUploadSerialNumber() const;
    int progressivePendingUploadCount() const;
    int progressiveObservedStepCount() const;
    int progressiveUploadedStepCount(const QString &taskId) const;
    QSet<int> progressiveObservedSteps() const;
    QSet<int> progressiveUploadedSteps(const QString &taskId) const;
    // Requires complete key and upload evidence before parent-ticket deletion.
    bool canStartProgressiveCleanup(
            const QByteArray &currentTaskIdRaw,
            quint8 currentTaskStatus,
            int currentKeySessionId) const;
    // Starts a same-session candidate sweep and records only validated tasks.
    void beginProgressiveSweep(int keySessionId);
    void addProgressiveCandidate(const QByteArray &taskIdRaw);
    QByteArray takeNextProgressiveCandidate();
    void clearProgressiveSweep();

    // Acquires a validated progressive task and selects its SET_COM effect.
    TicketReturnWorkflowEffect startProgressiveReturn(
            const QString &taskId,
            const QByteArray &taskIdRaw,
            int totalSteps,
            int keySessionId);

    // Validates and records one cumulative progressive task-log observation.
    TicketReturnWorkflowEffect acceptProgressiveTaskLog(
            const ProgressiveReturnLogRequest &request,
            int stationNo,
            const QList<QJsonObject> &items);
    // Selects upload, wait, or verified cleanup after comparing step evidence.
    TicketReturnWorkflowEffect nextProgressiveUpload(bool cleanupVerified);
    // Applies one correlated completed-step upload result.
    TicketReturnWorkflowEffect completeProgressiveUploadSuccess(
            const QString &taskId,
            int responseKeySessionId,
            int currentKeySessionId);
    TicketReturnWorkflowEffect completeProgressiveUploadFailure(
            const QString &taskId,
            int responseKeySessionId,
            int currentKeySessionId,
            const QString &reason);

    // Ends one progressive task and optionally its remaining candidate sweep.
    void clearProgressiveTask(bool clearCandidates);
    // Releases uploaded-step evidence only after verified physical deletion.
    void removeProgressiveUploadEvidence(const QString &taskId);

private:
    // Mutates operation-local state only after the public workflow methods
    // have selected a business action.
    void beginReturn(const QString &taskId,
                     const QByteArray &taskIdRaw,
                     int keySessionId);
    void completeHandshake();
    void beginStandardUpload();
    void completeStandardUpload();
    void clearReturn(bool handshakeOnly = false);
    void beginProgressiveTask(int totalSteps);
    void recordProgressiveTaskLog(int stationNo,
                                  const QList<QJsonObject> &items);
    QJsonObject takeNextProgressiveUploadItem();
    void beginProgressiveUpload(const QString &taskId, int serialNumber);
    void completeProgressiveUpload();
    void recordProgressiveUploadSuccess(const QString &taskId,
                                        int serialNumber);
    // Creates one side-effect-free instruction for the L1 event router.
    TicketReturnWorkflowEffect effect(
            TicketReturnWorkflowAction action,
            const QString &taskId = QString()) const;

    TicketStore *m_store;
    TicketReturnOperationContext m_operation;
    ProgressiveReturnOperationContext m_progressive;
};

#endif // TICKETRETURNWORKFLOWSERVICE_H
