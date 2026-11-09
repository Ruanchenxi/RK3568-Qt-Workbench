#include "features/key/application/CaptureTicketUseCase.h"

CaptureTicketAdmissionDecision CaptureTicketUseCase::evaluateAdmission(
        const CaptureTicketAdmissionRequest &request)
{
    CaptureTicketAdmissionDecision decision;
    if (request.captureBusy) {
        decision.reason = CaptureTicketAdmissionReason::CaptureBusy;
        decision.message = QStringLiteral("\u91c7\u7801\u7968\u6d41\u7a0b\u6b63\u5728\u8fdb\u884c\u4e2d");
        return decision;
    }
    if (request.stationAlreadyPending) {
        decision.reason = CaptureTicketAdmissionReason::DuplicateStation;
        decision.message = QStringLiteral("\u5f53\u524d\u7ad9\u53f7\u5df2\u6709\u672a\u5b8c\u6210\u7684\u91c7\u7801\u7968\uff0c\u4e0d\u80fd\u91cd\u590d\u9886\u53d6");
        return decision;
    }
    if (request.stationNo < 1 || request.stationNo > 255) {
        decision.reason = CaptureTicketAdmissionReason::InvalidStation;
        decision.message = QStringLiteral("\u91c7\u7801\u7968\u7ad9\u53f7\u5fc5\u987b\u5728 1 \u5230 255 \u4e4b\u95f4");
        return decision;
    }
    if (!request.session.connected || !request.session.keyPresent
            || !request.session.keyStable || !request.session.sessionReady
            || !request.session.protocolHealthy) {
        decision.reason = CaptureTicketAdmissionReason::KeyNotReady;
        decision.message = QStringLiteral("\u94a5\u5319\u5c1a\u672a\u5c31\u7eea\uff0c\u65e0\u6cd5\u9886\u53d6\u91c7\u7801\u7968");
        return decision;
    }
    if (request.session.commandInFlight || request.session.recoveryWindowActive) {
        decision.reason = CaptureTicketAdmissionReason::CommandBusy;
        decision.message = QStringLiteral("\u94a5\u5319\u901a\u8baf\u6b63\u5fd9\uff0c\u8bf7\u7a0d\u540e\u91cd\u8bd5");
        return decision;
    }
    if (request.peerWorkflowBusy) {
        decision.reason = CaptureTicketAdmissionReason::PeerWorkflowBusy;
        decision.message = QStringLiteral("\u5f53\u524d\u6709\u4f20\u7968\u3001\u56de\u4f20\u6216\u5220\u9664\u64cd\u4f5c\u5728\u9014\uff0c\u8bf7\u7a0d\u540e\u91cd\u8bd5");
        return decision;
    }
    decision.reason = CaptureTicketAdmissionReason::Ready;
    decision.allowed = true;
    return decision;
}

bool CaptureTicketUseCase::isCaptureTaskType(quint8 type)
{
    return type == 0x02 || type == 0x05;
}

const KeyTaskDto *CaptureTicketUseCase::findTask(
        const QList<KeyTaskDto> &tasks,
        const QByteArray &taskIdRaw)
{
    for (const KeyTaskDto &task : tasks) {
        if (task.taskId == taskIdRaw) return &task;
    }
    return nullptr;
}

CaptureTicketRecordDecision CaptureTicketUseCase::evaluateRecords(
        const QSet<int> &expectedInnerCodes,
        const QVariantList &records)
{
    CaptureTicketRecordDecision decision;
    decision.expectedInnerCodes = expectedInnerCodes;
    if (expectedInnerCodes.isEmpty()) {
        decision.message = QStringLiteral("\u91c7\u7801\u7968\u539f\u59cb\u6b65\u9aa4\u65e0\u6548\uff0c\u5df2\u7981\u6b62\u4e0a\u4f20\u548c\u5220\u7968");
        return decision;
    }
    for (const QVariant &value : records) {
        const QVariantMap record = value.toMap();
        bool ok = false;
        const int innerCode = record.value(QStringLiteral("innerCode")).toInt(&ok);
        const QString rfid = record.value(QStringLiteral("rfid")).toString().trimmed();
        if (ok && expectedInnerCodes.contains(innerCode) && !rfid.isEmpty()) {
            decision.capturedInnerCodes.insert(innerCode);
        }
    }
    const QSet<int> missing = expectedInnerCodes - decision.capturedInnerCodes;
    if (!missing.isEmpty()) {
        QStringList values;
        for (int value : missing) values.append(QString::number(value));
        decision.message = QStringLiteral("\u91c7\u7801\u5c1a\u672a\u5b8c\u6210\uff0c\u7f3a\u5c11\u8bbe\u5907 innerCode: %1")
                .arg(values.join(QStringLiteral(", ")));
        return decision;
    }
    decision.complete = true;
    return decision;
}
