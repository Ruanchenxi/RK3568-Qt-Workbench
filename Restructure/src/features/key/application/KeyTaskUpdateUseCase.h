#ifndef KEYTASKUPDATEUSECASE_H
#define KEYTASKUPDATEUSECASE_H

struct KeyTaskUpdateContext
{
    bool sessionReady = false;
    bool suppressSessionActions = false;
    bool allowDeleteVerification = false;
    bool transferConfirmationHandled = false;
    bool deleteActive = false;
    bool cancelDeleteActive = false;
    bool systemTicketSelected = false;
    bool taskListEmpty = true;
    bool transferActive = false;
};

struct KeyTaskUpdatePlan
{
    bool reconcileSystemTickets = false;
    bool recoverAutomaticTransfer = false;
    bool tryReturnedTicketCleanup = false;
    bool finalizeDelete = false;
    bool finalizeCancelDelete = false;
    bool processPendingCancel = false;
    bool tryProgressiveReturn = false;
    bool tryAutomaticReturn = false;
    bool refreshSelectedTicket = false;
    bool tryAutomaticTransfer = false;
    bool runMaintenance = false;
};

// Pure Q_TASK fan-out policy. It selects independent L2 entry points without
// calling a workflow, serial adapter, timer, store, or UI object.
class KeyTaskUpdateUseCase
{
public:
    static KeyTaskUpdatePlan plan(const KeyTaskUpdateContext &context);
};

#endif // KEYTASKUPDATEUSECASE_H
