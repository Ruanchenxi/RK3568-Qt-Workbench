#include "features/key/application/KeyTaskPresentationService.h"

namespace {

const SystemTicketDto *findSystemTicket(
        const QList<SystemTicketDto> &tickets,
        const QString &taskId)
{
    for (const SystemTicketDto &ticket : tickets) {
        if (ticket.taskId == taskId) return &ticket;
    }
    return nullptr;
}

QString hexByte(quint8 value)
{
    return QStringLiteral("0x%1")
            .arg(value, 2, 16, QLatin1Char('0')).toUpper();
}

} // namespace

QList<KeyTaskDisplayDto> KeyTaskPresentationService::present(
        const QList<KeyTaskDto> &tasks,
        const QList<SystemTicketDto> &systemTickets,
        const QHash<QString, CaptureTicketPhase> &capturePhases)
{
    QList<KeyTaskDisplayDto> rows;
    rows.reserve(tasks.size());

    for (const KeyTaskDto &task : tasks) {
        KeyTaskDisplayDto row;
        const QString taskId = taskIdFromRaw(task.taskId);
        const SystemTicketDto *systemTicket = findSystemTicket(systemTickets, taskId);
        const bool knownCapture = capturePhases.contains(taskId);

        if (knownCapture) {
            row.taskIdText = taskId;
            row.typeText = QStringLiteral("采码票");
            row.sourceText = QStringLiteral("本机采码领取");
            row.statusText = captureStatusText(capturePhases.value(taskId));
        } else if (systemTicket) {
            row.taskIdText = systemTicket->ticketNo.isEmpty()
                    ? taskId : systemTicket->ticketNo;
            row.typeText = ordinaryTypeText(systemTicket->taskType);
            row.sourceText = QStringLiteral("工作台下发");
            row.statusText = ordinaryStatusText(task.status);
        } else {
            const QString taskIdHex = QString(task.taskId.toHex().toUpper());
            row.taskIdText = taskIdHex.left(16) + QStringLiteral(" [钥匙已有]");
            row.typeText = QStringLiteral("未知票据");
            row.sourceText = QStringLiteral("钥匙已有");
            row.statusText = ordinaryStatusText(task.status);
            row.unregistered = true;
        }

        row.diagnosticText = QStringLiteral(
                    "Q_TASK 原始字段：status=%1, type=%2, source=%3")
                .arg(hexByte(task.status), hexByte(task.type), hexByte(task.source));
        rows.append(row);
    }
    return rows;
}

QString KeyTaskPresentationService::taskIdFromRaw(const QByteArray &taskIdRaw)
{
    if (taskIdRaw.size() < 8) return QString();
    qulonglong value = 0;
    for (int i = 0; i < 8; ++i) {
        value |= static_cast<qulonglong>(
                    static_cast<quint8>(taskIdRaw.at(i))) << (i * 8);
    }
    return QString::number(value);
}

QString KeyTaskPresentationService::ordinaryTypeText(int taskType)
{
    switch (taskType) {
    case 0: return QStringLiteral("停电票");
    case 1: return QStringLiteral("送电票");
    default: return QStringLiteral("普通票");
    }
}

QString KeyTaskPresentationService::ordinaryStatusText(quint8 status)
{
    switch (status) {
    case 0x00: return QStringLiteral("未操作");
    case 0x01: return QStringLiteral("操作中");
    case 0x02: return QStringLiteral("操作完成");
    case 0x04: return QStringLiteral("已删除（钥匙）");
    case 0x05: return QStringLiteral("已删除（图形）");
    default:
        return QStringLiteral("未知状态（%1）").arg(hexByte(status));
    }
}

QString KeyTaskPresentationService::captureStatusText(CaptureTicketPhase phase)
{
    switch (phase) {
    case CaptureTicketPhase::CheckingKeyCompatibility:
    case CaptureTicketPhase::Claiming:
        return QStringLiteral("正在领取");
    case CaptureTicketPhase::Delivering:
    case CaptureTicketPhase::AwaitingDeliveryProof:
        return QStringLiteral("正在下发");
    case CaptureTicketPhase::WaitingForCapture:
        return QStringLiteral("等待采码");
    case CaptureTicketPhase::ReadingRfid:
        return QStringLiteral("正在读取采码");
    case CaptureTicketPhase::Uploading:
        return QStringLiteral("正在回传");
    case CaptureTicketPhase::AwaitingDelete:
        return QStringLiteral("等待清票");
    case CaptureTicketPhase::Idle:
        return QStringLiteral("采码处理中");
    }
    return QStringLiteral("采码处理中");
}
