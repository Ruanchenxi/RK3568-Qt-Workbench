#include "features/key/application/KeyManualActionUseCase.h"

namespace {

QString disconnectedMessage(KeyManualAction action)
{
    switch (action) {
    case KeyManualAction::Initialize:
        return QStringLiteral("\u4E32\u53E3\u672A\u8FDE\u63A5\uFF0C\u65E0\u6CD5\u6267\u884C\u521D\u59CB\u5316");
    case KeyManualAction::DownloadRfid:
        return QStringLiteral("\u4E32\u53E3\u672A\u8FDE\u63A5\uFF0C\u65E0\u6CD5\u4E0B\u8F7D RFID");
    case KeyManualAction::QueryBattery:
        return QStringLiteral("\u4E32\u53E3\u672A\u8FDE\u63A5\uFF0C\u65E0\u6CD5\u67E5\u8BE2\u94A5\u5319\u7535\u91CF");
    case KeyManualAction::SyncTime:
        return QStringLiteral("\u4E32\u53E3\u672A\u8FDE\u63A5\uFF0C\u65E0\u6CD5\u6267\u884C\u94A5\u5319\u6821\u65F6");
    case KeyManualAction::QueryTasks:
        return QStringLiteral("\u4E32\u53E3\u672A\u8FDE\u63A5\uFF0C\u65E0\u6CD5\u67E5\u8BE2\u4EFB\u52A1");
    }
    return QString();
}

QString notReadyMessage(KeyManualAction action)
{
    switch (action) {
    case KeyManualAction::Initialize:
        return QStringLiteral("\u94A5\u5319\u672A\u5C31\u7EEA\uFF0C\u65E0\u6CD5\u6267\u884C\u521D\u59CB\u5316");
    case KeyManualAction::DownloadRfid:
        return QStringLiteral("\u94A5\u5319\u672A\u5C31\u7EEA\uFF0C\u65E0\u6CD5\u4E0B\u8F7D RFID");
    case KeyManualAction::QueryBattery:
        return QStringLiteral("A\u5EA7\u94A5\u5319\u672A\u5C31\u7EEA\uFF0C\u65E0\u6CD5\u67E5\u8BE2\u7535\u91CF");
    case KeyManualAction::SyncTime:
        return QStringLiteral("A\u5EA7\u94A5\u5319\u672A\u5C31\u7EEA\uFF0C\u65E0\u6CD5\u6267\u884C\u6821\u65F6");
    case KeyManualAction::QueryTasks:
        return QString();
    }
    return QString();
}

QString provisioningBusyMessage(KeyManualAction action)
{
    return action == KeyManualAction::Initialize
            ? QStringLiteral("\u6B63\u5728\u83B7\u53D6\u94A5\u5319\u521D\u59CB\u5316\u6570\u636E\uFF0C\u8BF7\u7A0D\u540E\u518D\u8BD5")
            : QStringLiteral("\u6B63\u5728\u83B7\u53D6 RFID \u6570\u636E\uFF0C\u8BF7\u7A0D\u540E\u518D\u8BD5");
}

}

KeyManualActionDecision KeyManualActionUseCase::evaluate(
        const KeyManualActionRequest &request)
{
    KeyManualActionDecision decision;
    if (request.action == KeyManualAction::QueryTasks
            && request.workbenchConfirmationActive) {
        decision.reason = KeyManualActionReason::WorkbenchConfirmationActive;
        decision.message = QStringLiteral(
                    "\u5F53\u524D\u5DE5\u4F5C\u53F0\u4F20\u7968\u6B63\u5728\u7B49\u5F85\u94A5\u5319\u4EFB\u52A1\u786E\u8BA4\uFF0C"
                    "\u8BF7\u7A0D\u540E\u518D\u67E5\u8BE2");
        return decision;
    }
    if (!request.serialConnected) {
        decision.reason = KeyManualActionReason::SerialDisconnected;
        decision.message = disconnectedMessage(request.action);
        return decision;
    }

    const bool requiresReadyKey = request.action != KeyManualAction::QueryTasks;
    if (requiresReadyKey && !request.keyReady) {
        decision.reason = KeyManualActionReason::KeyNotReady;
        decision.message = notReadyMessage(request.action);
        return decision;
    }

    const bool provisioningAction =
            request.action == KeyManualAction::Initialize
            || request.action == KeyManualAction::DownloadRfid;
    if (provisioningAction && request.provisioningBusy) {
        decision.reason = KeyManualActionReason::ProvisioningBusy;
        decision.message = provisioningBusyMessage(request.action);
        return decision;
    }

    decision.reason = KeyManualActionReason::Ready;
    decision.allowed = true;
    return decision;
}
