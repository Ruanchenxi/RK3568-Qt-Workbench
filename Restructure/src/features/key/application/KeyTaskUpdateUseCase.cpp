#include "features/key/application/KeyTaskUpdateUseCase.h"

KeyTaskUpdatePlan KeyTaskUpdateUseCase::plan(
        const KeyTaskUpdateContext &context)
{
    KeyTaskUpdatePlan result;
    const bool automaticActions =
            context.sessionReady && !context.suppressSessionActions;

    result.reconcileSystemTickets =
            automaticActions && !context.transferConfirmationHandled;
    result.recoverAutomaticTransfer = automaticActions;
    result.tryReturnedTicketCleanup = automaticActions;
    // A capture-controlled Q_TASK may simultaneously be the post-DEL proof
    // for capture cleanup. Permit only that proof while unrelated automatic
    // ticket actions remain suppressed.
    result.finalizeDelete = context.sessionReady
            && context.deleteActive
            && (automaticActions || context.allowDeleteVerification);
    result.finalizeCancelDelete =
            automaticActions && context.cancelDeleteActive;
    result.processPendingCancel = automaticActions;
    result.tryProgressiveReturn = automaticActions;
    result.tryAutomaticReturn = automaticActions;
    result.refreshSelectedTicket = context.systemTicketSelected;
    result.tryAutomaticTransfer =
            automaticActions
            && (!context.taskListEmpty || !context.transferActive);
    result.runMaintenance = automaticActions;
    return result;
}
