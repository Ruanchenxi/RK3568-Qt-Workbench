/**
 * @file KeyManageControllerSession.cpp
 * @brief Key-session identity, invalidation, and bounded retry coordination.
 *
 * Session ids and bridge epochs protect operation-local evidence from a
 * removed or replaced key. This file contains coordination only; protocol and
 * workflow state remain in their dedicated owners.
 */
#include "features/key/application/KeyManageController.h"

#include <QDebug>
#include <QTimer>
#include "features/key/application/KeyPortRuntimeCoordinator.h"
#include "features/key/application/KeyTaskObservationService.h"
#include "features/key/application/TicketDeleteWorkflowService.h"
#include "features/key/application/TicketReturnWorkflowService.h"
#include "features/key/application/TicketTransferWorkflowService.h"

namespace {

bool isSessionReadySnapshot(const KeySessionSnapshot &snapshot)
{
    return snapshot.connected
            && snapshot.keyPresent
            && snapshot.keyStable
            && snapshot.sessionReady;
}

} // namespace
void KeyManageController::tryAutoRefreshKeyTasksOnReady(const KeySessionSnapshot &snapshot)
{
    const bool ready = isSessionReadySnapshot(snapshot);
    if (!m_keyTaskObservation->beginReadyEdgeQuery(
                ready,
                m_ticketTransferWorkflow->hasActiveOperation(),
                m_keySessionId)) {
        return;
    }
    emit statusMessage(QStringLiteral("钥匙已就绪，自动读取钥匙票列表以检查待回传任务"));
    onQueryTasksClicked();
}

bool KeyManageController::isCurrentKeySession(int sessionId) const
{
    return sessionId > 0 && sessionId == m_keySessionId;
}

void KeyManageController::openNewKeySession()
{
    m_keySessionId = ++m_keySessionSeed;
    m_keyPortRuntime->acceptBridgeEpoch(m_session->snapshot().bridgeEpoch);
    m_keyTaskObservation->beginSession();
    emit statusMessage(QStringLiteral("钥匙会话已建立：session=%1").arg(m_keySessionId));
}

void KeyManageController::invalidateCurrentKeySession(const QString &reason)
{
    if (m_keySessionId <= 0) {
        return;
    }

    const int expiredSessionId = m_keySessionId;
    m_keySessionId = 0;
    // Session loss removes cached tasks so stale observations cannot authorize work.
    m_keyTaskObservation->invalidateSession();
    m_ticketDeleteWorkflow->resetRetryEvidence();
    m_ticketReturnWorkflow->clearProgressiveSweep();

    emit statusMessage(
            reason.trimmed().isEmpty()
            ? QStringLiteral("钥匙会话已失效：session=%1").arg(expiredSessionId)
            : QStringLiteral("钥匙会话已失效：session=%1，原因=%2")
                  .arg(expiredSessionId)
                  .arg(reason));
}

void KeyManageController::scheduleAutoTransferRetry(const QString &reason)
{
    if (m_autoTransferRetryScheduled) {
        return;
    }

    m_autoTransferRetryScheduled = true;
    qInfo().noquote()
            << QStringLiteral("[KeyManageController] scheduleAutoTransferRetry reason=%1 currentSession=%2")
                  .arg(reason.isEmpty() ? QStringLiteral("<none>") : reason,
                       QString::number(m_keySessionId));
    QTimer::singleShot(300, this, [this]() {
        m_autoTransferRetryScheduled = false;
        qInfo().noquote()
                << QStringLiteral("[KeyManageController] auto transfer retry timer fired currentSession=%1")
                      .arg(QString::number(m_keySessionId));
        tryAutoTransferPendingTicket();
    });
}

int KeyManageController::nextOpId()
{
    return m_nextOpId++;
}

QList<KeyTaskDto> KeyManageController::toTaskDtos(const QVariantList &taskList) const
{
    QList<KeyTaskDto> tasks;
    tasks.reserve(taskList.size());

    for (const QVariant &v : taskList) {
        const QVariantMap item = v.toMap();
        KeyTaskDto task;
        task.taskId = item.value("taskId").toByteArray();
        task.status = static_cast<quint8>(item.value("status").toInt());
        task.type = static_cast<quint8>(item.value("type").toInt());
        task.source = static_cast<quint8>(item.value("source").toInt());
        task.reserved = static_cast<quint8>(item.value("reserved").toInt());
        tasks.append(task);
    }

    return tasks;
}

// ─── 双座辅助链 ────────────────────────────────────────────────────────────────
