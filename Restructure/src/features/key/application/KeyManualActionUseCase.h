#ifndef KEYMANUALACTIONUSECASE_H
#define KEYMANUALACTIONUSECASE_H

#include <QString>

enum class KeyManualAction
{
    Initialize,
    DownloadRfid,
    QueryBattery,
    SyncTime,
    QueryTasks
};

enum class KeyManualActionReason
{
    Ready,
    WorkbenchConfirmationActive,
    SerialDisconnected,
    KeyNotReady,
    ProvisioningBusy
};

struct KeyManualActionRequest
{
    KeyManualAction action = KeyManualAction::QueryTasks;
    bool workbenchConfirmationActive = false;
    bool serialConnected = false;
    bool keyReady = false;
    bool provisioningBusy = false;
};

struct KeyManualActionDecision
{
    KeyManualActionReason reason = KeyManualActionReason::Ready;
    QString message;
    bool allowed = false;
};

// Pure admission rules for the five operator-triggered key commands. Keeping
// these conditions together avoids duplicating UI-oriented if chains in L1.
class KeyManualActionUseCase
{
public:
    static KeyManualActionDecision evaluate(
            const KeyManualActionRequest &request);
};

#endif // KEYMANUALACTIONUSECASE_H
