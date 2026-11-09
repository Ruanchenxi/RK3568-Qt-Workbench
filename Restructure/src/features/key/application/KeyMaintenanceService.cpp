#include "features/key/application/KeyMaintenanceService.h"

#include <QTimer>
#include <utility>

// Creates the two seat-specific timers without starting hardware work early.
KeyMaintenanceService::KeyMaintenanceService(
        IKeySessionService *session,
        QObject *parent)
    : QObject(parent)
    , m_session(session)
    , m_aSeatTimer(new QTimer(this))
    , m_bSeatTimer(new QTimer(this))
{
    Q_ASSERT(m_session);
    m_aSeatTimer->setInterval(300000);
    m_bSeatTimer->setInterval(600000);
    connect(m_aSeatTimer, &QTimer::timeout,
            this, [this]() { refreshASeatBattery(); });
    connect(m_bSeatTimer, &QTimer::timeout,
            this, [this]() { refreshBSeatBattery(); });
}

// Injects A-seat business exclusion while keeping workflow types outside.
void KeyMaintenanceService::setBusinessBusyProvider(
        BusinessBusyProvider provider)
{
    m_businessBusyProvider = std::move(provider);
}

// Keeps idle periodic polling paused; Q_TASK-triggered maintenance and manual
// battery queries remain available without adding background serial traffic.
void KeyMaintenanceService::start()
{
    if (m_started) {
        return;
    }
    m_started = true;
    m_aSeatTimer->stop();
    m_bSeatTimer->stop();
}

// Keeps the key clock accurate after a ready Q_TASK observation without an
// automatic battery query; battery remains available through the manual action.
void KeyMaintenanceService::afterTasksUpdated(
        const KeySessionSnapshot &snapshot)
{
    if (!snapshot.keyPresent
            || !snapshot.keyStable
            || !snapshot.sessionReady
            || snapshot.commandInFlight
            || aSeatBusinessBusy()) {
        return;
    }

    m_session->startAuxSequence(
                0x01,
                KeyAuxSequenceMode::SyncOnly,
                KeyAuxSequenceOrigin::AutoAfterTasks,
                QStringLiteral("auto_after_tasks"));
}

// Treats a missing provider as idle and delegates all workflow knowledge.
bool KeyMaintenanceService::aSeatBusinessBusy() const
{
    return m_businessBusyProvider && m_businessBusyProvider();
}

// Refreshes A-seat battery only when the interactive key session is idle.
void KeyMaintenanceService::refreshASeatBattery()
{
    const KeySessionSnapshot snapshot = m_session->snapshot();
    if (!snapshot.connected
            || !snapshot.keyPresent
            || !snapshot.keyStable
            || !snapshot.sessionReady
            || snapshot.commandInFlight
            || aSeatBusinessBusy()) {
        return;
    }

    m_session->startAuxSequence(
                0x01,
                KeyAuxSequenceMode::BatteryOnly,
                KeyAuxSequenceOrigin::PeriodicRefresh,
                QStringLiteral("periodic_refresh"));
}

// Refreshes passive B-seat battery without borrowing A-seat business state.
void KeyMaintenanceService::refreshBSeatBattery()
{
    const KeySessionSnapshot snapshot = m_session->snapshot();
    if (!snapshot.connected
            || !snapshot.bKeyPresent
            || snapshot.commandInFlight) {
        return;
    }

    m_session->startAuxSequence(
                0x02,
                KeyAuxSequenceMode::BatteryOnly,
                KeyAuxSequenceOrigin::PeriodicRefresh,
                QStringLiteral("b_periodic_refresh"));
}
