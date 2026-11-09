#include "features/key/application/CaptureTicketWorkflowService.h"

CaptureTicketWorkflowService::CaptureTicketWorkflowService(QObject *parent)
    : QObject(parent)
{
}

CaptureTicketWorkflowEffect CaptureTicketWorkflowService::start(
        const CaptureTicketAdmissionRequest &request,
        int keySessionId,
        int operationId)
{
    const CaptureTicketAdmissionDecision decision =
            CaptureTicketUseCase::evaluateAdmission(request);
    if (!decision.allowed) {
        CaptureTicketWorkflowEffect result;
        result.action = CaptureTicketWorkflowAction::ReportFailure;
        result.message = decision.message;
        result.handled = true;
        return result;
    }

    // Parked tickets remain tracked while one new claim uses the serial lane.
    m_operation.clear();
    m_operation.phase = CaptureTicketPhase::CheckingKeyCompatibility;
    m_operation.stationNo = request.stationNo;
    m_operation.keySessionId = keySessionId;
    m_operation.operationId = operationId;
    return effect(CaptureTicketWorkflowAction::QueryTasks,
                  QStringLiteral("\u6b63\u5728\u68c0\u67e5\u94a5\u5319\u4e2d\u7684\u73b0\u6709\u4efb\u52a1\u662f\u5426\u5141\u8bb8\u8ffd\u52a0\u91c7\u7801\u7968"));
}

CaptureTicketWorkflowEffect CaptureTicketWorkflowService::observeTasks(
        const QList<KeyTaskDto> &tasks,
        int currentKeySessionId)
{
    if (m_operation.phase == CaptureTicketPhase::CheckingKeyCompatibility) {
        if (currentKeySessionId != m_operation.keySessionId) {
            m_operation.clear();
            CaptureTicketWorkflowEffect result;
            result.action = CaptureTicketWorkflowAction::ReportFailure;
            result.message = QStringLiteral("\u94a5\u5319\u4f1a\u8bdd\u5df2\u53d8\u5316\uff0c\u672c\u6b21\u91c7\u7801\u7968\u9886\u53d6\u5df2\u53d6\u6d88");
            result.handled = true;
            return result;
        }
        for (const KeyTaskDto &task : tasks) {
            if (!CaptureTicketUseCase::isCaptureTaskType(task.type)) {
                m_operation.clear();
                CaptureTicketWorkflowEffect result;
                result.action = CaptureTicketWorkflowAction::ReportFailure;
                result.message = QStringLiteral("\u94a5\u5319\u4e2d\u5b58\u5728\u666e\u901a\u7968\u6216\u672a\u77e5\u7c7b\u578b\u4efb\u52a1\uff0c\u4e0d\u80fd\u8ffd\u52a0\u91c7\u7801\u7968");
                result.handled = true;
                return result;
            }
        }
        m_operation.phase = CaptureTicketPhase::Claiming;
        return effect(CaptureTicketWorkflowAction::ClaimTicket,
                      tasks.isEmpty()
                      ? QStringLiteral("\u6b63\u5728\u9886\u53d6\u91c7\u7801\u7968")
                      : QStringLiteral("\u5df2\u786e\u8ba4\u94a5\u5319\u4e2d\u73b0\u6709\u4efb\u52a1\u5747\u4e3a\u91c7\u7801\u7968\uff0c\u6b63\u5728\u8ffd\u52a0\u65b0\u4efb\u52a1"));
    }

    if (!m_operation.isActive()) {
        return resumeTrackedTask(tasks, currentKeySessionId);
    }
    if (!m_operation.hasClaim()) return CaptureTicketWorkflowEffect();

    const KeyTaskDto *task = CaptureTicketUseCase::findTask(
                tasks, m_operation.taskIdRaw);
    if (task && !CaptureTicketUseCase::isCaptureTaskType(task->type)) {
        CaptureTicketWorkflowEffect result = effect(
                    CaptureTicketWorkflowAction::ReportFailure,
                    QStringLiteral("Q_TASK \u4efb\u52a1\u7c7b\u578b\u4e0e\u91c7\u7801\u7968\u4e0d\u4e00\u81f4\uff0c\u5df2\u505c\u6b62\u81ea\u52a8\u5904\u7406"));
        m_operation.clear();
        return result;
    }
    if (m_operation.phase == CaptureTicketPhase::AwaitingDelete) {
        // Upload success is a fact, not a recoverable command stage. Every
        // fresh Q_TASK decides whether cleanup is already complete or DEL is
        // still required on the currently connected key.
        m_operation.keySessionId = currentKeySessionId;
        if (!task) {
            return completeVerifiedDelete(
                        m_operation.taskId, true,
                        QStringLiteral("Q_TASK \u5df2\u786e\u8ba4\u91c7\u7801\u4efb\u52a1\u4e0d\u5728\u94a5\u5319\u4e2d"));
        }
        return effect(CaptureTicketWorkflowAction::BeginVerifiedDelete,
                      QStringLiteral("\u91c7\u7801\u7ed3\u679c\u5df2\u88ab\u670d\u52a1\u7aef\u63a5\u6536\uff0cQ_TASK \u786e\u8ba4\u4efb\u52a1\u4ecd\u5728\u94a5\u5319\u4e2d\uff0c\u6b63\u5728\u91cd\u65b0\u6e05\u7968"));
    }
    if (!task) {
        m_operation.keySessionId = currentKeySessionId;
        m_operation.phase = CaptureTicketPhase::Delivering;
        m_operation.deliveryUnknownAfterDisconnect = false;
        return effect(CaptureTicketWorkflowAction::SendTicket,
                      QStringLiteral("\u94a5\u5319\u4e2d\u672a\u627e\u5230\u672c\u5730\u5df2\u9886\u53d6\u7684\u91c7\u7801\u7968\uff0c\u6b63\u5728\u7ee7\u7eed\u4e0b\u53d1"));
    }

    m_operation.keySessionId = currentKeySessionId;
    m_operation.deliveryUnknownAfterDisconnect = false;
    m_operation.deliveryConfirmed = true;
    m_operation.lastReadSessionId = currentKeySessionId;
    m_operation.phase = CaptureTicketPhase::ReadingRfid;
    return effect(CaptureTicketWorkflowAction::ReadRfid,
                  QStringLiteral("\u5df2\u786e\u8ba4\u91c7\u7801\u7968\u5b58\u5728\u4e8e\u94a5\u5319\u4e2d\uff0c\u6b63\u5728\u8bfb\u53d6 RFID"));
}

CaptureTicketWorkflowEffect CaptureTicketWorkflowService::acceptClaim(
        const QJsonObject &claimData,
        const QString &taskId,
        const QByteArray &taskIdRaw,
        const QByteArray &payload,
        const QSet<int> &expectedInnerCodes,
        int operationId,
        int currentKeySessionId,
        bool keyReady)
{
    if (m_operation.phase != CaptureTicketPhase::Claiming
            || operationId != m_operation.operationId) {
        return CaptureTicketWorkflowEffect();
    }
    if (claimData.value(QStringLiteral("stationNo")).toInt(-1)
            != m_operation.stationNo) {
        return rejectClaim(
                    operationId,
                    QStringLiteral("\u540e\u7aef\u8fd4\u56de\u7684\u7ad9\u53f7\u4e0e\u7528\u6237\u9009\u62e9\u4e0d\u4e00\u81f4"));
    }
    if (m_trackedOperations.contains(taskId)) {
        return rejectClaim(operationId,
                           QStringLiteral("\u540e\u7aef\u8fd4\u56de\u4e86\u5f53\u524d\u8fdb\u7a0b\u5df2\u8ddf\u8e2a\u7684\u91c7\u7801\u7968"));
    }
    if (taskId.isEmpty() || taskIdRaw.size() != 16 || payload.isEmpty()
            || expectedInnerCodes.isEmpty()) {
        return rejectClaim(operationId,
                           QStringLiteral("\u91c7\u7801\u7968\u7f16\u7801\u7ed3\u679c\u4e0d\u5b8c\u6574\uff0c\u5df2\u7981\u6b62\u4e0b\u53d1"));
    }

    m_operation.claimData = claimData;
    m_operation.taskId = taskId;
    m_operation.taskIdRaw = taskIdRaw;
    m_operation.payload = payload;
    m_operation.expectedInnerCodes = expectedInnerCodes;
    m_operation.keySessionId = currentKeySessionId;
    if (!keyReady) {
        CaptureTicketWorkflowEffect result = effect(
                    CaptureTicketWorkflowAction::None,
                    QStringLiteral("\u91c7\u7801\u7968\u5df2\u9886\u53d6\uff0c\u4f46\u94a5\u5319\u5f53\u524d\u672a\u5c31\u7eea\uff1b\u5df2\u4fdd\u7559\u5185\u5b58\u4e0a\u4e0b\u6587\uff0c\u91cd\u8fde\u540e\u7ee7\u7eed\u4e0b\u53d1"));
        parkCurrent(CaptureTicketPhase::WaitingForCapture);
        return result;
    }
    m_operation.phase = CaptureTicketPhase::Delivering;
    return effect(CaptureTicketWorkflowAction::SendTicket,
                  QStringLiteral("\u6b63\u5728\u5411\u94a5\u5319\u4e0b\u53d1\u91c7\u7801\u7968"));
}

CaptureTicketWorkflowEffect CaptureTicketWorkflowService::rejectClaim(
        int operationId, const QString &reason)
{
    if (m_operation.phase != CaptureTicketPhase::Claiming
            || operationId != m_operation.operationId) {
        return CaptureTicketWorkflowEffect();
    }
    m_operation.clear();
    CaptureTicketWorkflowEffect result;
    result.action = CaptureTicketWorkflowAction::ReportFailure;
    result.message = QStringLiteral("\u91c7\u7801\u7968\u9886\u53d6\u5931\u8d25\uff1a%1").arg(reason);
    result.handled = true;
    return result;
}

CaptureTicketWorkflowEffect CaptureTicketWorkflowService::finishDelivery(
        int currentKeySessionId)
{
    if (m_operation.phase != CaptureTicketPhase::Delivering
            || currentKeySessionId != m_operation.keySessionId) {
        return CaptureTicketWorkflowEffect();
    }
    m_operation.phase = CaptureTicketPhase::AwaitingDeliveryProof;
    return effect(CaptureTicketWorkflowAction::QueryTasks,
                  QStringLiteral("\u91c7\u7801\u7968\u62a5\u6587\u5df2\u53d1\u9001\uff0c\u6b63\u5728\u901a\u8fc7 Q_TASK \u786e\u8ba4"));
}

CaptureTicketWorkflowEffect CaptureTicketWorkflowService::failDelivery(
        const QString &reason)
{
    if (m_operation.phase != CaptureTicketPhase::Delivering) {
        return CaptureTicketWorkflowEffect();
    }
    m_operation.deliveryUnknownAfterDisconnect = true;
    CaptureTicketWorkflowEffect result = effect(
                CaptureTicketWorkflowAction::ReportFailure,
                QStringLiteral("\u91c7\u7801\u7968\u4e0b\u53d1\u672a\u5b8c\u6210\uff1a%1\uff1b\u5df2\u4fdd\u7559\u9886\u53d6\u7ed3\u679c\uff0c\u91cd\u8fde\u540e\u5c06\u7528 Q_TASK \u5bf9\u8d26").arg(reason));
    parkCurrent(CaptureTicketPhase::WaitingForCapture);
    return result;
}

CaptureTicketWorkflowEffect CaptureTicketWorkflowService::acceptRfid(
        const QString &taskId,
        const QVariantList &records,
        int currentKeySessionId)
{
    if (m_operation.phase != CaptureTicketPhase::ReadingRfid
            || taskId != m_operation.taskId
            || currentKeySessionId != m_operation.keySessionId) {
        return CaptureTicketWorkflowEffect();
    }
    for (const QVariant &value : records) {
        const QVariantMap record = value.toMap();
        bool ok = false;
        const int innerCode = record.value(QStringLiteral("innerCode")).toInt(&ok);
        const QString rfid = record.value(QStringLiteral("rfid"))
                .toString().trimmed().toUpper();
        if (ok && m_operation.expectedInnerCodes.contains(innerCode)
                && !rfid.isEmpty()) {
            QVariantMap normalized = record;
            normalized.insert(QStringLiteral("rfid"), rfid);
            const QString identity = QStringLiteral("%1|%2")
                    .arg(innerCode).arg(rfid);
            m_operation.recordsByIdentity.insert(identity, normalized);
        }
    }
    QVariantList accumulated;
    for (const QVariant &value : m_operation.recordsByIdentity) accumulated.append(value);
    const CaptureTicketRecordDecision decision =
            CaptureTicketUseCase::evaluateRecords(
                m_operation.expectedInnerCodes, accumulated);
    if (!decision.complete) {
        CaptureTicketWorkflowEffect result = effect(
                    CaptureTicketWorkflowAction::None, decision.message);
        parkCurrent(CaptureTicketPhase::WaitingForCapture);
        if (hasUnreadTrackedTask(currentKeySessionId)) {
            result.action = CaptureTicketWorkflowAction::QueryTasks;
            result.message += QStringLiteral("\uff1b\u6b63\u5728\u7ee7\u7eed\u68c0\u67e5\u5176\u4ed6\u91c7\u7801\u7968");
        }
        return result;
    }
    m_operation.phase = CaptureTicketPhase::Uploading;
    CaptureTicketWorkflowEffect result = effect(
                CaptureTicketWorkflowAction::UploadResults,
                QStringLiteral("\u91c7\u7801\u8bbe\u5907\u5df2\u5168\u90e8\u6536\u9f50\uff0c\u6b63\u5728\u4e0a\u4f20\u7ed3\u679c"));
    result.taskLogItems = accumulatedLogItems();
    return result;
}

CaptureTicketWorkflowEffect CaptureTicketWorkflowService::failRfid(
        const QString &taskId, const QString &reason)
{
    if (taskId != m_operation.taskId
            || m_operation.phase != CaptureTicketPhase::ReadingRfid) {
        return CaptureTicketWorkflowEffect();
    }
    CaptureTicketWorkflowEffect result = effect(
                CaptureTicketWorkflowAction::ReportFailure,
                QStringLiteral("\u91c7\u7801 RFID \u8bfb\u53d6\u5931\u8d25\uff1a%1\uff1b\u94a5\u5319\u4efb\u52a1\u5df2\u4fdd\u7559").arg(reason));
    parkCurrent(CaptureTicketPhase::WaitingForCapture);
    return result;
}

CaptureTicketWorkflowEffect CaptureTicketWorkflowService::finishUpload(
        const QString &taskId, int operationId)
{
    if (m_operation.phase != CaptureTicketPhase::Uploading
            || taskId != m_operation.taskId
            || operationId != m_operation.operationId) {
        return CaptureTicketWorkflowEffect();
    }
    m_operation.phase = CaptureTicketPhase::AwaitingDelete;
    return effect(CaptureTicketWorkflowAction::BeginVerifiedDelete,
                  QStringLiteral("\u91c7\u7801\u7ed3\u679c\u5df2\u4e0a\u4f20\uff0c\u6b63\u5728\u5220\u9664\u94a5\u5319\u4efb\u52a1"));
}

CaptureTicketWorkflowEffect CaptureTicketWorkflowService::failUpload(
        const QString &taskId, int operationId, const QString &reason)
{
    if (m_operation.phase != CaptureTicketPhase::Uploading
            || taskId != m_operation.taskId
            || operationId != m_operation.operationId) {
        return CaptureTicketWorkflowEffect();
    }
    CaptureTicketWorkflowEffect result = effect(
                CaptureTicketWorkflowAction::ReportFailure,
                QStringLiteral("\u91c7\u7801\u7ed3\u679c\u4e0a\u4f20\u5931\u8d25\uff1a%1\uff1b\u4e0d\u4f1a\u5220\u9664\u94a5\u5319\u4efb\u52a1").arg(reason));
    parkCurrent(CaptureTicketPhase::WaitingForCapture);
    return result;
}

CaptureTicketWorkflowEffect CaptureTicketWorkflowService::interrupt(
        const QString &reason)
{
    if (!m_operation.isActive()) {
        if (m_trackedOperations.isEmpty()) return CaptureTicketWorkflowEffect();
        CaptureTicketWorkflowEffect result;
        result.action = CaptureTicketWorkflowAction::None;
        result.message = QStringLiteral("\u94a5\u5319\u8fde\u63a5\u5df2\u4e2d\u65ad\uff0c%1 \u5f20\u91c7\u7801\u7968\u5df2\u4fdd\u7559\u5728\u5f53\u524d\u8fdb\u7a0b\u4e2d")
                .arg(m_trackedOperations.size());
        result.handled = true;
        return result;
    }
    if (!m_operation.hasClaim()
            && m_operation.phase != CaptureTicketPhase::Claiming) {
        m_operation.clear();
        CaptureTicketWorkflowEffect result;
        result.action = CaptureTicketWorkflowAction::ReportFailure;
        result.message = QStringLiteral("\u91c7\u7801\u7968\u6d41\u7a0b\u5df2\u4e2d\u65ad\uff1a%1").arg(reason);
        result.handled = true;
        return result;
    }
    if (m_operation.phase == CaptureTicketPhase::Claiming) {
        m_operation.keySessionId = 0;
        return effect(CaptureTicketWorkflowAction::None,
                      QStringLiteral("\u94a5\u5319\u5728\u9886\u53d6\u91c7\u7801\u7968\u671f\u95f4\u79fb\u9664\uff0c\u5c06\u4fdd\u7559 HTTP \u7ed3\u679c\uff0c\u91cd\u8fde\u540e\u7ee7\u7eed"));
    }
    if (m_operation.phase == CaptureTicketPhase::Delivering
            || m_operation.phase == CaptureTicketPhase::AwaitingDeliveryProof) {
        m_operation.deliveryUnknownAfterDisconnect = true;
    }
    m_operation.keySessionId = 0;
    if (m_operation.phase != CaptureTicketPhase::Uploading
            && m_operation.phase != CaptureTicketPhase::AwaitingDelete) {
        m_operation.phase = CaptureTicketPhase::WaitingForCapture;
    }
    return effect(CaptureTicketWorkflowAction::None,
                  QStringLiteral("\u94a5\u5319\u8fde\u63a5\u5df2\u4e2d\u65ad\uff0c\u91c7\u7801\u7968\u4e0a\u4e0b\u6587\u5df2\u4fdd\u7559\uff0c\u91cd\u8fde\u540e\u5c06\u81ea\u52a8\u5bf9\u8d26"));
}

CaptureTicketWorkflowEffect CaptureTicketWorkflowService::resumeAfterReconnect(
        int currentKeySessionId)
{
    if (m_operation.phase == CaptureTicketPhase::Uploading) {
        return CaptureTicketWorkflowEffect();
    }
    if (m_operation.hasClaim()) {
        m_operation.keySessionId = currentKeySessionId;
        return effect(CaptureTicketWorkflowAction::QueryTasks,
                      m_operation.phase == CaptureTicketPhase::AwaitingDelete
                          ? QStringLiteral("\u91c7\u7801\u7ed3\u679c\u5df2\u88ab\u670d\u52a1\u7aef\u63a5\u6536\uff0c\u901a\u8baf\u6062\u590d\u540e\u5148\u901a\u8fc7 Q_TASK \u6838\u5bf9\u94a5\u5319\u5b9e\u9645\u72b6\u6001")
                          : QStringLiteral("\u91c7\u7801\u7968\u901a\u8baf\u5df2\u6062\u590d\uff0c\u6b63\u5728\u901a\u8fc7 Q_TASK \u7ee7\u7eed\u5bf9\u8d26"));
    }
    if (!m_trackedOperations.isEmpty()) {
        CaptureTicketWorkflowEffect result;
        result.action = CaptureTicketWorkflowAction::QueryTasks;
        result.operationId = -1;
        result.keySessionId = currentKeySessionId;
        result.message = QStringLiteral("\u94a5\u5319\u901a\u8baf\u5df2\u6062\u590d\uff0c\u6b63\u5728\u68c0\u67e5 %1 \u5f20\u5f85\u5904\u7406\u91c7\u7801\u7968")
                .arg(m_trackedOperations.size());
        result.handled = true;
        return result;
    }
    return CaptureTicketWorkflowEffect();
}

CaptureTicketWorkflowEffect CaptureTicketWorkflowService::completeVerifiedDelete(
        const QString &taskId, bool success, const QString &reason)
{
    if (m_operation.phase != CaptureTicketPhase::AwaitingDelete
            || taskId != m_operation.taskId) {
        return CaptureTicketWorkflowEffect();
    }
    if (!success) {
        return effect(CaptureTicketWorkflowAction::ReportFailure,
                      QStringLiteral("\u91c7\u7801\u7ed3\u679c\u5df2\u4e0a\u4f20\uff0c\u4f46\u94a5\u5319\u5220\u7968\u672a\u5b8c\u6210\uff1a%1").arg(reason));
    }
    CaptureTicketWorkflowEffect result = effect(
                m_trackedOperations.isEmpty()
                    ? CaptureTicketWorkflowAction::Complete
                    : CaptureTicketWorkflowAction::QueryTasks,
                m_trackedOperations.isEmpty()
                    ? QStringLiteral("\u91c7\u7801\u7ed3\u679c\u5df2\u4e0a\u4f20\uff0c\u94a5\u5319\u4efb\u52a1\u5df2\u786e\u8ba4\u5220\u9664\uff1a%1").arg(taskId)
                    : QStringLiteral("\u91c7\u7801\u7968 %1 \u5df2\u5b8c\u6210\uff0c\u6b63\u5728\u7ee7\u7eed\u68c0\u67e5\u5176\u4ed6\u91c7\u7801\u7968").arg(taskId));
    m_operation.clear();
    return result;
}

bool CaptureTicketWorkflowService::hasActiveOperation() const
{
    return m_operation.isActive() || !m_trackedOperations.isEmpty();
}

bool CaptureTicketWorkflowService::hasForegroundOperation() const
{
    return m_operation.isActive();
}

bool CaptureTicketWorkflowService::hasPendingStation(int stationNo) const
{
    if (m_operation.isActive() && m_operation.stationNo == stationNo) return true;
    for (const CaptureTicketOperationContext &operation : m_trackedOperations) {
        if (operation.stationNo == stationNo) return true;
    }
    return false;
}

bool CaptureTicketWorkflowService::ownsTicketTransfer() const
{
    return m_operation.phase == CaptureTicketPhase::Delivering
            || m_operation.phase == CaptureTicketPhase::AwaitingDeliveryProof;
}

bool CaptureTicketWorkflowService::isAwaitingDelete() const
{
    return m_operation.phase == CaptureTicketPhase::AwaitingDelete;
}

bool CaptureTicketWorkflowService::ownsTask(const QString &taskId) const
{
    return !taskId.isEmpty()
            && (m_operation.taskId == taskId
                || m_trackedOperations.contains(taskId));
}

bool CaptureTicketWorkflowService::forgetVerifiedDeletedTask(
        const QString &taskId)
{
    const QString normalizedTaskId = taskId.trimmed();
    if (normalizedTaskId.isEmpty()) return false;

    bool removed = m_trackedOperations.remove(normalizedTaskId) > 0;
    if (m_operation.taskId == normalizedTaskId) {
        m_operation.clear();
        removed = true;
    }
    return removed;
}

CaptureTicketPhase CaptureTicketWorkflowService::phaseForTask(
        const QString &taskId) const
{
    if (taskId.isEmpty()) return CaptureTicketPhase::Idle;
    if (m_operation.taskId == taskId) return m_operation.phase;
    const auto it = m_trackedOperations.constFind(taskId);
    return it == m_trackedOperations.constEnd()
            ? CaptureTicketPhase::Idle : it.value().phase;
}

QString CaptureTicketWorkflowService::activeTaskId() const
{
    return m_operation.taskId;
}

CaptureTicketWorkflowEffect CaptureTicketWorkflowService::effect(
        CaptureTicketWorkflowAction action,
        const QString &message) const
{
    CaptureTicketWorkflowEffect result;
    result.action = action;
    result.stationNo = m_operation.stationNo;
    result.operationId = m_operation.operationId;
    result.keySessionId = m_operation.keySessionId;
    result.taskId = m_operation.taskId;
    result.taskIdRaw = m_operation.taskIdRaw;
    result.payload = m_operation.payload;
    result.message = message;
    result.handled = true;
    return result;
}

QJsonArray CaptureTicketWorkflowService::accumulatedLogItems() const
{
    QJsonArray items;
    for (const QVariant &value : m_operation.recordsByIdentity) {
        const QVariantMap record = value.toMap();
        QJsonObject item;
        item.insert(QStringLiteral("innerCode"),
                    record.value(QStringLiteral("innerCode")).toInt());
        item.insert(QStringLiteral("rfid"),
                    record.value(QStringLiteral("rfid")).toString());
        if (record.contains(QStringLiteral("lockerType"))) {
            item.insert(QStringLiteral("lockerType"),
                        record.value(QStringLiteral("lockerType")).toInt());
        }
        items.append(item);
    }
    return items;
}

void CaptureTicketWorkflowService::parkCurrent(CaptureTicketPhase phase)
{
    if (!m_operation.hasClaim()) return;
    m_operation.phase = phase;
    m_trackedOperations.insert(m_operation.taskId, m_operation);
    m_operation.clear();
}

CaptureTicketWorkflowEffect CaptureTicketWorkflowService::resumeTrackedTask(
        const QList<KeyTaskDto> &tasks,
        int currentKeySessionId)
{
    // Q_TASK order is used only to choose the next serial request; every
    // ticket keeps independent completion and upload correlation.
    for (const KeyTaskDto &task : tasks) {
        for (auto it = m_trackedOperations.begin();
             it != m_trackedOperations.end(); ++it) {
            if (it.value().taskIdRaw != task.taskId) continue;
            m_operation = it.value();
            m_trackedOperations.erase(it);
            if (!CaptureTicketUseCase::isCaptureTaskType(task.type)) {
                CaptureTicketWorkflowEffect result = effect(
                            CaptureTicketWorkflowAction::ReportFailure,
                            QStringLiteral("Q_TASK \u4efb\u52a1\u7c7b\u578b\u4e0e\u5df2\u8ddf\u8e2a\u91c7\u7801\u7968\u4e0d\u4e00\u81f4\uff0c\u5df2\u505c\u6b62\u8be5\u4efb\u52a1"));
                m_operation.clear();
                return result;
            }
            if (m_operation.lastReadSessionId == currentKeySessionId) {
                parkCurrent(CaptureTicketPhase::WaitingForCapture);
                break;
            }
            m_operation.keySessionId = currentKeySessionId;
            m_operation.deliveryConfirmed = true;
            m_operation.lastReadSessionId = currentKeySessionId;
            m_operation.phase = CaptureTicketPhase::ReadingRfid;
            return effect(CaptureTicketWorkflowAction::ReadRfid,
                          QStringLiteral("\u6b63\u5728\u6309\u4efb\u52a1\u53f7\u8bfb\u53d6\u91c7\u7801 RFID\uff1a%1")
                              .arg(m_operation.taskId));
        }
    }

    for (auto it = m_trackedOperations.begin();
         it != m_trackedOperations.end(); ++it) {
        const KeyTaskDto *task = CaptureTicketUseCase::findTask(
                    tasks, it.value().taskIdRaw);
        if (task || it.value().deliveryConfirmed) continue;
        m_operation = it.value();
        m_trackedOperations.erase(it);
        m_operation.keySessionId = currentKeySessionId;
        m_operation.phase = CaptureTicketPhase::Delivering;
        return effect(CaptureTicketWorkflowAction::SendTicket,
                      QStringLiteral("\u6b63\u5728\u7ee7\u7eed\u4e0b\u53d1\u5df2\u9886\u53d6\u7684\u91c7\u7801\u7968\uff1a%1")
                          .arg(m_operation.taskId));
    }

    for (auto it = m_trackedOperations.begin();
         it != m_trackedOperations.end(); ++it) {
        if (!it.value().deliveryConfirmed
                || CaptureTicketUseCase::findTask(tasks, it.value().taskIdRaw)) {
            continue;
        }
        const QString missingTaskId = it.key();
        m_trackedOperations.erase(it);
        CaptureTicketWorkflowEffect result;
        result.action = CaptureTicketWorkflowAction::ReportFailure;
        result.taskId = missingTaskId;
        result.message = QStringLiteral("\u5df2\u8ddf\u8e2a\u7684\u91c7\u7801\u7968\u4e0d\u518d\u5b58\u5728\u4e8e\u94a5\u5319\u4e2d\uff0c\u5df2\u505c\u6b62\u672c\u5730\u81ea\u52a8\u5904\u7406\uff1a%1")
                .arg(missingTaskId);
        result.handled = true;
        return result;
    }
    return CaptureTicketWorkflowEffect();
}

bool CaptureTicketWorkflowService::hasUnreadTrackedTask(
        int currentKeySessionId) const
{
    for (const CaptureTicketOperationContext &operation : m_trackedOperations) {
        if (!operation.deliveryConfirmed
                || operation.lastReadSessionId != currentKeySessionId) {
            return true;
        }
    }
    return false;
}
