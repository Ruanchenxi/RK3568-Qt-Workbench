#ifndef KEYMAINTENANCESERVICE_H
#define KEYMAINTENANCESERVICE_H

#include <functional>

#include <QObject>

#include "shared/contracts/IKeySessionService.h"

class QTimer;

// Owns periodic and post-query A/B-seat maintenance scheduling. It does not
// own ticket workflows or protocol request state.
class KeyMaintenanceService : public QObject
{
public:
    using BusinessBusyProvider = std::function<bool()>;

    explicit KeyMaintenanceService(IKeySessionService *session,
                                   QObject *parent = nullptr);

    // Supplies the application-level exclusion rule without coupling this
    // service to transfer, return, deletion, or cancellation contexts.
    void setBusinessBusyProvider(BusinessBusyProvider provider);

    // Applies the product maintenance policy once. Idle periodic polling is
    // paused; post-Q_TASK maintenance remains active through afterTasksUpdated.
    void start();

    // Schedules time synchronization after a valid Q_TASK observation when
    // A-seat business is idle; automatic battery polling stays disabled.
    void afterTasksUpdated(const KeySessionSnapshot &snapshot);

private:
    bool aSeatBusinessBusy() const;
    void refreshASeatBattery();
    void refreshBSeatBattery();

    IKeySessionService *m_session;
    BusinessBusyProvider m_businessBusyProvider;
    QTimer *m_aSeatTimer;
    QTimer *m_bSeatTimer;
    bool m_started = false;
};

#endif // KEYMAINTENANCESERVICE_H
