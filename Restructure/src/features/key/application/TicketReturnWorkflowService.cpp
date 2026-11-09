#include "features/key/application/TicketReturnWorkflowService.h"

#include "features/key/application/TicketStore.h"

#include <QMap>
#include <QJsonArray>

TicketReturnWorkflowService::TicketReturnWorkflowService(
        TicketStore *store,
        QObject *parent)
    : QObject(parent)
    , m_store(store)
{
    Q_ASSERT(m_store);
}
TicketReturnStartResult TicketReturnWorkflowService::evaluateStart(
        const TicketReturnStartRequest &request) const
{
    return TicketReturnUseCase::evaluateStart(request);
}
ProgressiveReturnSweepStartDecision
TicketReturnWorkflowService::evaluateProgressiveSweepStart(
        const ProgressiveReturnSweepStartRequest &request) const
{
    return TicketReturnUseCase::evaluateProgressiveSweepStart(request);
}

ProgressiveReturnSweepNextDecision
TicketReturnWorkflowService::evaluateProgressiveSweepNext(
        const ProgressiveReturnSweepNextRequest &request) const
{
    return TicketReturnUseCase::evaluateProgressiveSweepNext(request);
}

ProgressiveReturnCandidateDecision
TicketReturnWorkflowService::evaluateProgressiveCandidate(
        const ProgressiveReturnCandidateRequest &request) const
{
    return TicketReturnUseCase::evaluateProgressiveCandidate(request);
}

TicketReturnAutomaticCandidateDecision
TicketReturnWorkflowService::evaluateAutomaticCandidate(
        const TicketReturnAutomaticCandidateRequest &request) const
{
    return TicketReturnUseCase::evaluateAutomaticCandidate(request);
}

TicketReturnWorkflowStartOutcome TicketReturnWorkflowService::startReturn(
        const TicketReturnStartRequest &request,
        int keySessionId)
{
    TicketReturnWorkflowStartOutcome outcome;
    outcome.decision = evaluateStart(request);
    if (!outcome.decision.allowed) {
        return outcome;
    }

    beginReturn(request.taskId, request.keyTaskIdRaw, keySessionId);
    m_store->updateReturnState(request.taskId, TicketReturnState::Handshake);
    outcome.effect = effect(
                TicketReturnWorkflowAction::SendSetCom, request.taskId);
    outcome.effect.handled = true;
    outcome.effect.keySessionId = keySessionId;
    outcome.effect.statusMessage = QStringLiteral(
                "\u5F00\u59CB\u56DE\u4F20\uFF0C"
                "\u6B63\u5728\u91CD\u65B0\u786E\u8BA4\u94A5\u5319\u901A\u4FE1\uFF1A%1")
            .arg(request.taskId);
    return outcome;
}

TicketReturnWorkflowEffect TicketReturnWorkflowService::acceptSetComAck(
        int currentKeySessionId)
{
    if (!hasPendingHandshake()
            || handshakeTaskIdRaw().isEmpty()
            || activeSessionId() <= 0
            || activeSessionId() != currentKeySessionId) {
        return effect(TicketReturnWorkflowAction::None);
    }

    TicketReturnWorkflowEffect result = effect(
                TicketReturnWorkflowAction::QueryTaskLog,
                handshakeTaskId());
    result.taskIdRaw = handshakeTaskIdRaw();
    result.keySessionId = activeSessionId();
    result.handled = true;
    result.statusMessage = QStringLiteral(
                "\u56DE\u4F20\u524D\u63E1\u624B\u5DF2\u786E\u8BA4\uFF0C"
                "\u5F00\u59CB\u8BFB\u53D6\u94A5\u5319\u56DE\u4F20\u65E5\u5FD7\uFF1A%1")
            .arg(result.taskId);
    m_store->updateReturnState(result.taskId, TicketReturnState::LogRequesting);
    completeHandshake();
    return result;
}

TicketReturnWorkflowEffect TicketReturnWorkflowService::acceptTaskLogAck(
        int currentKeySessionId)
{
    TicketReturnWorkflowEffect result = effect(
                TicketReturnWorkflowAction::None, activeTaskId());
    if (!hasActiveReturn()
            || activeSessionId() <= 0
            || activeSessionId() != currentKeySessionId) {
        return result;
    }
    m_store->updateReturnState(activeTaskId(), TicketReturnState::LogReceiving);
    result.handled = true;
    return result;
}

TicketReturnWorkflowEffect TicketReturnWorkflowService::acceptTaskLog(
        const QJsonObject &payload,
        int currentKeySessionId)
{
    TicketReturnLogRequest request;
    request.progressiveReturnActive = progressiveActive();
    request.returnActive = hasActiveReturn();
    request.sameKeySession = activeSessionId() > 0
            && activeSessionId() == currentKeySessionId;
    const TicketReturnLogDecision decision =
            TicketReturnUseCase::evaluateTaskLog(request);

    TicketReturnWorkflowEffect result;
    result.handled = true;
    result.statusMessage = decision.message;
    if (decision.action == TicketReturnLogAction::HandleProgressive) {
        result.action = TicketReturnWorkflowAction::HandleProgressiveLog;
        return result;
    }
    if (decision.action == TicketReturnLogAction::RejectNoActiveReturn) {
        return result;
    }

    beginStandardUpload();
    m_store->updateReturnState(activeTaskId(), TicketReturnState::Uploading);
    result = effect(
                TicketReturnWorkflowAction::UploadStandardLog,
                activeTaskId());
    result.handled = true;
    result.keySessionId = activeSessionId();
    result.payload = payload;
    result.statusMessage = QStringLiteral(
                "\u5DF2\u89E3\u6790\u94A5\u5319\u56DE\u4F20\u65E5\u5FD7\uFF0C"
                "\u51C6\u5907\u4E0A\u4F20\u5230\u670D\u52A1\u7AEF");
    return result;
}

TicketReturnWorkflowEffect
TicketReturnWorkflowService::completeStandardUploadSuccess(
        const QString &taskId,
        int responseKeySessionId,
        int currentKeySessionId)
{
    TicketReturnWorkflowEffect result;
    const bool matches = hasActiveReturn()
            && hasStandardUpload()
            && taskId == activeTaskId()
            && responseKeySessionId > 0
            && responseKeySessionId == activeSessionId();
    if (!matches) {
        return result;
    }

    completeStandardUpload();
    TicketReturnUploadSuccessRequest request;
    request.sameKeySession = responseKeySessionId == currentKeySessionId;
    request.hasRawTaskId = !activeTaskIdRaw().isEmpty();
    const TicketReturnUploadSuccessDecision decision =
            TicketReturnUseCase::evaluateUploadSuccess(request);

    result = effect(TicketReturnWorkflowAction::None, taskId);
    result.handled = true;
    result.keySessionId = responseKeySessionId;
    result.statusMessage = decision.message;
    if (decision.action
            == TicketReturnUploadSuccessAction::StartVerifiedKeyCleanup) {
        result.action = request.sameKeySession
                ? TicketReturnWorkflowAction::BeginVerifiedDelete
                : TicketReturnWorkflowAction::AwaitVerifiedDelete;
        result.taskIdRaw = activeTaskIdRaw();
        if (request.sameKeySession) {
            result.statusMessage = QStringLiteral(
                        "\u56DE\u4F20\u4E0A\u4F20\u6210\u529F\uFF0C"
                        "\u5F00\u59CB\u5220\u9664\u94A5\u5319\u4E2D\u7684\u5DF2\u5B8C\u6210\u4EFB\u52A1");
        } else {
            result.keySessionId = 0;
            result.statusMessage = QStringLiteral(
                        "\u56DE\u4F20\u4E0A\u4F20\u6210\u529F\uFF0C"
                        "\u7B49\u5F85\u94A5\u5319\u91CD\u65B0\u5728\u4F4D\u540E\u6839\u636E Q_TASK \u7EE7\u7EED\u6E05\u7406");
        }
    } else {
        m_store->updateReturnState(
                    taskId, TicketReturnState::ManualRequired,
                    decision.message);
    }
    clearReturn();
    return result;
}

TicketReturnWorkflowEffect
TicketReturnWorkflowService::completeStandardUploadFailure(
        const QString &taskId,
        int responseKeySessionId,
        const QString &reason)
{
    TicketReturnWorkflowEffect result;
    const bool matches = hasActiveReturn()
            && hasStandardUpload()
            && taskId == activeTaskId()
            && responseKeySessionId > 0
            && responseKeySessionId == activeSessionId();
    if (!matches) {
        return result;
    }

    completeStandardUpload();
    m_store->updateReturnState(
                taskId, TicketReturnState::ManualRequired, reason);
    result = effect(TicketReturnWorkflowAction::None, taskId);
    result.handled = true;
    result.keySessionId = responseKeySessionId;
    result.statusMessage = QStringLiteral("\u56DE\u4F20\u4E0A\u4F20\u5931\u8D25\uFF1A%1")
            .arg(reason);
    clearReturn();
    return result;
}

TicketReturnWorkflowEffect TicketReturnWorkflowService::interrupt(
        const QString &reason,
        int currentKeySessionId)
{
    TicketReturnWorkflowEffect result;
    if (!hasReturnWork()) {
        return result;
    }

    const QString taskId = hasActiveReturn()
            ? activeTaskId() : handshakeTaskId();
    TicketReturnInterruptionRequest request;
    request.reason = reason;
    request.progressiveActive = progressiveActive();
    request.standardHttpUploadActive = hasStandardUpload();
    request.progressiveHttpUploadActive = progressiveUploadActive();
    request.sameKeySession = activeSessionId() > 0
            && activeSessionId() == currentKeySessionId;
    const TicketReturnInterruptionDecision decision =
            TicketReturnUseCase::evaluateInterruption(request);

    if (!taskId.isEmpty()) {
        m_store->updateReturnState(taskId,
                                   decision.ticketState,
                                   decision.ticketError,
                                   decision.ticketOutcome);
    }
    result = effect(TicketReturnWorkflowAction::None, taskId);
    result.handled = true;
    result.statusMessage = decision.statusMessage;
    switch (decision.action) {
    case TicketReturnInterruptionAction::WaitForStandardUpload:
        result.action = TicketReturnWorkflowAction::WaitForUpload;
        break;
    case TicketReturnInterruptionAction::WaitForProgressiveUpload:
        clearProgressiveSweep();
        result.action = TicketReturnWorkflowAction::WaitForUpload;
        break;
    case TicketReturnInterruptionAction::ContinueProgressiveSweep:
        result.action = TicketReturnWorkflowAction::ContinueProgressiveSweep;
        break;
    case TicketReturnInterruptionAction::StopProgressiveReturn:
        result.action = TicketReturnWorkflowAction::StopProgressiveSweep;
        break;
    case TicketReturnInterruptionAction::ClearStandardReturn:
        clearReturn();
        break;
    }
    return result;
}

TicketReturnWorkflowEffect TicketReturnWorkflowService::effect(
        TicketReturnWorkflowAction action,
        const QString &taskId) const
{
    TicketReturnWorkflowEffect result;
    result.action = action;
    result.taskId = taskId;
    return result;
}

bool TicketReturnWorkflowService::hasActiveReturn() const
{
    return m_operation.isActive();
}

bool TicketReturnWorkflowService::hasPendingHandshake() const
{
    return m_operation.hasPendingHandshake();
}

bool TicketReturnWorkflowService::hasReturnWork() const
{
    return hasActiveReturn()
            || hasPendingHandshake()
            || m_operation.hasHttpUpload()
            || m_progressive.active
            || m_progressive.hasHttpUpload()
            || !m_progressive.candidateTaskIds.isEmpty();
}

bool TicketReturnWorkflowService::hasStandardUpload() const
{
    return m_operation.hasHttpUpload();
}

QString TicketReturnWorkflowService::activeTaskId() const
{
    return m_operation.taskId;
}

QByteArray TicketReturnWorkflowService::activeTaskIdRaw() const
{
    return m_operation.taskIdRaw;
}

QString TicketReturnWorkflowService::handshakeTaskId() const
{
    return m_operation.handshakeTaskId;
}

QByteArray TicketReturnWorkflowService::handshakeTaskIdRaw() const
{
    return m_operation.handshakeTaskIdRaw;
}

int TicketReturnWorkflowService::activeSessionId() const
{
    return m_operation.keySessionId;
}

QString TicketReturnWorkflowService::chainTaskId() const
{
    const QString taskId = m_operation.chainTaskId();
    return taskId.isEmpty() ? m_progressive.httpTaskId : taskId;
}

void TicketReturnWorkflowService::beginReturn(
        const QString &taskId,
        const QByteArray &taskIdRaw,
        int keySessionId)
{
    m_operation.begin(taskId, taskIdRaw, keySessionId);
}

void TicketReturnWorkflowService::completeHandshake()
{
    m_operation.clearHandshake();
}

void TicketReturnWorkflowService::beginStandardUpload()
{
    m_operation.beginHttpUpload();
}

void TicketReturnWorkflowService::completeStandardUpload()
{
    m_operation.clearHttpUpload();
}

void TicketReturnWorkflowService::clearReturn(bool handshakeOnly)
{
    if (handshakeOnly) {
        m_operation.clearHandshake();
    } else {
        m_operation.clear();
    }
}

bool TicketReturnWorkflowService::progressiveActive() const
{
    return m_progressive.active;
}

bool TicketReturnWorkflowService::progressiveUploadActive() const
{
    return m_progressive.hasHttpUpload();
}

bool TicketReturnWorkflowService::progressiveCandidatesPending() const
{
    return !m_progressive.candidateTaskIds.isEmpty();
}

int TicketReturnWorkflowService::progressiveSweepSessionId() const
{
    return m_progressive.sweepSessionId;
}

int TicketReturnWorkflowService::progressiveTotalSteps() const
{
    return m_progressive.totalSteps;
}

int TicketReturnWorkflowService::progressiveStationNo() const
{
    return m_progressive.stationNo;
}

QString TicketReturnWorkflowService::progressiveUploadTaskId() const
{
    return m_progressive.httpTaskId;
}

int TicketReturnWorkflowService::progressiveUploadSerialNumber() const
{
    return m_progressive.httpSerialNumber;
}

int TicketReturnWorkflowService::progressivePendingUploadCount() const
{
    return m_progressive.pendingItems.size();
}

int TicketReturnWorkflowService::progressiveObservedStepCount() const
{
    return m_progressive.observedCompletedSteps.size();
}

int TicketReturnWorkflowService::progressiveUploadedStepCount(
        const QString &taskId) const
{
    return m_progressive.uploadedSteps.value(taskId).size();
}

QSet<int> TicketReturnWorkflowService::progressiveObservedSteps() const
{
    return m_progressive.observedCompletedSteps;
}

QSet<int> TicketReturnWorkflowService::progressiveUploadedSteps(
        const QString &taskId) const
{
    return m_progressive.uploadedSteps.value(taskId);
}

bool TicketReturnWorkflowService::canStartProgressiveCleanup(
        const QByteArray &currentTaskIdRaw,
        quint8 currentTaskStatus,
        int currentKeySessionId) const
{
    ProgressiveReturnCleanupRequest request;
    request.contextActive = progressiveActive();
    request.totalSteps = progressiveTotalSteps();
    request.sameKeySession = activeSessionId() > 0
            && activeSessionId() == currentKeySessionId;
    request.observedCompletedSteps = progressiveObservedSteps();
    request.uploadedSteps = progressiveUploadedSteps(activeTaskId());
    request.sameKeyTaskIdentity = !currentTaskIdRaw.isEmpty()
            && currentTaskIdRaw == activeTaskIdRaw();
    request.keyTaskCompleted = currentTaskStatus == 0x02;
    return TicketReturnUseCase::evaluateProgressiveCleanup(request).allowed;
}
void TicketReturnWorkflowService::beginProgressiveSweep(int keySessionId)
{
    m_progressive.clearSweep();
    m_progressive.sweepSessionId = keySessionId;
}

void TicketReturnWorkflowService::addProgressiveCandidate(
        const QByteArray &taskIdRaw)
{
    if (!m_progressive.candidateTaskIds.contains(taskIdRaw)) {
        m_progressive.candidateTaskIds.append(taskIdRaw);
    }
}

QByteArray TicketReturnWorkflowService::takeNextProgressiveCandidate()
{
    return m_progressive.candidateTaskIds.isEmpty()
            ? QByteArray()
            : m_progressive.candidateTaskIds.takeFirst();
}

void TicketReturnWorkflowService::clearProgressiveSweep()
{
    m_progressive.clearSweep();
}

void TicketReturnWorkflowService::beginProgressiveTask(int totalSteps)
{
    m_progressive.beginTask(totalSteps);
}
TicketReturnWorkflowEffect TicketReturnWorkflowService::startProgressiveReturn(
        const QString &taskId,
        const QByteArray &taskIdRaw,
        int totalSteps,
        int keySessionId)
{
    beginProgressiveTask(totalSteps);
    beginReturn(taskId, taskIdRaw, keySessionId);
    m_store->updateReturnState(taskId, TicketReturnState::Handshake);
    TicketReturnWorkflowEffect result = effect(
                TicketReturnWorkflowAction::SendSetCom, taskId);
    result.handled = true;
    result.keySessionId = keySessionId;
    result.statusMessage = QStringLiteral(
                "\u9010\u6B65\u56DE\u4F20\uFF1A"
                "\u5F00\u59CB\u8BFB\u53D6\u4EFB\u52A1 %1 "
                "\u7684\u7D2F\u8BA1\u6B65\u9AA4\u65E5\u5FD7")
            .arg(taskId);
    return result;
}

void TicketReturnWorkflowService::recordProgressiveTaskLog(
        int stationNo,
        const QList<QJsonObject> &items)
{
    m_progressive.stationNo = stationNo;
    m_progressive.observedCompletedSteps.clear();
    m_progressive.pendingItems.clear();

    const QSet<int> uploaded =
            m_progressive.uploadedSteps.value(m_operation.taskId);
    QMap<int, QJsonObject> completedBySerial;
    for (const QJsonObject &item : items) {
        const int serialNumber =
                item.value(QStringLiteral("serialNumber")).toInt(-1);
        const int opStatus = item.value(QStringLiteral("opStatus")).toInt(-1);
        if (opStatus != 1
                || serialNumber < 0
                || serialNumber >= m_progressive.totalSteps) {
            continue;
        }
        completedBySerial.insert(serialNumber, item);
    }
    for (auto it = completedBySerial.cbegin();
         it != completedBySerial.cend(); ++it) {
        const int serialNumber = it.key();
        m_progressive.observedCompletedSteps.insert(serialNumber);
        if (!uploaded.contains(serialNumber)) {
            m_progressive.pendingItems.append(it.value());
        }
    }
}

TicketReturnWorkflowEffect TicketReturnWorkflowService::acceptProgressiveTaskLog(
        const ProgressiveReturnLogRequest &request,
        int stationNo,
        const QList<QJsonObject> &items)
{
    const ProgressiveReturnLogDecision decision =
            TicketReturnUseCase::evaluateProgressiveTaskLog(request);
    TicketReturnWorkflowEffect result = effect(
                TicketReturnWorkflowAction::ContinueProgressiveUploads,
                activeTaskId());
    result.handled = true;
    if (!decision.accepted) {
        if (!activeTaskId().isEmpty()) {
            m_store->updateReturnState(
                        activeTaskId(),
                        TicketReturnState::Idle,
                        decision.message,
                        TicketReturnOutcome::RetryableFailure);
        }
        result.action = TicketReturnWorkflowAction::ContinueProgressiveSweep;
        result.statusMessage = decision.message;
        return result;
    }

    recordProgressiveTaskLog(stationNo, items);
    return result;
}

TicketReturnWorkflowEffect TicketReturnWorkflowService::nextProgressiveUpload(
        bool cleanupVerified)
{
    ProgressiveReturnNextRequest request;
    request.taskId = activeTaskId();
    request.contextActive = progressiveActive();
    request.httpUploadActive = progressiveUploadActive();
    request.pendingUploadCount = progressivePendingUploadCount();
    request.uploadedStepCount = progressiveUploadedStepCount(activeTaskId());
    request.totalSteps = progressiveTotalSteps();
    request.cleanupVerified = cleanupVerified;
    const ProgressiveReturnNextDecision decision =
            TicketReturnUseCase::evaluateProgressiveNext(request);

    TicketReturnWorkflowEffect result = effect(
                TicketReturnWorkflowAction::None, request.taskId);
    result.handled = request.contextActive;
    switch (decision.action) {
    case ProgressiveReturnNextAction::NoAction:
        return result;
    case ProgressiveReturnNextAction::StartVerifiedCleanup:
        result.action = TicketReturnWorkflowAction::BeginVerifiedDelete;
        result.taskIdRaw = activeTaskIdRaw();
        result.keySessionId = activeSessionId();
        result.statusMessage = QStringLiteral(
                    "\u4EFB\u52A1 %1 \u7684\u5168\u90E8 %2 "
                    "\u6B65\u5747\u5DF2\u56DE\u4F20\u6210\u529F\uFF0C"
                    "\u5F00\u59CB\u5220\u9664\u7236\u7968")
                .arg(request.taskId)
                .arg(progressiveTotalSteps());
        return result;
    case ProgressiveReturnNextAction::WaitForRemainingSteps:
        m_store->updateReturnState(request.taskId,
                                   decision.ticketState,
                                   decision.ticketError,
                                   decision.ticketOutcome);
        result.action = TicketReturnWorkflowAction::ContinueProgressiveSweep;
        result.statusMessage = decision.statusMessage;
        return result;
    case ProgressiveReturnNextAction::UploadNextCompletedStep:
        break;
    }

    QJsonObject item;
    int serialNumber = -1;
    while (progressivePendingUploadCount() > 0) {
        item = takeNextProgressiveUploadItem();
        serialNumber = item.value(QStringLiteral("serialNumber")).toInt(-1);
        if (serialNumber >= 0 && serialNumber < progressiveTotalSteps()) {
            break;
        }
        item = QJsonObject();
    }
    if (item.isEmpty()) {
        return nextProgressiveUpload(cleanupVerified);
    }

    QJsonObject payload;
    payload.insert(QStringLiteral("stationNo"), progressiveStationNo());
    payload.insert(QStringLiteral("steps"), progressiveTotalSteps());
    payload.insert(QStringLiteral("taskId"), activeTaskId());
    QJsonArray taskLogItems;
    taskLogItems.append(item);
    payload.insert(QStringLiteral("taskLogItems"), taskLogItems);

    beginProgressiveUpload(activeTaskId(), serialNumber);
    m_store->updateReturnState(activeTaskId(), TicketReturnState::Uploading);
    result.action = TicketReturnWorkflowAction::UploadProgressiveLog;
    result.payload = payload;
    result.keySessionId = activeSessionId();
    result.statusMessage = QStringLiteral(
                "\u4EFB\u52A1 %1 \u6B63\u5728\u56DE\u4F20\u7B2C %2/%3 \u6B65")
            .arg(activeTaskId())
            .arg(serialNumber + 1)
            .arg(progressiveTotalSteps());
    return result;
}

TicketReturnWorkflowEffect
TicketReturnWorkflowService::completeProgressiveUploadSuccess(
        const QString &taskId,
        int responseKeySessionId,
        int currentKeySessionId)
{
    TicketReturnWorkflowEffect result;
    const bool matches = progressiveUploadActive()
            && hasActiveReturn()
            && taskId == progressiveUploadTaskId()
            && taskId == activeTaskId()
            && responseKeySessionId > 0
            && responseKeySessionId == activeSessionId();
    if (!matches) {
        return result;
    }

    ProgressiveReturnUploadSuccessRequest request;
    request.responseTaskId = taskId;
    request.httpTaskId = progressiveUploadTaskId();
    request.serialNumber = progressiveUploadSerialNumber();
    request.totalSteps = progressiveTotalSteps();
    request.contextActive = progressiveActive();
    request.activeTaskId = activeTaskId();
    request.sameKeySession = responseKeySessionId == currentKeySessionId;
    const ProgressiveReturnUploadSuccessDecision decision =
            TicketReturnUseCase::evaluateProgressiveUploadSuccess(request);
    if (decision.action
            == ProgressiveReturnUploadSuccessAction::NotProgressiveUpload) {
        return result;
    }

    completeProgressiveUpload();
    if (decision.recordUploadedStep) {
        recordProgressiveUploadSuccess(taskId, request.serialNumber);
    }
    TicketReturnWorkflowAction action =
            TicketReturnWorkflowAction::ContinueProgressiveUploads;
    if (decision.action
            == ProgressiveReturnUploadSuccessAction::StopProgressiveReturn) {
        action = decision.recordUploadedStep && !request.sameKeySession
                ? TicketReturnWorkflowAction::StopProgressiveSweepAndRequery
                : TicketReturnWorkflowAction::StopProgressiveSweep;
    }
    result = effect(action, taskId);
    result.handled = true;
    result.statusMessage = decision.statusMessage;
    return result;
}

TicketReturnWorkflowEffect
TicketReturnWorkflowService::completeProgressiveUploadFailure(
        const QString &taskId,
        int responseKeySessionId,
        int currentKeySessionId,
        const QString &reason)
{
    TicketReturnWorkflowEffect result;
    const bool matches = progressiveUploadActive()
            && hasActiveReturn()
            && taskId == progressiveUploadTaskId()
            && taskId == activeTaskId()
            && responseKeySessionId > 0
            && responseKeySessionId == activeSessionId();
    if (!matches) {
        return result;
    }

    ProgressiveReturnUploadFailureRequest request;
    request.responseTaskId = taskId;
    request.httpTaskId = progressiveUploadTaskId();
    request.reason = reason;
    const ProgressiveReturnUploadFailureDecision decision =
            TicketReturnUseCase::evaluateProgressiveUploadFailure(request);
    if (decision.action
            == ProgressiveReturnUploadFailureAction::NotProgressiveUpload) {
        return result;
    }

    completeProgressiveUpload();
    m_store->updateReturnState(taskId,
                               decision.ticketState,
                               decision.ticketError,
                               decision.ticketOutcome);
    result = effect(
                responseKeySessionId == currentKeySessionId
                    ? TicketReturnWorkflowAction::ContinueProgressiveSweep
                    : TicketReturnWorkflowAction::StopProgressiveSweep,
                taskId);
    result.handled = true;
    result.statusMessage = decision.statusMessage;
    return result;
}
QJsonObject TicketReturnWorkflowService::takeNextProgressiveUploadItem()
{
    return m_progressive.pendingItems.isEmpty()
            ? QJsonObject()
            : m_progressive.pendingItems.takeFirst();
}

void TicketReturnWorkflowService::beginProgressiveUpload(
        const QString &taskId,
        int serialNumber)
{
    m_progressive.httpTaskId = taskId;
    m_progressive.httpSerialNumber = serialNumber;
}

void TicketReturnWorkflowService::completeProgressiveUpload()
{
    m_progressive.clearHttpUpload();
}

void TicketReturnWorkflowService::recordProgressiveUploadSuccess(
        const QString &taskId,
        int serialNumber)
{
    if (serialNumber >= 0) {
        m_progressive.uploadedSteps[taskId].insert(serialNumber);
    }
}

void TicketReturnWorkflowService::clearProgressiveTask(
        bool clearCandidates)
{
    m_progressive.clearTask(clearCandidates);
    m_operation.clear();
}

void TicketReturnWorkflowService::removeProgressiveUploadEvidence(
        const QString &taskId)
{
    m_progressive.uploadedSteps.remove(taskId);
}
