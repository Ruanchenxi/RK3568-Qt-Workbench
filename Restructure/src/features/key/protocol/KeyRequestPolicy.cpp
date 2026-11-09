#include "KeyRequestPolicy.h"

#include "KeyProtocolDefs.h"

namespace KeyRequestPolicy {

bool ackMatches(KeyRequestType requestType,
                quint8 expectedAckCommand,
                quint8 acknowledgedCommand)
{
    switch (requestType) {
    case KeyRequestType::SetCom:
        return acknowledgedCommand == KeyProtocol::CmdSetCom;
    case KeyRequestType::SetTime:
        return acknowledgedCommand == KeyProtocol::CmdSetTime;
    case KeyRequestType::DeleteTask:
        return acknowledgedCommand == KeyProtocol::CmdDel;
    // 分帧传输按本次发送时记录的期望命令匹配；不校验帧序号，不能区分同命令迟到 ACK。
    case KeyRequestType::Ticket:
    case KeyRequestType::InitTransfer:
    case KeyRequestType::RfidTransfer:
        return acknowledgedCommand == expectedAckCommand;
    default:
        return false;
    }
}

RetryAction classifyRetry(const RetryObservation &observation)
{
    if (!observation.requestActive || !observation.timerGenerationMatches) {
        return RetryAction::IgnoreStale;
    }
    if (observation.retryCountAfterTimeout < observation.maxRetries) {
        return RetryAction::Resend;
    }

    const bool auxiliaryCommand =
            observation.requestType == KeyRequestType::QueryBattery
            || observation.requestType == KeyRequestType::SetTime
            || observation.requestType == KeyRequestType::SetCom;
    if (auxiliaryCommand && observation.auxiliarySeatB) {
        return RetryAction::FailAuxiliarySeatB;
    }
    if (observation.requestType == KeyRequestType::QTask
            && observation.startupQTaskProbe) {
        return RetryAction::IgnoreStartupProbe;
    }
    if (observation.requestType == KeyRequestType::QTask
            && observation.keyPresent
            && observation.qtaskRecoveryRound
               < observation.maxQTaskRecoveryRounds) {
        return RetryAction::ScheduleQTaskRecovery;
    }
    return RetryAction::FinalTimeout;
}

QString commandName(const CommandDescription &description)
{
    switch (description.requestType) {
    case KeyRequestType::SetCom:
        return QStringLiteral("SET_COM");
    case KeyRequestType::QTask:
        return QStringLiteral("Q_TASK");
    case KeyRequestType::DeleteTask:
        return QStringLiteral("DEL");
    case KeyRequestType::Ticket:
        return QStringLiteral("TICKET frame %1/%2")
                .arg(description.frameIndex + 1)
                .arg(description.frameCount);
    case KeyRequestType::TaskLog:
        return QStringLiteral("I_TASK_LOG");
    case KeyRequestType::CaptureRfid:
        return QStringLiteral("I_RFID");
    case KeyRequestType::InitTransfer:
        return QStringLiteral("INIT frame %1/%2")
                .arg(description.frameIndex + 1)
                .arg(description.frameCount);
    case KeyRequestType::RfidTransfer:
        return QStringLiteral("DN_RFID frame %1/%2")
                .arg(description.frameIndex + 1)
                .arg(description.frameCount);
    case KeyRequestType::QueryBattery:
        return QStringLiteral("Q_KEYEQ");
    case KeyRequestType::SetTime:
        return QStringLiteral("SET_TIME");
    default:
        return QStringLiteral("0x%1")
                .arg(description.expectedAckCommand, 2, 16, QChar('0'))
                .toUpper();
    }
}

QString retrySummary(const CommandDescription &description,
                     int retryCount,
                     int maxRetries)
{
    return QStringLiteral("Retry %1/%2 for %3")
            .arg(retryCount)
            .arg(maxRetries)
            .arg(commandName(description));
}

QString ackSummary(quint8 acknowledgedCommand)
{
    switch (acknowledgedCommand) {
    case KeyProtocol::CmdSetCom:
        return QStringLiteral("ACK for SET_COM");
    case KeyProtocol::CmdSetTime:
        return QStringLiteral("ACK for SET_TIME");
    case KeyProtocol::CmdDel:
        return QStringLiteral("ACK for DEL");
    case KeyProtocol::CmdInit:
        return QStringLiteral("ACK for INIT");
    case static_cast<quint8>(KeyProtocol::CmdInit | 0x80):
        return QStringLiteral("ACK for INIT_MORE");
    case KeyProtocol::CmdDownloadRfid:
        return QStringLiteral("ACK for DN_RFID");
    case static_cast<quint8>(KeyProtocol::CmdDownloadRfid | 0x80):
        return QStringLiteral("ACK for DN_RFID_MORE");
    default:
        return QStringLiteral("ACK for 0x%1")
                .arg(acknowledgedCommand, 2, 16, QChar('0'));
    }
}

} // namespace KeyRequestPolicy
