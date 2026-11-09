#include "features/key/ui/TicketStatusPresenter.h"

#include "features/key/domain/TicketReturnState.h"
#include "features/key/domain/TicketTransferState.h"

namespace
{
// Projects only return workflow meaning; transfer and administrator states
// remain separate so combined labels cannot leak back into business rules.
QString returnTableText(TicketReturnState state)
{
    switch (state) {
    case TicketReturnState::Handshake:
        return QStringLiteral("\u56DE\u4F20\u524D\u63E1\u624B\u4E2D");
    case TicketReturnState::LogRequesting:
        return QStringLiteral("\u6B63\u5728\u5411\u94A5\u5319\u8BF7\u6C42\u56DE\u4F20\u65E5\u5FD7");
    case TicketReturnState::LogReceiving:
        return QStringLiteral("\u6B63\u5728\u63A5\u6536\u94A5\u5319\u56DE\u4F20\u65E5\u5FD7");
    case TicketReturnState::Uploading:
        return QStringLiteral("\u65E5\u5FD7\u5DF2\u8BFB\u53D6\uFF0C\u6B63\u5728\u56DE\u4F20\u5230\u670D\u52A1\u7AEF");
    case TicketReturnState::Cleanup:
        return QStringLiteral("\u670D\u52A1\u7AEF\u5DF2\u786E\u8BA4\uFF0C\u6B63\u5728\u6E05\u7406\u94A5\u5319\u4EFB\u52A1");
    case TicketReturnState::DeleteFailed:
        return QStringLiteral("\u94A5\u5319\u5220\u9664\u9A8C\u8BC1\u5931\u8D25\uFF0C\u9700\u4EBA\u5DE5\u5904\u7406");
    case TicketReturnState::ManualRequired:
        return QStringLiteral("\u56DE\u4F20\u9700\u4EBA\u5DE5\u786E\u8BA4\uFF0C\u8BF7\u67E5\u770B\u670D\u52A1\u65E5\u5FD7");
    case TicketReturnState::Idle:
    case TicketReturnState::Unknown:
        return QString();
    }
    return QString();
}
}

QString TicketStatusPresenter::tableText(const QString &state)
{
    const QString returnText =
            returnTableText(ticketReturnStateFromStorage(state));
    if (!returnText.isEmpty())
        return returnText;
    switch (ticketTransferStateFromStorage(state)) {
    case TicketTransferState::Received:
        return QStringLiteral("\u4E3B\u7A0B\u5E8F\u5DF2\u63A5\u6536\uFF0C\u5C1A\u672A\u5F00\u59CB\u4F20\u7968");
    case TicketTransferState::AutoPending:
        return QStringLiteral("\u7B49\u5F85\u94A5\u5319\u5C31\u7EEA\u540E\u81EA\u52A8\u4F20\u7968");
    case TicketTransferState::Sending:
        return QStringLiteral("\u6B63\u5728\u4F20\u7968\u5230\u94A5\u5319");
    case TicketTransferState::Success:
        return QStringLiteral("\u5DF2\u4F20\u7968\u5230\u94A5\u5319\uFF0C\u7B49\u5F85\u4EFB\u52A1\u6267\u884C");
    case TicketTransferState::KeyCleared:
        return QStringLiteral("\u94A5\u5319\u4EFB\u52A1\u5DF2\u5220\u9664\uFF0C\u53EF\u518D\u6B21\u4F20\u7968");
    case TicketTransferState::Failed:
        return QStringLiteral("\u4F20\u7968\u5931\u8D25\uFF0C\u8BF7\u67E5\u770B\u4E32\u53E3\u62A5\u6587");
    case TicketTransferState::Unknown:
        break;
    }
    if (state == QLatin1String("admin-delete-pending"))
        return QStringLiteral("\u94A5\u5319\u5DF2\u6E05\u7406\uFF0C\u6B63\u5728\u901A\u77E5\u5DE5\u4F5C\u53F0\u7EC8\u6B62");
    if (state == QLatin1String("admin-delete-failed"))
        return QStringLiteral("\u5DE5\u4F5C\u53F0\u901A\u77E5\u5931\u8D25\uFF0C\u9700\u624B\u52A8\u5904\u7406");
    if (state == QLatin1String("failed"))
        return QStringLiteral("\u4F20\u7968\u5931\u8D25\uFF0C\u8BF7\u67E5\u770B\u4E32\u53E3\u62A5\u6587");
    return state;
}

QString TicketStatusPresenter::detailText(const SystemTicketDto &ticket)
{
    if (ticket.adminDeleteStage == SystemTicketDto::AdminDeleteStage::TerminationFailed) {
        const QString error = ticket.adminDeleteError.trimmed();
        return error.isEmpty()
                ? QStringLiteral("\u5DE5\u4F5C\u53F0\u7EC8\u6B62\u5931\u8D25\uFF0C\u8BF7\u624B\u52A8\u5173\u95ED\u8BE5\u4EFB\u52A1")
                : QStringLiteral("\u5DE5\u4F5C\u53F0\u7EC8\u6B62\u5931\u8D25\uFF1A%1").arg(error);
    }
    if (ticket.adminDeleteStage == SystemTicketDto::AdminDeleteStage::KeyClearedAwaitingTermination) {
        return QStringLiteral("\u94A5\u5319\u5DF2\u6E05\u7406\uFF0C\u6B63\u5728\u901A\u77E5\u5DE5\u4F5C\u53F0\u7EC8\u6B62");
    }

    const TicketReturnState returnState =
            ticketReturnStateFromStorage(ticket.returnState);
    const TicketReturnOutcome returnOutcome =
            ticketReturnOutcomeFromStorage(ticket.returnOutcome);
    switch (returnState) {
    case TicketReturnState::Handshake:
        return QStringLiteral("\u6B63\u5728\u4E0E\u94A5\u5319\u5EFA\u7ACB\u56DE\u4F20\u63E1\u624B");
    case TicketReturnState::LogRequesting:
        return QStringLiteral("\u63E1\u624B\u5B8C\u6210\uFF0C\u6B63\u5728\u8BF7\u6C42\u94A5\u5319\u65E5\u5FD7");
    case TicketReturnState::LogReceiving:
        return QStringLiteral("\u6B63\u5728\u63A5\u6536\u94A5\u5319\u65E5\u5FD7\uFF0C\u8BF7\u7A0D\u5019");
    case TicketReturnState::Uploading:
        return QStringLiteral("\u65E5\u5FD7\u5DF2\u8BFB\u53D6\uFF0C\u6B63\u5728\u4E0A\u4F20\u670D\u52A1\u7AEF");
    case TicketReturnState::Cleanup:
        return QStringLiteral("\u56DE\u4F20\u5DF2\u4E0A\u4F20\uFF0C\u6B63\u5728\u786E\u8BA4\u94A5\u5319\u4EFB\u52A1\u5DF2\u6E05\u7406");
    case TicketReturnState::DeleteFailed:
        return ticket.returnError.trimmed().isEmpty()
                ? QStringLiteral("\u94A5\u5319\u6E05\u7406\u5931\u8D25\uFF0C\u8BF7\u786E\u8BA4\u540E\u91CD\u8BD5")
                : QStringLiteral("\u94A5\u5319\u6E05\u7406\u5931\u8D25\uFF1A%1").arg(ticket.returnError);
    case TicketReturnState::ManualRequired:
        return ticket.returnError.trimmed().isEmpty()
                ? QStringLiteral("\u9700\u4EBA\u5DE5\u786E\u8BA4\uFF0C\u8BF7\u67E5\u770B\u670D\u52A1\u65E5\u5FD7")
                : QStringLiteral("\u9700\u4EBA\u5DE5\u786E\u8BA4\uFF1A%1").arg(ticket.returnError);
    case TicketReturnState::Idle:
        if (returnOutcome == TicketReturnOutcome::RetryableFailure) {
            return QStringLiteral("\u4E0A\u6B21\u56DE\u4F20\u672A\u5B8C\u6210\uFF0C"
                                  "\u53EF\u91CD\u8BD5\uFF1A%1")
                    .arg(ticket.returnError);
        }
        if (returnOutcome == TicketReturnOutcome::WaitingForRemainingSteps)
            return ticket.returnError;
        break;
    case TicketReturnState::Unknown:
        break;
    }

    switch (ticketTransferStateFromStorage(ticket.transferState)) {
    case TicketTransferState::Received:
        return QStringLiteral("\u5DF2\u63A5\u6536\u5DE5\u4F5C\u53F0\u6307\u4EE4\uFF0C\u7B49\u5F85\u4F20\u7968");
    case TicketTransferState::AutoPending:
        return QStringLiteral("\u7B49\u5F85\u94A5\u5319\u5C31\u7EEA\u540E\u81EA\u52A8\u4F20\u7968");
    case TicketTransferState::Sending:
        return QStringLiteral("\u6B63\u5728\u5411\u94A5\u5319\u4F20\u7968\uFF0C\u8BF7\u7A0D\u5019");
    case TicketTransferState::Success:
        return QStringLiteral("\u4F20\u7968\u5B8C\u6210\uFF0C\u7B49\u5F85\u4EFB\u52A1\u6267\u884C");
    case TicketTransferState::KeyCleared:
        return QStringLiteral("\u94A5\u5319\u4EFB\u52A1\u5DF2\u5220\u9664\uFF0C\u53EF\u91CD\u65B0\u4F20\u7968");
    case TicketTransferState::Failed:
        return ticket.lastError.trimmed().isEmpty()
                ? QStringLiteral("\u4F20\u7968\u5931\u8D25\uFF0C\u8BF7\u67E5\u770B\u65E5\u5FD7")
                : QStringLiteral("\u4F20\u7968\u5931\u8D25\uFF1A%1").arg(ticket.lastError);
    case TicketTransferState::Unknown:
        break;
    }
    return ticket.transferState;
}

QString TicketStatusPresenter::tableText(const SystemTicketDto &ticket)
{
    if (ticket.adminDeleteStage == SystemTicketDto::AdminDeleteStage::TerminationFailed)
        return tableText(QStringLiteral("admin-delete-failed"));
    if (ticket.adminDeleteStage == SystemTicketDto::AdminDeleteStage::KeyClearedAwaitingTermination)
        return tableText(QStringLiteral("admin-delete-pending"));
    const TicketReturnState returnState =
            ticketReturnStateFromStorage(ticket.returnState);
    const TicketReturnOutcome returnOutcome =
            ticketReturnOutcomeFromStorage(ticket.returnOutcome);
    if (returnState == TicketReturnState::Idle
            && returnOutcome == TicketReturnOutcome::RetryableFailure) {
        return QStringLiteral("\u4E0A\u6B21\u56DE\u4F20\u672A\u5B8C\u6210\uFF0C\u53EF\u91CD\u8BD5");
    }
    if (returnState == TicketReturnState::Idle
            && returnOutcome == TicketReturnOutcome::WaitingForRemainingSteps)
        return ticket.returnError;
    return returnState == TicketReturnState::Idle
            ? tableText(ticket.transferState)
            : tableText(ticket.returnState);
}

QString TicketStatusPresenter::statusColor(const SystemTicketDto &ticket)
{
    const TicketReturnState returnState =
            ticketReturnStateFromStorage(ticket.returnState);
    const TicketReturnOutcome returnOutcome =
            ticketReturnOutcomeFromStorage(ticket.returnOutcome);
    if (returnState == TicketReturnState::Idle
            && (returnOutcome == TicketReturnOutcome::RetryableFailure
                || returnOutcome == TicketReturnOutcome::WaitingForRemainingSteps)) {
        return QStringLiteral("#EF6C00");
    }
    switch (returnState) {
    case TicketReturnState::Handshake:
    case TicketReturnState::LogRequesting:
    case TicketReturnState::LogReceiving:
        return QStringLiteral("#1565C0");
    case TicketReturnState::Uploading:
    case TicketReturnState::Cleanup:
        return QStringLiteral("#EF6C00");
    case TicketReturnState::Idle:
    case TicketReturnState::DeleteFailed:
    case TicketReturnState::ManualRequired:
    case TicketReturnState::Unknown:
        break;
    }
    const TicketTransferState transferState =
            ticketTransferStateFromStorage(ticket.transferState);
    if (transferState == TicketTransferState::Success)
        return QStringLiteral("#2E7D32");
    if (transferState == TicketTransferState::AutoPending
            || transferState == TicketTransferState::KeyCleared)
        return QStringLiteral("#1565C0");
    if (transferState == TicketTransferState::Sending)
        return QStringLiteral("#EF6C00");
    return QStringLiteral("#C62828");
}

QString TicketStatusPresenter::returnHint(const SystemTicketDto &ticket,
                                          quint8 keyTaskStatus,
                                          bool canManualReturn,
                                          const QString &blockedReason)
{
    if (ticketReturnStateFromStorage(ticket.returnState)
            == TicketReturnState::DeleteFailed) {
        return ticket.returnError.trimmed().isEmpty()
                ? QStringLiteral("\u94A5\u5319\u5220\u9664\u9A8C\u8BC1\u5931\u8D25\uFF0C\u8BF7\u786E\u8BA4\u94A5\u5319\u72B6\u6001\u540E\u518D\u5904\u7406")
                : QStringLiteral("\u94A5\u5319\u5220\u9664\u9A8C\u8BC1\u5931\u8D25\uFF1A%1").arg(ticket.returnError);
    }
    if (ticketTransferStateFromStorage(ticket.transferState)
            == TicketTransferState::Success && keyTaskStatus != 0x02)
        return QStringLiteral("\u5F53\u524D\u4EFB\u52A1\u5728\u94A5\u5319\u4E2D\u5C1A\u672A\u5B8C\u6210\uFF0C\u5B8C\u6210\u540E\u624D\u5141\u8BB8\u56DE\u4F20");
    if (canManualReturn)
        return QStringLiteral("\u5F53\u524D\u4EFB\u52A1\u5DF2\u5B8C\u6210\uFF0C\u53EF\u6267\u884C\u624B\u52A8\u56DE\u4F20\uFF1B\u5176\u4ED6\u672A\u5B8C\u6210\u4EFB\u52A1\u4E0D\u5F71\u54CD\u672C\u4EFB\u52A1\u56DE\u4F20");
    if (ticketTransferStateFromStorage(ticket.transferState)
            == TicketTransferState::Success
            && !blockedReason.trimmed().isEmpty())
        return blockedReason;
    return QStringLiteral("\u624B\u52A8\u56DE\u4F20\u4EC5\u8981\u6C42\u5F53\u524D\u9009\u4E2D\u4EFB\u52A1\u5B8C\u6210\uFF1B\u63A5\u53E3\u672A\u914D\u7F6E\u65F6\u4E0D\u4F1A\u4E0A\u4F20");
}