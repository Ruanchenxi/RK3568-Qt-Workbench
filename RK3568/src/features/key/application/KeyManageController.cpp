/**
 * @file KeyManageController.cpp
 * @brief 钥匙管理页控制器实现
 */
#include "features/key/application/KeyManageController.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonParseError>
#include <QDebug>
#include <QStringList>
#include <QTimer>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrlQuery>

#include "core/AppContext.h"
#include "core/ConfigManager.h"
#include "features/key/application/KeySessionService.h"
#include "features/key/application/KeyProvisioningService.h"
#include "features/key/application/TicketCancelCoordinator.h"
#include "features/key/application/TicketStore.h"
#include "features/key/application/TicketIngressService.h"
#include "features/key/application/TicketReturnHttpClient.h"
#include "features/auth/infra/password/AuthServiceAdapter.h"
#include "features/key/protocol/KeyProtocolDefs.h"
#include "features/key/protocol/KeySerialClient.h"

namespace {

int resolveBusinessStationNo()
{
    ConfigManager *config = ConfigManager::instance();
    bool ok = false;
    const int stationNo = config->stationId().trimmed().toInt(&ok);
    if (ok && stationNo >= 0) {
        return stationNo;
    }

    const int legacyStationNo = config->value(QStringLiteral("key/backendStationNo"), 0).toInt(&ok);
    return (ok && legacyStationNo >= 0) ? legacyStationNo : 0;
}

QString rawTaskIdToDecimalString(const QByteArray &taskIdRaw)
{
    if (taskIdRaw.size() < 8) {
        return QString();
    }

    qulonglong value = 0;
    for (int i = 0; i < 8; ++i) {
        value |= (static_cast<qulonglong>(static_cast<quint8>(taskIdRaw[i])) << (i * 8));
    }
    return QString::number(value);
}

bool isSessionReadySnapshot(const KeySessionSnapshot &snapshot)
{
    return snapshot.connected
            && snapshot.keyPresent
            && snapshot.keyStable
            && snapshot.sessionReady;
}

bool isReturnInFlightState(const QString &state)
{
    return state == QLatin1String("return-requesting-log")
            || state == QLatin1String("return-handshake")
            || state == QLatin1String("return-log-requesting")
            || state == QLatin1String("return-log-receiving")
            || state == QLatin1String("return-uploading");
}

bool isReturnCleanupState(const QString &state)
{
    return state == QLatin1String("return-upload-success")
            || state == QLatin1String("return-delete-pending")
            || state == QLatin1String("return-delete-verifying")
            || state == QLatin1String("return-delete-success");
}

bool shouldSkipAutoReturnForState(const QString &state)
{
    return state == QLatin1String("return-success")
            || state == QLatin1String("return-delete-success")
            || state == QLatin1String("return-delete-failed")
            || isReturnInFlightState(state)
            || isReturnCleanupState(state)
            || state == QLatin1String("manual-required")
            || state == QLatin1String("upload_uncertain")
            || state == QLatin1String("return-failed");
}

bool isAutomaticTransferSource(const SystemTicketDto &ticket)
{
    return ticket.transferTriggerSource == QLatin1String("automatic");
}

QString extractTaskIdFromWorkbenchJson(const QByteArray &jsonBytes)
{
    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(jsonBytes, &pe);
    if (doc.isNull() || !doc.isObject()) {
        return QString();
    }

    const QJsonObject root = doc.object();
    const QJsonObject ticketObj = (root.contains("data") && root.value("data").isObject())
            ? root.value("data").toObject()
            : root;
    return ticketObj.value("taskId").toString().trimmed();
}

}

KeyManageController::KeyManageController(IKeySessionService *session,
                                         SerialLogManager *logManager,
                                         QObject *parent)
    : QObject(parent)
    , m_session(session)
    , m_logManager(logManager)
    , m_ticketStore(new TicketStore(this))
    , m_ticketIngress(new TicketIngressService(this))
    , m_ticketReturnClient(new TicketReturnHttpClient(this))
    , m_keyProvisioning(new KeyProvisioningService(nullptr, this))
    , m_ticketCancelCoordinator(new TicketCancelCoordinator(this))
    , m_terminationNetwork(new QNetworkAccessManager(this))
    , m_nextOpId(1)
    , m_selectedSystemTicketId()
    , m_activeTransferTaskId()
    , m_activeReturnTaskId()
    , m_activeReturnTaskIdRaw()
    , m_httpTransactionSafetyTimer(new QTimer(this))
{
    m_httpTransactionSafetyTimer->setSingleShot(true);
    m_httpTransactionSafetyTimer->setInterval(60000); // 60 秒安全超时
    connect(m_httpTransactionSafetyTimer, &QTimer::timeout, this, [this]() {
        const QString timedOutTaskId = m_httpTransferTransactionTaskId;
        qWarning().noquote()
                << QStringLiteral("[KeyManageController] HTTP transfer transaction safety timeout fired! "
                                  "Force-clearing stuck lock. taskId=%1 session=%2 epoch=%3")
                      .arg(m_httpTransferTransactionTaskId.isEmpty()
                                   ? QStringLiteral("<none>")
                                   : m_httpTransferTransactionTaskId,
                           QString::number(m_httpTransferTransactionSessionId),
                           QString::number(m_httpTransferTransactionBridgeEpoch));
        emit statusMessage(QStringLiteral("传票互斥锁超时（60s），强制释放，请检查日志"));
        clearHttpTransferTransaction(true);
        if (!timedOutTaskId.isEmpty()) {
            m_ticketStore->updateTransferState(timedOutTaskId,
                                               QStringLiteral("failed"),
                                               QStringLiteral("HTTP传票事务超时，已强制清理活动上下文"));
        }
    });
    if (!m_session) {
        m_session = new KeySessionService(this);
    }
    if (!m_logManager) {
        m_logManager = new SerialLogManager(this);
    }

    connect(m_session, &IKeySessionService::eventOccurred,
            this, &KeyManageController::handleSessionEvent);
    connect(m_session, &IKeySessionService::logItemEmitted,
            this, &KeyManageController::handleLogItem);
    connect(m_ticketStore, &TicketStore::ticketsChanged,
            this, &KeyManageController::handleSystemTicketsChanged);
    connect(m_ticketIngress, &TicketIngressService::httpServerLogAppended,
            this, &KeyManageController::handleHttpServerLog);
    m_ticketIngress->setIngressHandler(
            [this](const QByteArray &jsonBytes,
                   const TicketIngressService::PersistBodyFn &persistBody) {
        TicketIngressService::IngressResult result;
        QString taskId = extractTaskIdFromWorkbenchJson(jsonBytes);
        if (taskId.isEmpty()) {
            result.accepted = false;
            result.statusCode = 200;
            result.message = QStringLiteral("传票请求中 taskId 为空，无法处理");
            qInfo().noquote()
                    << QStringLiteral("[KeyManageController] ticket ingress rejected reason=empty-taskId");
            return result;
        }

        const TicketCancelCoordinator::CancelRecord trackedCancel =
                m_ticketCancelCoordinator->recordFor(taskId);
        if (trackedCancel.valid
                && (trackedCancel.cancelState == QLatin1String("cancel-accepted")
                    || trackedCancel.cancelState == QLatin1String("cancel-pending")
                    || trackedCancel.cancelState == QLatin1String("cancel-executing"))) {
            result.accepted = false;
            result.statusCode = 200;
            result.message = QStringLiteral("该任务已受理撤销，当前不再接受重复传票");
            result.taskId = taskId;
            qInfo().noquote()
                    << QStringLiteral("[KeyManageController] ticket ingress rejected reason=cancel-active taskId=%1 cancelState=%2")
                          .arg(taskId, trackedCancel.cancelState);
            return result;
        }
        // WHY: 钥匙未就绪时禁止执行钥匙传票（本地 HTTP 入口硬门禁）
        // 在票进入本地票池之前，先检查钥匙状态，避免无钥匙时票错误进入待传链
        const KeySessionSnapshot snapshot = m_session->snapshot();
        const bool keyReady = snapshot.connected
                && snapshot.keyPresent
                && snapshot.keyStable
                && snapshot.sessionReady;

        if (!keyReady) {
            result.accepted = false;
            result.statusCode = 200;  // 业务拒绝仍返回 HTTP 200
            result.message = QStringLiteral("未检测到钥匙在位，请确认！");
            result.taskId.clear();
            qInfo().noquote()
                    << QStringLiteral("[KeyManageController] ticket ingress rejected reason=key-not-ready connected=%1 keyPresent=%2 keyStable=%3 sessionReady=%4")
                          .arg(snapshot.connected ? QStringLiteral("true") : QStringLiteral("false"),
                               snapshot.keyPresent ? QStringLiteral("true") : QStringLiteral("false"),
                               snapshot.keyStable ? QStringLiteral("true") : QStringLiteral("false"),
                               snapshot.sessionReady ? QStringLiteral("true") : QStringLiteral("false"));
            return result;
        }
        // commandInFlight / recoveryWindowActive 不在此处拦截：
        // 这两个状态是瞬时的（Q_TASK、电量查询等随时在途），在 ingress 层硬拦会导致用户几乎永远无法传票。
        // 让请求先入池，由 startDirectWorkbenchTransfer 判断——如果当时仍在途则标记 failed 并返回错误，
        // 用户重试即可，不会被永久卡死。

        if (m_httpTransferTransactionActive) {
            result.accepted = false;
            result.statusCode = 200;
            result.message = QStringLiteral("当前已有工作台传票确认事务在执行，请稍后重试");
            result.taskId = taskId;
            qInfo().noquote()
                    << QStringLiteral("[KeyManageController] ticket ingress rejected reason=http-transaction-active taskId=%1 activeTaskId=%2")
                          .arg(taskId.isEmpty() ? QStringLiteral("<empty>") : taskId,
                               m_httpTransferTransactionTaskId.isEmpty()
                                       ? QStringLiteral("<none>")
                                       : m_httpTransferTransactionTaskId);
            return result;
        }

        const SystemTicketDto oldTicket = m_ticketStore->ticketById(taskId);
        if (oldTicket.valid) {
            if (oldTicket.transferState == QLatin1String("success")) {
                // 若有最新 Q_TASK 数据且该任务已不在钥匙中，说明本地状态陈旧，允许重传
                const bool hasKeyTaskData = !m_lastKeyTasks.isEmpty();
                const bool taskStillInKey = taskExistsInList(m_lastKeyTasks, taskId);
                if (hasKeyTaskData && !taskStillInKey) {
                    qInfo().noquote()
                            << QStringLiteral("[KeyManageController] ticket ingress: local success state stale (taskId not in lastKeyTasks), allowing re-transfer taskId=%1")
                                  .arg(taskId);
                    m_ticketStore->updateTransferState(taskId, QStringLiteral("received"));
                } else {
                    result.accepted = false;
                    result.statusCode = 200;
                    result.message = QStringLiteral("该任务已传票到钥匙，请勿重复操作");
                    result.taskId = taskId;
                    return result;
                }
            }
            if (oldTicket.transferState == QLatin1String("sending")) {
                result.accepted = false;
                result.statusCode = 200;
                result.message = QStringLiteral("该任务正在传票中，请勿重复操作");
                result.taskId = taskId;
                return result;
            }
            if (oldTicket.transferState == QLatin1String("received")
                    || oldTicket.transferState == QLatin1String("auto-pending")) {
                result.accepted = false;
                result.statusCode = 200;
                result.message = QStringLiteral("该任务已有待处理的传票请求，请勿重复操作");
                result.taskId = taskId;
                return result;
            }
        }

        const int expectedSessionId = m_keySessionId;
        const int expectedBridgeEpoch = snapshot.bridgeEpoch;
        if (!m_activeReturnTaskId.isEmpty()
                || !m_pendingReturnHandshakeTaskId.isEmpty()
                || !m_pendingDeletedSystemTicketId.isEmpty()) {
            result.accepted = false;
            result.statusCode = 200;
            result.message = QStringLiteral("当前钥匙仍有回传或删除确认中的业务，请稍后重试");
            result.taskId = taskId;
            return result;
        }
        if (m_autoQuerySessionId == expectedSessionId && expectedSessionId > 0) {
            result.accepted = false;
            result.statusCode = 200;
            result.message = QStringLiteral("当前钥匙通讯正在确认，请稍后重试");
            result.taskId = taskId;
            return result;
        }

        // P0：先入池（received），再异步发送，立即返回 success=true
        // m_httpTransferTransactionActive 充当并发互斥锁，在传票异步完成后由事件处理器释放
        // setHttpTransferTransaction 同时启动 60 秒安全超时，防止锁永久卡死
        setHttpTransferTransaction(taskId, expectedSessionId, expectedBridgeEpoch);

        const QString savedPath = persistBody();
        QString ingestError;
        if (!m_ticketStore->ingestJson(jsonBytes, savedPath, &ingestError)) {
            qWarning().noquote()
                    << QStringLiteral("[KeyManageController] ticket ingress ingest failed taskId=%1 reason=%2")
                          .arg(taskId, ingestError);
            clearHttpTransferTransaction(false);
            result.accepted = false;
            result.statusCode = 200;
            result.message = QStringLiteral("传票请求入池失败：%1").arg(ingestError);
            result.taskId = taskId;
            return result;
        }

        QString transferBlockedReason;
        if (!startDirectWorkbenchTransfer(taskId,
                                          jsonBytes,
                                          expectedSessionId,
                                          expectedBridgeEpoch,
                                          &transferBlockedReason)) {
            m_ticketStore->updateTransferState(taskId, QStringLiteral("failed"), transferBlockedReason);
            clearHttpTransferTransaction(false);
            result.accepted = false;
            result.statusCode = 200;
            result.message = transferBlockedReason.isEmpty()
                    ? QStringLiteral("钥匙状态在发送前发生变化，请重新点击钥匙传票")
                    : transferBlockedReason;
            result.taskId = taskId;
            return result;
        }

        // 传票已开始异步发送，挂起 socket 等待串口 + Q_TASK 确认
        // TicketTransferFinished/TicketTransferFailed 事件处理器负责调用 completeDeferredResponse
        result.accepted = true;
        result.deferred = true;
        result.statusCode = 200;
        result.message = QStringLiteral("传票请求已开始发送");
        result.taskId = taskId;
        return result;
    });

    // ── cancelTicket ingress handler ──
    m_ticketIngress->setCancelHandler([this](const QString &taskId) {
        TicketIngressService::IngressResult result;
        result.statusCode = 200;
        result.taskId = taskId;

        if (taskId.isEmpty()) {
            result.accepted = false;
            result.message = QStringLiteral("taskId 为空");
            return result;
        }

        const TicketCancelCoordinator::CancelRecord trackedRecord =
                m_ticketCancelCoordinator->recordFor(taskId);

        if (trackedRecord.valid) {
            if (trackedRecord.cancelState == QLatin1String("cancel-done")) {
                result.accepted = true;
                result.message = QStringLiteral("任务已取消");
                return result;
            }
            if (trackedRecord.cancelState == QLatin1String("cancel-accepted")
                    || trackedRecord.cancelState == QLatin1String("cancel-pending")
                    || trackedRecord.cancelState == QLatin1String("cancel-executing")) {
                result.accepted = true;
                result.message = QStringLiteral("任务取消已受理");
                return result;
            }
        }

        SystemTicketDto ticket = m_ticketStore->ticketById(taskId);

        // 票不存在 — 尝试孤儿恢复（重启后票池丢失但钥匙里仍有任务）
        if (!ticket.valid) {
            quint8 orphanStatus = 0xFF;
            const QByteArray orphanRaw = findKeyTaskIdRaw(taskId, &orphanStatus);
            if (!orphanRaw.isEmpty()) {
                // 钥匙里有这个任务，建孤儿恢复记录后继续走正常流程
                m_ticketStore->ingestOrphanTicket(taskId);
                ticket = m_ticketStore->ticketById(taskId);
                qInfo().noquote()
                        << QStringLiteral("[KeyManageController] cancel ingress orphan-recovery taskId=%1 keyStatus=0x%2")
                              .arg(taskId, QString::number(orphanStatus, 16));
            } else {
                result.accepted = false;
                result.message = QStringLiteral("未找到对应任务");
                qInfo().noquote()
                        << QStringLiteral("[KeyManageController] cancel ingress rejected reason=not-found taskId=%1")
                              .arg(taskId);
                return result;
            }
        }

        // 已有撤销意图（幂等）
        if (!ticket.cancelState.isEmpty()
                && ticket.cancelState != QLatin1String("none")) {
            result.accepted = true;
            result.message = QStringLiteral("任务取消已受理");
            m_ticketCancelCoordinator->recordAccepted(taskId,
                                                     ticket,
                                                     ticket.cancelState,
                                                     result.message);
            qInfo().noquote()
                    << QStringLiteral("[KeyManageController] cancel ingress idempotent taskId=%1 cancelState=%2")
                          .arg(taskId, ticket.cancelState);
            return result;
        }

        // 已完成回传闭环（幂等成功）
        if (ticket.returnState == QLatin1String("return-delete-success")
                || ticket.returnState == QLatin1String("return-success")
                || ticket.transferState == QLatin1String("key-cleared")) {
            result.accepted = true;
            result.message = QStringLiteral("任务已完成，无需撤销");
            m_ticketCancelCoordinator->recordDone(taskId, ticket, result.message);
            qInfo().noquote()
                    << QStringLiteral("[KeyManageController] cancel ingress no-op taskId=%1 returnState=%2 transferState=%3")
                          .arg(taskId, ticket.returnState, ticket.transferState);
            return result;
        }

        // 已在回传清理链中（不新起撤销链）
        if (ticket.returnState == QLatin1String("return-upload-success")
                || ticket.returnState == QLatin1String("return-delete-pending")
                || ticket.returnState == QLatin1String("return-delete-verifying")) {
            result.accepted = true;
            result.message = QStringLiteral("任务已在清理中，无需重复撤销");
            qInfo().noquote()
                    << QStringLiteral("[KeyManageController] cancel ingress already-in-cleanup taskId=%1 returnState=%2")
                          .arg(taskId, ticket.returnState);
            return result;
        }

        // 已传到钥匙（success）：必须以"当前钥匙里能看到这张任务"为硬门槛，否则拒绝
        const KeySessionSnapshot snapshot = m_session->snapshot();
        if (ticket.transferState == QLatin1String("success")) {
            const bool keyReady = snapshot.connected && snapshot.keyPresent && snapshot.sessionReady;
            const bool hasTaskList = !m_lastKeyTasks.isEmpty();
            const bool taskInKey = taskExistsInList(m_lastKeyTasks, taskId);
            if (!keyReady || !hasTaskList || !taskInKey) {
                result.accepted = false;
                result.message = QStringLiteral("任务已下发到钥匙，当前无法确认钥匙中仍存在该任务，无法撤销");
                qInfo().noquote()
                        << QStringLiteral("[KeyManageController] cancel ingress rejected reason=key-task-not-confirmed taskId=%1 keyReady=%2 hasTaskList=%3 taskInKey=%4")
                              .arg(taskId,
                                   keyReady ? QStringLiteral("true") : QStringLiteral("false"),
                                   hasTaskList ? QStringLiteral("true") : QStringLiteral("false"),
                                   taskInKey ? QStringLiteral("true") : QStringLiteral("false"));
                return result;
            }
        }

        if ((ticket.transferState == QLatin1String("received")
                    || ticket.transferState == QLatin1String("auto-pending"))
                && !taskExistsInList(m_lastKeyTasks, taskId)) {
            m_ticketCancelCoordinator->recordDone(taskId, ticket, QStringLiteral("任务已取消"));
            m_ticketStore->removeTicket(taskId);
            result.accepted = true;
            result.message = QStringLiteral("任务已取消");
            qInfo().noquote()
                    << QStringLiteral("[KeyManageController] cancel ingress direct-remove taskId=%1 transferState=%2")
                          .arg(taskId, ticket.transferState);
            return result;
        }

        // 其他情况：受理撤销，但不抢占当前链路。
        const bool currentlyInKey = taskExistsInList(m_lastKeyTasks, taskId);
        QString blockedReason;
        const bool canProcessNow = canProcessCancelNow(&blockedReason);
        const QString nextCancelState = canProcessNow
                ? QStringLiteral("cancel-pending")
                : QStringLiteral("cancel-accepted");

        m_ticketStore->updateCancelTracking(taskId,
                                            nextCancelState,
                                            QStringLiteral("workbench-http"),
                                            ticket.residentSlotId.isEmpty() ? QStringLiteral("slot-1") : ticket.residentSlotId,
                                            ticket.residentKeyId,
                                            QDateTime::currentDateTime(),
                                            currentlyInKey,
                                            canProcessNow
                                                ? QStringLiteral("已受理撤销，等待 Q_TASK 对账后执行删除")
                                                : QStringLiteral("已受理撤销，等待当前业务链收口"));
        m_ticketCancelCoordinator->recordAccepted(taskId,
                                                 m_ticketStore->ticketById(taskId),
                                                 nextCancelState,
                                                 canProcessNow
                                                     ? QStringLiteral("任务取消已受理，准备对账执行")
                                                     : QStringLiteral("任务取消已受理，待当前任务收口后执行"));
        result.accepted = true;
        result.message = canProcessNow
                ? QStringLiteral("任务取消已受理，准备对账执行")
                : QStringLiteral("任务取消已受理，待当前任务收口后执行");
        qInfo().noquote()
                << QStringLiteral("[KeyManageController] cancel ingress accepted taskId=%1 transferState=%2 returnState=%3 nextCancelState=%4 blockedReason=%5")
                      .arg(taskId,
                           ticket.transferState,
                           ticket.returnState,
                           nextCancelState,
                           blockedReason.isEmpty() ? QStringLiteral("<none>") : blockedReason);
        if (canProcessNow) {
            schedulePendingCancelReconcile(QStringLiteral("cancel-request-accepted"), 150);
        }
        return result;
    });

    connect(m_ticketReturnClient, &TicketReturnHttpClient::logAppended,
            this, &KeyManageController::handleHttpClientLog);
    connect(m_ticketReturnClient, &TicketReturnHttpClient::uploadSucceeded,
            this, &KeyManageController::handleReturnUploadSucceeded);
    connect(m_ticketReturnClient, &TicketReturnHttpClient::uploadFailed,
            this, &KeyManageController::handleReturnUploadFailed);
    connect(m_keyProvisioning, &KeyProvisioningService::logAppended,
            this, &KeyManageController::handleHttpClientLog);
    connect(m_keyProvisioning, &KeyProvisioningService::initPayloadReady,
            this, &KeyManageController::handleInitPayloadReady);
    connect(m_keyProvisioning, &KeyProvisioningService::rfidPayloadReady,
            this, &KeyManageController::handleRfidPayloadReady);
    connect(m_keyProvisioning, &KeyProvisioningService::requestFailed,
            this, &KeyManageController::handleProvisionRequestFailed);
    connect(m_logManager, &SerialLogManager::overflowDropped, this, [this]() {
        emit statusMessage(QStringLiteral("日志达到上限，已自动丢弃旧日志"));
    });
    connect(ConfigManager::instance(), &ConfigManager::configChanged,
            this, [this](const QString &key, const QVariant &) {
        if (key == QLatin1String("serial/keyPort")) {
            refreshKeySerialApplyStatus();
        }
    });
}

void KeyManageController::start()
{
    m_ticketStore->loadFromDisk();

    auto retryPendingTerminations = [this]() {
        for (const SystemTicketDto &ticket : m_ticketStore->tickets()) {
            if (ticket.adminDeleteStage
                    == SystemTicketDto::AdminDeleteStage::KeyClearedAwaitingTermination) {
                callTerminationThenRemove(ticket.taskId);
            }
        }
    };
    connect(AppContext::instance(), &AppContext::userLoggedIn,
            this, [retryPendingTerminations](const UserInfo &user) {
        // 只有管理员 token 才有权限调工作台 termination 接口
        if (!user.role.contains(QStringLiteral("admin"), Qt::CaseInsensitive)) {
            return;
        }
        retryPendingTerminations();
    });
    // 控制器启动时若管理员已登录，立即重试（覆盖"先登录后启动"场景）
    const UserInfo &currentUser = AppContext::instance()->currentUser();
    if (currentUser.isValid()
            && currentUser.role.contains(QStringLiteral("admin"), Qt::CaseInsensitive)) {
        retryPendingTerminations();
    }

    m_ticketCancelCoordinator->start();
    tryAutoConnectKeyPort();

    // A座：5 分钟定期刷新电量（只在 A座就绪且无命令在途时执行）
    m_autoBatteryRefreshTimer = new QTimer(this);
    m_autoBatteryRefreshTimer->setInterval(300000);
    connect(m_autoBatteryRefreshTimer, &QTimer::timeout,
            this, &KeyManageController::onAutoBatteryRefreshTimeout);
    m_autoBatteryRefreshTimer->start();

    // B座：10 分钟定期刷新电量（完全独立于 A座业务，只要 B座在位即执行）
    m_autoBatteryRefreshTimerB = new QTimer(this);
    m_autoBatteryRefreshTimerB->setInterval(600000);
    connect(m_autoBatteryRefreshTimerB, &QTimer::timeout,
            this, &KeyManageController::onAutoBatteryRefreshTimeoutB);
    m_autoBatteryRefreshTimerB->start();

    if (!m_ticketIngress->start()) {
        emit statusMessage(m_ticketIngress->lastError());
    }
    const KeySessionSnapshot snapshot = currentSnapshot();
    m_lastReadyState = snapshot.connected
            && snapshot.keyPresent
            && snapshot.keyStable
            && snapshot.sessionReady;
    m_acceptedBridgeEpoch = snapshot.bridgeEpoch;
    emit sessionSnapshotChanged(snapshot);
    emit systemTicketsUpdated(systemTickets());
    refreshKeySerialApplyStatus();
    schedulePendingCancelReconcile(QStringLiteral("startup"), 500);
}

void KeyManageController::onInitClicked()
{
    const KeySessionSnapshot snapshot = m_session->snapshot();
    if (!snapshot.connected) {
        emit statusMessage(QStringLiteral("串口未连接，无法执行初始化"));
        return;
    }
    if (!snapshot.keyPresent || !snapshot.keyStable || !snapshot.sessionReady) {
        emit statusMessage(QStringLiteral("钥匙未就绪，无法执行初始化"));
        return;
    }
    if (m_keyProvisioning->isBusy()) {
        emit statusMessage(QStringLiteral("正在获取钥匙初始化数据，请稍后再试"));
        return;
    }

    const int stationNo = resolveBusinessStationNo();
    emit statusMessage(QStringLiteral("初始化钥匙，正在获取初始化数据..."));
    m_keyProvisioning->requestInitPayload(stationNo);
}

void KeyManageController::onDownloadRfidClicked()
{
    const KeySessionSnapshot snapshot = m_session->snapshot();
    if (!snapshot.connected) {
        emit statusMessage(QStringLiteral("串口未连接，无法下载 RFID"));
        return;
    }
    if (!snapshot.keyPresent || !snapshot.keyStable || !snapshot.sessionReady) {
        emit statusMessage(QStringLiteral("钥匙未就绪，无法下载 RFID"));
        return;
    }
    if (m_keyProvisioning->isBusy()) {
        emit statusMessage(QStringLiteral("正在获取 RFID 数据，请稍后再试"));
        return;
    }

    const int stationNo = resolveBusinessStationNo();
    emit statusMessage(QStringLiteral("下载 RFID，正在获取后端数据..."));
    m_keyProvisioning->requestRfidPayload(stationNo);
}

void KeyManageController::onQueryBatteryClicked()
{
    const KeySessionSnapshot snapshot = m_session->snapshot();
    if (!snapshot.connected) {
        emit statusMessage(QStringLiteral("串口未连接，无法查询钥匙电量"));
        return;
    }
    if (!snapshot.keyPresent || !snapshot.keyStable || !snapshot.sessionReady) {
        emit statusMessage(QStringLiteral("A座钥匙未就绪，无法查询电量"));
        return;
    }
    CommandRequest req;
    req.opId = nextOpId();
    req.id = CommandId::QueryBattery;
    req.options.insert(QStringLiteral("seat"), static_cast<uint>(0x01));
    m_session->execute(req);
    emit statusMessage(QStringLiteral("执行获取电量 (Q_KEYEQ)，目标=A座"));
}

void KeyManageController::onSyncDeviceTimeClicked()
{
    const KeySessionSnapshot snapshot = m_session->snapshot();
    if (!snapshot.connected) {
        emit statusMessage(QStringLiteral("串口未连接，无法执行钥匙校时"));
        return;
    }
    if (!snapshot.keyPresent || !snapshot.keyStable || !snapshot.sessionReady) {
        emit statusMessage(QStringLiteral("A座钥匙未就绪，无法执行校时"));
        return;
    }
    CommandRequest req;
    req.opId = nextOpId();
    req.id = CommandId::SyncDeviceTime;
    req.options.insert(QStringLiteral("seat"), static_cast<uint>(0x01));
    m_session->execute(req);
    emit statusMessage(QStringLiteral("执行钥匙校时 (SET_TIME)，目标=A座"));
}

void KeyManageController::onQueryTasksClicked()
{
    if (m_httpTransferTransactionActive) {
        emit statusMessage(QStringLiteral("当前工作台传票正在做钥匙任务确认，请稍后再查询"));
        return;
    }

    const KeySessionSnapshot snapshot = m_session->snapshot();
    if (!snapshot.connected) {
        emit statusMessage(QStringLiteral("串口未连接，无法查询任务"));
        return;
    }

    CommandRequest req;
    req.id = CommandId::QueryTasks;
    req.opId = nextOpId();
    m_session->execute(req);
    emit statusMessage(QStringLiteral("执行获取任务数 (Q_TASK)"));
}

void KeyManageController::onDeleteClicked()
{
    const KeySessionSnapshot snapshot = m_session->snapshot();
    if (!snapshot.connected) {
        emit statusMessage(QStringLiteral("串口未连接，无法删除票"));
        return;
    }

    m_pendingDeletedSystemTicketId.clear();
    m_pendingDeletedKeyTaskRaw.clear();
    m_pendingDeleteAllowsRetransfer = false;
    m_pendingDeleteOrigin = DeleteOrigin::None;

    if (m_selectedKeyTaskRaw.isEmpty()) {
        emit statusMessage(QStringLiteral("未选中钥匙票，无法删除，请先读取钥匙票列表并选中一条任务"));
        return;
    }

    m_pendingDeletedKeyTaskRaw = m_selectedKeyTaskRaw;
    m_pendingDeletedSystemTicketId = taskIdFromRaw(m_pendingDeletedKeyTaskRaw);
    const SystemTicketDto mappedTicket = m_ticketStore->ticketById(m_pendingDeletedSystemTicketId);
    m_pendingDeleteAllowsRetransfer = mappedTicket.valid
            && (mappedTicket.returnState.isEmpty() || mappedTicket.returnState == QLatin1String("idle"));
    m_pendingDeleteSessionId = m_keySessionId;
    m_pendingDeleteRetryCount = 0;
    m_pendingDeleteOrigin = DeleteOrigin::ManualDelete;

    CommandRequest req;
    req.id = CommandId::DeleteTask;
    req.opId = nextOpId();
    req.payload = m_pendingDeletedKeyTaskRaw;
    m_session->execute(req);
    emit statusMessage(QStringLiteral("执行删除票 (DEL)"));
}

void KeyManageController::onTransferSelectedTicket()
{
    if (m_selectedSystemTicketId.trimmed().isEmpty()) {
        emit statusMessage(QStringLiteral("未选中系统票，无法发送传票"));
        return;
    }

    tryStartTicketTransfer(m_selectedSystemTicketId, false);
}

void KeyManageController::onReturnSelectedTicket()
{
    if (m_selectedSystemTicketId.trimmed().isEmpty()) {
        emit statusMessage(QStringLiteral("未选中系统票，无法执行回传"));
        return;
    }

    tryStartTicketReturn(m_selectedSystemTicketId, false);
}

void KeyManageController::onGetSystemTicketListClicked()
{
    qInfo().noquote()
            << QStringLiteral("[KeyManageController] onGetSystemTicketListClicked count=%1 selectedTaskId=%2")
                  .arg(QString::number(systemTickets().size()),
                       m_selectedSystemTicketId.isEmpty() ? QStringLiteral("<none>") : m_selectedSystemTicketId);
    emit systemTicketsUpdated(systemTickets());
}

void KeyManageController::onSystemTicketSelected(int row)
{
    const QList<SystemTicketDto> tickets = systemTickets();
    if (row < 0 || row >= tickets.size()) {
        m_selectedSystemTicketId.clear();
        emit selectedSystemTicketChanged(SystemTicketDto{});
        return;
    }

    m_selectedSystemTicketId = tickets.at(row).taskId;
    emit selectedSystemTicketChanged(tickets.at(row));
}

void KeyManageController::onKeyTaskSelected(int row)
{
    if (row < 0 || row >= m_lastKeyTasks.size()) {
        m_selectedKeyTaskRaw.clear();
        return;
    }

    m_selectedKeyTaskRaw = m_lastKeyTasks.at(row).taskId;
}

void KeyManageController::onExpertModeChanged(bool enabled)
{
    m_logManager->setExpertMode(enabled);
    emit logTableRefreshRequested();
}

void KeyManageController::onShowHexChanged(bool enabled)
{
    m_logManager->setShowHex(enabled);
}

void KeyManageController::onClearLogsClicked()
{
    m_logManager->clear();
    emit logsCleared();
    emit statusMessage(QStringLiteral("串口日志已清空"));
}

void KeyManageController::onClearHttpClientLogsClicked()
{
    m_httpClientLogLines.clear();
    emit statusMessage(QStringLiteral("HTTP客户端报文已清空"));
}

void KeyManageController::onClearHttpServerLogsClicked()
{
    m_httpServerLogLines.clear();
    emit statusMessage(QStringLiteral("HTTP服务端报文已清空"));
}

bool KeyManageController::exportLogs(const QString &path, QString *error) const
{
    return m_logManager->exportText(path, error);
}

QList<LogItem> KeyManageController::visibleLogs() const
{
    return m_logManager->filteredItems();
}

bool KeyManageController::shouldDisplay(const LogItem &item) const
{
    return m_logManager->shouldDisplay(item);
}

KeySessionSnapshot KeyManageController::currentSnapshot() const
{
    return m_session->snapshot();
}

QList<SystemTicketDto> KeyManageController::systemTickets() const
{
    return m_ticketStore->tickets();
}

QString KeyManageController::httpServerLogText() const
{
    return m_httpServerLogLines.join("\n");
}

QString KeyManageController::httpClientLogText() const
{
    return m_httpClientLogLines.join("\n");
}

QVariantMap KeyManageController::keySerialApplyStatus() const
{
    const KeySessionSnapshot snapshot = m_session->snapshot();
    const QString configuredPort = ConfigManager::instance()->keySerialPort().trimmed();
    QString busyReason;
    QVariantMap status;
    status.insert(QStringLiteral("configuredPort"), configuredPort);
    status.insert(QStringLiteral("runtimePort"), snapshot.portName.trimmed());
    status.insert(QStringLiteral("verifiedPort"), snapshot.verifiedPortName.trimmed());
    status.insert(QStringLiteral("state"), m_keySerialApplyState);
    status.insert(QStringLiteral("message"), m_keySerialApplyMessage);
    status.insert(QStringLiteral("canApply"),
                  !m_keyPortApplyInProgress
                  && !configuredPort.isEmpty()
                  && configuredPort != snapshot.portName.trimmed()
                  && !isKeyPortApplyBusy(&busyReason));
    status.insert(QStringLiteral("busyReason"), busyReason);
    return status;
}

bool KeyManageController::canStartManualReturn(const QString &taskId, QString *blockedReason) const
{
    return canStartTicketReturn(taskId, false, nullptr, blockedReason);
}

bool KeyManageController::isSelectedKeyTaskOrphan() const
{
    if (m_selectedKeyTaskRaw.isEmpty()) {
        return false;
    }
    const QString taskId = taskIdFromRaw(m_selectedKeyTaskRaw);
    if (taskId.isEmpty()) {
        return false;
    }
    const SystemTicketDto ticket = m_ticketStore->ticketById(taskId);
    return ticket.valid && ticket.transferState == QLatin1String("orphan-recovered");
}

void KeyManageController::onClearOrphanKeyTaskClicked()
{
    if (m_selectedKeyTaskRaw.isEmpty()) {
        emit statusMessage(QStringLiteral("未选中钥匙任务，请先读取钥匙票列表并选中未登记任务"));
        return;
    }

    const QString taskId = taskIdFromRaw(m_selectedKeyTaskRaw);
    const SystemTicketDto ticket = m_ticketStore->ticketById(taskId);
    if (!ticket.valid || ticket.transferState != QLatin1String("orphan-recovered")) {
        emit statusMessage(QStringLiteral("选中任务不是未登记孤儿任务，不允许使用此操作清除"));
        return;
    }

    const KeySessionSnapshot snapshot = m_session->snapshot();
    if (!snapshot.connected || !snapshot.keyPresent || !snapshot.keyStable || !snapshot.sessionReady) {
        emit statusMessage(QStringLiteral("钥匙未就绪，无法执行未登记任务清除"));
        return;
    }

    if (!m_pendingDeletedSystemTicketId.isEmpty()) {
        emit statusMessage(QStringLiteral("当前已有删除操作在途，请稍后再试"));
        return;
    }

    if (!m_activeReturnTaskId.isEmpty() || !m_pendingReturnHandshakeTaskId.isEmpty()) {
        emit statusMessage(QStringLiteral("当前有回传操作在途，无法同时执行未登记任务清除"));
        return;
    }

    if (!m_activeTransferTaskId.isEmpty()) {
        emit statusMessage(QStringLiteral("当前有传票操作在途，无法同时执行未登记任务清除"));
        return;
    }

    if (!m_pendingCancelSystemTicketId.isEmpty()) {
        emit statusMessage(QStringLiteral("当前有撤销操作在途，无法同时执行未登记任务清除"));
        return;
    }

    m_pendingDeletedKeyTaskRaw = m_selectedKeyTaskRaw;
    m_pendingDeletedSystemTicketId = taskId;
    m_pendingDeleteAllowsRetransfer = false;
    m_pendingDeleteSessionId = m_keySessionId;
    m_pendingDeleteRetryCount = 0;
    m_pendingDeleteOrigin = DeleteOrigin::AdminClearOrphan;

    CommandRequest req;
    req.id = CommandId::DeleteTask;
    req.opId = nextOpId();
    req.payload = m_pendingDeletedKeyTaskRaw;
    m_session->execute(req);
    qInfo().noquote()
            << QStringLiteral("[KeyManageController] onClearOrphanKeyTaskClicked taskId=%1 session=%2")
                  .arg(taskId, QString::number(m_keySessionId));
    emit statusMessage(QStringLiteral("发送 DEL 命令，清除未登记任务：%1").arg(taskId));
}

bool KeyManageController::isSelectedSystemTicketForceClearable() const
{
    if (m_selectedSystemTicketId.isEmpty()) {
        return false;
    }
    const SystemTicketDto ticket = m_ticketStore->ticketById(m_selectedSystemTicketId);
    if (!ticket.valid) {
        return false;
    }
    // 不允许对孤儿占位票使用强制清理（孤儿票由子步骤 3 处理）
    if (ticket.transferState == QLatin1String("orphan-recovered")) {
        return false;
    }
    // 允许强制清理的失败态：
    // 1. transferState == "failed" — 传票失败，链路已终止，钥匙里无此任务
    // 2. TerminationFailed — DEL 已确认成功，通知工作台失败，钥匙侧已干净
    // 3. return-delete-failed — 回传删除验证达上限，链路已终止（钥匙任务可能仍存在）
    // 4. manual-required — 回传上传失败或缺少 raw ID，链路已终止（钥匙任务可能仍存在）
    const bool isTransferFailed =
            (ticket.transferState == QLatin1String("failed"));
    const bool isTerminationFailed =
            (ticket.adminDeleteStage == SystemTicketDto::AdminDeleteStage::TerminationFailed);
    const bool isReturnDeleteFailed =
            (ticket.returnState == QLatin1String("return-delete-failed"));
    const bool isManualRequired =
            (ticket.returnState == QLatin1String("manual-required"));
    return isTransferFailed || isTerminationFailed || isReturnDeleteFailed || isManualRequired;
}

void KeyManageController::onForceCleanFailedTicketClicked()
{
    if (m_selectedSystemTicketId.isEmpty()) {
        emit statusMessage(QStringLiteral("未选中系统票，请先在系统票列表中选中需要强制清理的记录"));
        return;
    }

    const SystemTicketDto ticket = m_ticketStore->ticketById(m_selectedSystemTicketId);
    if (!ticket.valid) {
        emit statusMessage(QStringLiteral("选中的系统票记录不存在"));
        return;
    }

    if (ticket.transferState == QLatin1String("orphan-recovered")) {
        emit statusMessage(QStringLiteral("未登记孤儿任务请使用\"清除未登记任务\"操作，不适用强制清理"));
        return;
    }

    if (!isSelectedSystemTicketForceClearable()) {
        emit statusMessage(QStringLiteral("当前选中的系统票状态不满足强制清理条件"));
        return;
    }

    // in-flight 守卫：避免在业务链路进行中删除记录导致状态悬空
    const QString taskId = m_selectedSystemTicketId;
    if (taskId == m_pendingDeletedSystemTicketId) {
        emit statusMessage(QStringLiteral("当前有删除操作在途，请稍后再试"));
        return;
    }
    if (taskId == m_activeReturnTaskId || taskId == m_pendingReturnHandshakeTaskId) {
        emit statusMessage(QStringLiteral("当前有回传操作在途，无法同时执行强制清理"));
        return;
    }
    if (taskId == m_activeTransferTaskId || taskId == m_pendingTransferHandshakeTaskId) {
        emit statusMessage(QStringLiteral("当前有传票操作在途，无法同时执行强制清理"));
        return;
    }
    if (m_httpTransferTransactionActive && taskId == m_httpTransferTransactionTaskId) {
        emit statusMessage(QStringLiteral("当前工作台传票事务未完成，无法同时执行强制清理"));
        return;
    }
    if (taskId == m_pendingCancelSystemTicketId) {
        emit statusMessage(QStringLiteral("当前有撤销操作在途，无法同时执行强制清理"));
        return;
    }

    m_ticketStore->removeTicket(taskId);
    qInfo().noquote()
            << QStringLiteral("[KeyManageController] onForceCleanFailedTicketClicked: force-removed failed system ticket taskId=%1 transferState=%2 returnState=%3 adminDeleteStage=%4")
                  .arg(taskId, ticket.transferState, ticket.returnState,
                       QString::number(static_cast<int>(ticket.adminDeleteStage)));
    emit statusMessage(QStringLiteral("已强制清理失败系统票：%1").arg(taskId));
}


void KeyManageController::requestApplyConfiguredKeyPort()
{
    const QString configuredPort = ConfigManager::instance()->keySerialPort().trimmed();
    if (configuredPort.isEmpty()) {
        m_keySerialApplyState = QStringLiteral("apply_failed");
        m_keySerialApplyMessage = QStringLiteral("当前未配置钥匙串口，无法应用到运行态");
        emit statusMessage(m_keySerialApplyMessage);
        refreshKeySerialApplyStatus();
        return;
    }

    const KeySessionSnapshot snapshot = m_session->snapshot();
    if (snapshot.portName.trimmed() == configuredPort
            && snapshot.connected
            && (!snapshot.keyPresent || (snapshot.sessionReady && snapshot.protocolHealthy))) {
        m_keySerialApplyState = QStringLiteral("applied");
        m_keySerialApplyMessage = QStringLiteral("运行态已使用当前配置串口，无需重新应用");
        emit statusMessage(m_keySerialApplyMessage);
        refreshKeySerialApplyStatus();
        return;
    }

    QString busyReason;
    if (isKeyPortApplyBusy(&busyReason)) {
        m_keySerialApplyState = QStringLiteral("saved_not_applied");
        m_keySerialApplyMessage = busyReason;
        emit statusMessage(busyReason);
        refreshKeySerialApplyStatus();
        return;
    }

    const int cfgBaud = ConfigManager::instance()->baudRate();
    const int baud = (cfgBaud > 0) ? cfgBaud : 115200;

    m_keyPortApplyInProgress = true;
    m_suppressSessionAutomation = true;
    ++m_keyPortApplyEpoch;
    m_keyPortApplyTargetPort = configuredPort;
    m_keySerialApplyState = QStringLiteral("applying");
    m_keySerialApplyMessage = QStringLiteral("正在切换钥匙串口到 %1，并等待新运行态完成通讯接管").arg(configuredPort);
    emit statusMessage(m_keySerialApplyMessage);
    refreshKeySerialApplyStatus();

    invalidateCurrentKeySession(QStringLiteral("应用新钥匙串口配置"));
    m_session->setPortSwitchInProgress(true);
    m_session->disconnectPort();
    m_session->clearVerifiedPort();

    const bool connected = m_session->connectPort(configuredPort, baud);

    if (connected) {
        emit sessionSnapshotChanged(m_session->snapshot());
        emit statusMessage(QStringLiteral("新钥匙串口 %1 已打开，等待通讯确认后完成运行态切换").arg(configuredPort));
        refreshKeySerialApplyStatus();
        return;
    }

    m_session->setPortSwitchInProgress(false);
    m_keyPortApplyInProgress = false;
    m_suppressSessionAutomation = false;
    m_keyPortApplyTargetPort.clear();
    m_keySerialApplyState = QStringLiteral("apply_failed");
    m_keySerialApplyMessage = QStringLiteral("配置已保存，但新钥匙串口 %1 未成功应用到运行态").arg(configuredPort);
    emit sessionSnapshotChanged(m_session->snapshot());
    emit statusMessage(m_keySerialApplyMessage);
    refreshKeySerialApplyStatus();
}

void KeyManageController::handleSessionEvent(const KeySessionEvent &event)
{
    const int eventBridgeEpoch = event.data.value(QStringLiteral("bridgeEpoch")).toInt();
    if (!m_keyPortApplyInProgress
            && m_acceptedBridgeEpoch > 0
            && eventBridgeEpoch > 0
            && eventBridgeEpoch != m_acceptedBridgeEpoch) {
        switch (event.kind) {
        case KeySessionEvent::TasksUpdated:
        case KeySessionEvent::TaskLogReady:
        case KeySessionEvent::Ack:
        case KeySessionEvent::Nak:
        case KeySessionEvent::Timeout:
        case KeySessionEvent::KeyStateChanged:
        case KeySessionEvent::TicketTransferProgress:
        case KeySessionEvent::TicketTransferFinished:
        case KeySessionEvent::TicketTransferFailed:
            qInfo().noquote()
                    << QStringLiteral("[KeyManageController] ignore stale bridge-epoch event kind=%1 eventEpoch=%2 acceptedEpoch=%3")
                          .arg(QString::number(static_cast<int>(event.kind)),
                               QString::number(eventBridgeEpoch),
                               QString::number(m_acceptedBridgeEpoch));
            return;
        default:
            break;
        }
    }

    switch (event.kind) {
    case KeySessionEvent::Notice:
        emit statusMessage(event.data.value("what").toString());
        break;
    case KeySessionEvent::Timeout: {
        const QString what = event.data.value("what").toString();
        // 超时也要释放 auto-query 窗口，否则 Q_TASK 超时后 m_autoQuerySessionId 永久卡住
        if (m_autoQuerySessionId == m_keySessionId && m_keySessionId > 0) {
            m_autoQuerySessionId = 0;
        }
        if (what.contains(QStringLiteral("DEL"), Qt::CaseInsensitive)) {
            if (!m_pendingDeletedSystemTicketId.isEmpty()) {
                if (m_pendingDeleteOrigin == DeleteOrigin::AdminClearOrphan) {
                    // AdminClearOrphan：DEL 超时即视为本轮操作中断，不自动续删，保留孤儿记录
                    const QString taskId = m_pendingDeletedSystemTicketId;
                    m_pendingDeletedSystemTicketId.clear();
                    m_pendingDeletedKeyTaskRaw.clear();
                    m_pendingDeleteAllowsRetransfer = false;
                    m_pendingDeleteSessionId = 0;
                    m_pendingDeleteRetryCount = 0;
                    m_pendingDeleteOrigin = DeleteOrigin::None;
                    emit statusMessage(QStringLiteral("未登记任务清理未完成（DEL 超时），请确认钥匙状态后再次点击清除：%1").arg(taskId));
                } else {
                    m_ticketStore->updateReturnState(m_pendingDeletedSystemTicketId,
                                                     QStringLiteral("return-delete-verifying"),
                                                     QStringLiteral("DEL 超时，等待后续 Q_TASK 对账确认钥匙任务是否已删除"));
                    m_pendingDeleteSessionId = 0;
                    emit statusMessage(QStringLiteral("DEL 超时，保留删除验证意图，等待后续 Q_TASK 继续对账"));
                }
            }
        }
        if (!m_pendingCancelSystemTicketId.isEmpty()
                && what.contains(QStringLiteral("DEL"), Qt::CaseInsensitive)) {
            const QString taskId = m_pendingCancelSystemTicketId;
            const SystemTicketDto ticket = m_ticketStore->ticketById(taskId);
            if (ticket.valid) {
                m_ticketStore->updateCancelTracking(taskId,
                                                    QStringLiteral("cancel-pending"),
                                                    QStringLiteral("workbench-http"),
                                                    ticket.residentSlotId.isEmpty() ? QStringLiteral("slot-1") : ticket.residentSlotId,
                                                    ticket.residentKeyId,
                                                    ticket.cancelRequestedAt.isValid() ? ticket.cancelRequestedAt : QDateTime::currentDateTime(),
                                                    true,
                                                    QStringLiteral("DEL 超时，等待后续 Q_TASK 对账确认任务是否已删除"));
                m_ticketCancelCoordinator->recordAccepted(taskId,
                                                         m_ticketStore->ticketById(taskId),
                                                         QStringLiteral("cancel-pending"),
                                                         QStringLiteral("DEL 超时，等待后续 Q_TASK 对账确认任务是否已删除"));
            }
            m_pendingCancelSessionId = 0;
            schedulePendingCancelReconcile(QStringLiteral("cancel-del-timeout"), 800);
        }
        if (!m_pendingTransferHandshakeTaskId.isEmpty()
                && what.contains(QStringLiteral("SET_COM"), Qt::CaseInsensitive)) {
            const QString taskId = m_pendingTransferHandshakeTaskId;
            m_pendingTransferHandshakeTaskId.clear();
            m_pendingTransferHandshakeJsonBytes.clear();
            m_pendingTransferHandshakeDebugChunk = 0;
            m_ticketStore->updateTransferState(taskId, QStringLiteral("failed"),
                                               QStringLiteral("传票前握手超时"));
            m_activeTransferTaskId.clear();
            m_activeTransferSessionId = 0;
            if (m_httpTransferTransactionActive) {
                clearHttpTransferTransaction(false);
            }
        }
        if ((!m_activeReturnTaskId.isEmpty() || !m_pendingReturnHandshakeTaskId.isEmpty())
                && (what.contains(QStringLiteral("SET_COM"), Qt::CaseInsensitive)
                    || what.contains(QStringLiteral("I_TASK_LOG"), Qt::CaseInsensitive)
                    || what.contains(QStringLiteral("0X05"), Qt::CaseInsensitive)
                    || what.contains(QStringLiteral("UP_TASK_LOG"), Qt::CaseInsensitive)
                    || what.contains(QStringLiteral("0X15"), Qt::CaseInsensitive))) {
            failActiveReturnHandshake(what);
        }
        emit timeoutOccurred(what);
        emit statusMessage(what);
        break;
    }
    case KeySessionEvent::Ack:
        if (static_cast<quint8>(event.data.value("ackedCmd").toInt()) == KeyProtocol::CmdSetCom
                && !m_pendingTransferHandshakeTaskId.isEmpty()
                && !m_pendingTransferHandshakeJsonBytes.isEmpty()
                && isCurrentKeySession(m_activeTransferSessionId)) {
            const QString taskId = m_pendingTransferHandshakeTaskId;
            const QByteArray jsonBytes = m_pendingTransferHandshakeJsonBytes;
            const int debugChunk = m_pendingTransferHandshakeDebugChunk;
            m_pendingTransferHandshakeTaskId.clear();
            m_pendingTransferHandshakeJsonBytes.clear();
            m_pendingTransferHandshakeDebugChunk = 0;
            TicketTransferRequest request;
            request.jsonBytes = jsonBytes;
            request.opId = nextOpId();
            request.debugFrameChunkSize = debugChunk;
            m_session->transferTicket(request);
            emit statusMessage(QStringLiteral("传票前握手已确认，开始发送 TICKET 帧：%1").arg(taskId));
        }
        if (static_cast<quint8>(event.data.value("ackedCmd").toInt()) == KeyProtocol::CmdSetCom
                && !m_pendingReturnHandshakeTaskId.isEmpty()
                && !m_pendingReturnHandshakeTaskIdRaw.isEmpty()
                && isCurrentKeySession(m_activeReturnSessionId)) {
            m_ticketStore->updateReturnState(m_pendingReturnHandshakeTaskId,
                                             QStringLiteral("return-log-requesting"));
            CommandRequest req;
            req.id = CommandId::QueryTaskLog;
            req.opId = nextOpId();
            req.payload = m_pendingReturnHandshakeTaskIdRaw;
            m_session->execute(req);
            emit statusMessage(QStringLiteral("回传前握手已确认，开始读取钥匙回传日志：%1")
                                   .arg(m_pendingReturnHandshakeTaskId));
            m_pendingReturnHandshakeTaskId.clear();
            m_pendingReturnHandshakeTaskIdRaw.clear();
        }
        if (static_cast<quint8>(event.data.value("ackedCmd").toInt()) == KeyProtocol::CmdITaskLog
                && !m_activeReturnTaskId.isEmpty()
                && isCurrentKeySession(m_activeReturnSessionId)) {
            m_ticketStore->updateReturnState(m_activeReturnTaskId,
                                             QStringLiteral("return-log-receiving"));
        }
        if (static_cast<quint8>(event.data.value("ackedCmd").toInt()) == KeyProtocol::CmdDel
                && !m_pendingCancelSystemTicketId.isEmpty()) {
            schedulePendingCancelReconcile(QStringLiteral("cancel-del-ack"), 300);
        }
        emit ackReceived(static_cast<quint8>(event.data.value("ackedCmd").toInt()));
        break;
    case KeySessionEvent::Nak:
        // NAK 也释放 auto-query 窗口（Q_TASK 被拒绝时）
        if (m_autoQuerySessionId == m_keySessionId && m_keySessionId > 0) {
            m_autoQuerySessionId = 0;
        }
        if (static_cast<quint8>(event.data.value("origCmd").toInt()) == KeyProtocol::CmdDel) {
            if (!m_pendingDeletedSystemTicketId.isEmpty()) {
                if (m_pendingDeleteOrigin == DeleteOrigin::AdminClearOrphan) {
                    // AdminClearOrphan：DEL NAK 即视为本轮操作中断，不自动续删，保留孤儿记录
                    const QString taskId = m_pendingDeletedSystemTicketId;
                    m_pendingDeletedSystemTicketId.clear();
                    m_pendingDeletedKeyTaskRaw.clear();
                    m_pendingDeleteAllowsRetransfer = false;
                    m_pendingDeleteSessionId = 0;
                    m_pendingDeleteRetryCount = 0;
                    m_pendingDeleteOrigin = DeleteOrigin::None;
                    emit statusMessage(QStringLiteral("未登记任务清理未完成（DEL 被拒绝），请确认钥匙状态后再次点击清除：%1").arg(taskId));
                } else {
                    m_ticketStore->updateReturnState(m_pendingDeletedSystemTicketId,
                                                     QStringLiteral("return-delete-verifying"),
                                                     QStringLiteral("DEL 收到 NAK，等待后续 Q_TASK 对账确认钥匙任务状态"));
                    m_pendingDeleteSessionId = 0;
                }
            }
        }
        if (!m_pendingCancelSystemTicketId.isEmpty()
                && static_cast<quint8>(event.data.value("origCmd").toInt()) == KeyProtocol::CmdDel) {
            const QString taskId = m_pendingCancelSystemTicketId;
            const SystemTicketDto ticket = m_ticketStore->ticketById(taskId);
            if (ticket.valid) {
                m_ticketStore->updateCancelTracking(taskId,
                                                    QStringLiteral("cancel-pending"),
                                                    QStringLiteral("workbench-http"),
                                                    ticket.residentSlotId.isEmpty() ? QStringLiteral("slot-1") : ticket.residentSlotId,
                                                    ticket.residentKeyId,
                                                    ticket.cancelRequestedAt.isValid() ? ticket.cancelRequestedAt : QDateTime::currentDateTime(),
                                                    true,
                                                    QStringLiteral("DEL 收到 NAK，等待后续 Q_TASK 对账确认钥匙任务状态"));
                m_ticketCancelCoordinator->recordAccepted(taskId,
                                                         m_ticketStore->ticketById(taskId),
                                                         QStringLiteral("cancel-pending"),
                                                         QStringLiteral("DEL 收到 NAK，等待后续 Q_TASK 对账确认钥匙任务状态"));
            }
            m_pendingCancelSessionId = 0;
            schedulePendingCancelReconcile(QStringLiteral("cancel-del-nak"), 800);
        }
        if (!m_pendingTransferHandshakeTaskId.isEmpty()
                && static_cast<quint8>(event.data.value("origCmd").toInt()) == KeyProtocol::CmdSetCom) {
            const QString taskId = m_pendingTransferHandshakeTaskId;
            m_pendingTransferHandshakeTaskId.clear();
            m_pendingTransferHandshakeJsonBytes.clear();
            m_pendingTransferHandshakeDebugChunk = 0;
            m_ticketStore->updateTransferState(taskId, QStringLiteral("failed"),
                                               QStringLiteral("传票前握手被钥匙拒绝(NAK)"));
            m_activeTransferTaskId.clear();
            m_activeTransferSessionId = 0;
            if (m_httpTransferTransactionActive) {
                clearHttpTransferTransaction(false);
            }
            emit statusMessage(QStringLiteral("传票前握手被钥匙拒绝：%1").arg(taskId));
        }
        if (!m_activeReturnTaskId.isEmpty() || !m_pendingReturnHandshakeTaskId.isEmpty()) {
            const quint8 origCmd = static_cast<quint8>(event.data.value("origCmd").toInt());
            if (origCmd == KeyProtocol::CmdSetCom
                    || origCmd == KeyProtocol::CmdITaskLog
                    || origCmd == KeyProtocol::CmdUpTaskLog) {
                const QString reason = QStringLiteral("收到 NAK，origCmd=0x%1")
                        .arg(origCmd, 2, 16, QLatin1Char('0'))
                        .toUpper();
                failActiveReturnHandshake(reason);
            }
        }
        emit nakReceived(static_cast<quint8>(event.data.value("origCmd").toInt()),
                         static_cast<quint8>(event.data.value("errCode").toInt()));
        break;
    case KeySessionEvent::TasksUpdated: {
        if (m_suppressSessionAutomation) {
            qInfo().noquote()
                    << QStringLiteral("[KeyManageController] ignore TasksUpdated during controlled key-port apply epoch=%1")
                          .arg(QString::number(m_keyPortApplyEpoch));
            break;
        }
        const QList<KeyTaskDto> tasks = toTaskDtos(event.data.value("tasks").toList());
        const KeySessionSnapshot snapshot = m_session->snapshot();
        const bool ready = isSessionReadySnapshot(snapshot);
        const bool httpTransactionControlledEvent =
                isActiveHttpTransferTransactionFor(m_httpTransferTransactionSessionId, eventBridgeEpoch);
        m_lastKeyTasks = tasks;
        if (httpTransactionControlledEvent) {
            qInfo().noquote()
                    << QStringLiteral("[KeyManageController] TasksUpdated captured for active http transfer transaction taskId=%1 session=%2 epoch=%3; suppress auto side-effects")
                          .arg(m_httpTransferTransactionTaskId.isEmpty()
                                       ? QStringLiteral("<none>")
                                       : m_httpTransferTransactionTaskId,
                               QString::number(m_httpTransferTransactionSessionId),
                               QString::number(m_httpTransferTransactionBridgeEpoch));
        } else if (ready) {
            reconcileSystemTicketsFromKeyTasks(tasks);
            // 主动孤儿识别：钥匙中有任务但本地票池无记录 → 建孤儿恢复票
            for (const KeyTaskDto &task : tasks) {
                const QString orphanTaskId = taskIdFromRaw(task.taskId);
                if (!m_ticketStore->ticketById(orphanTaskId).valid) {
                    m_ticketStore->ingestOrphanTicket(orphanTaskId);
                    qInfo().noquote()
                            << QStringLiteral("[KeyManageController] orphan device task detected and recovered taskId=%1")
                                  .arg(orphanTaskId);
                }
            }
            recoverAutoTransferWhenKeyEmpty(tasks);
            tryAutoCleanupReturnedTicket(tasks);
        } else {
            qInfo().noquote()
                    << QStringLiteral("[KeyManageController] TasksUpdated arrived while session not ready, skip auto actions currentSession=%1")
                          .arg(QString::number(m_keySessionId));
        }
        bool selectedStillExists = false;
        for (const KeyTaskDto &task : tasks) {
            if (task.taskId == m_selectedKeyTaskRaw) {
                selectedStillExists = true;
                break;
            }
        }
        if (!selectedStillExists) {
            m_selectedKeyTaskRaw.clear();
        }
        emit tasksUpdated(tasks);
        emit statusMessage(QStringLiteral("Q_TASK 完成，任务数=%1").arg(tasks.size()));
        // Q_TASK 完成后释放 auto-query 窗口，允许人工传票通过 ingress 门禁
        if (m_autoQuerySessionId == m_keySessionId && m_keySessionId > 0) {
            m_autoQuerySessionId = 0;
        }
        if (!httpTransactionControlledEvent && ready && !m_pendingDeletedSystemTicketId.isEmpty()) {
            finalizePendingReturnDelete(tasks);
        }
        if (!httpTransactionControlledEvent && ready) {
            reconcileStaleOrphanTickets(tasks);
        }
        if (!httpTransactionControlledEvent && ready && !m_pendingCancelSystemTicketId.isEmpty()) {
            finalizePendingCancelDelete(tasks);
        }
        if (!httpTransactionControlledEvent && ready) {
            tryProcessPendingCancelTasks(tasks);
        }
        if (!httpTransactionControlledEvent && ready) {
            tryAutoReturnCompletedTicket(tasks);
        }
        if (!m_selectedSystemTicketId.isEmpty()) {
            emit selectedSystemTicketChanged(m_ticketStore->ticketById(m_selectedSystemTicketId));
        }
        if (!httpTransactionControlledEvent && ready && (!tasks.isEmpty() || m_activeTransferTaskId.isEmpty())) {
            tryAutoTransferPendingTicket();
        }
        // A座所有业务处理完成后，若无命令在途，触发 A座自动维护链（电量+校时）
        if (!httpTransactionControlledEvent && ready) {
            tryAutoSeat1MaintenanceAfterTasksUpdated(snapshot, tasks);
        }
        break;
    }
    case KeySessionEvent::AuxCommandFinished:
        // 辅助命令完成，UI 状态由 snapshot 驱动，无需额外处理
        break;
    case KeySessionEvent::TaskLogReady: {
        if (m_suppressSessionAutomation) {
            qInfo().noquote()
                    << QStringLiteral("[KeyManageController] ignore TaskLogReady during controlled key-port apply epoch=%1")
                          .arg(QString::number(m_keyPortApplyEpoch));
            break;
        }
        if (m_activeReturnTaskId.isEmpty() || !isCurrentKeySession(m_activeReturnSessionId)) {
            emit statusMessage(QStringLiteral("收到任务回传日志，但当前无活动回传任务"));
            break;
        }

        QJsonObject payload;
        payload.insert("stationNo", event.data.value("stationNo").toInt());
        payload.insert("steps", event.data.value("steps").toInt());
        payload.insert("taskId", event.data.value("taskId").toString());

        QJsonArray items;
        const QVariantList rawItems = event.data.value("items").toList();
        for (const QVariant &itemVar : rawItems) {
            items.append(QJsonObject::fromVariantMap(itemVar.toMap()));
        }
        payload.insert("taskLogItems", items);

        m_ticketStore->updateReturnState(m_activeReturnTaskId, QStringLiteral("return-uploading"));
        m_ticketReturnClient->uploadReturnPayload(payload);
        emit statusMessage(QStringLiteral("已解析钥匙回传日志，准备上传到服务端"));
        break;
    }
    case KeySessionEvent::TicketTransferProgress: {
        if (m_suppressSessionAutomation) {
            qInfo().noquote()
                    << QStringLiteral("[KeyManageController] ignore TicketTransferProgress during controlled key-port apply epoch=%1")
                          .arg(QString::number(m_keyPortApplyEpoch));
            break;
        }
        if (m_activeTransferTaskId.isEmpty() || !isCurrentKeySession(m_activeTransferSessionId)) {
            qInfo().noquote()
                    << QStringLiteral("[KeyManageController] ignore stale TicketTransferProgress activeTransfer=%1 activeTransferSession=%2 currentSession=%3")
                          .arg(m_activeTransferTaskId.isEmpty() ? QStringLiteral("<none>") : m_activeTransferTaskId,
                               QString::number(m_activeTransferSessionId),
                               QString::number(m_keySessionId));
            break;
        }
        const int frameIndex = event.data.value("frameIndex").toInt();
        const int totalFrames = event.data.value("totalFrames").toInt();
        if (!m_activeTransferTaskId.isEmpty()) {
            m_ticketStore->updateTransferState(m_activeTransferTaskId, QStringLiteral("sending"));
        }
        emit statusMessage(QStringLiteral("传票发送中：第 %1/%2 帧").arg(frameIndex).arg(totalFrames));
        break;
    }
    case KeySessionEvent::TicketTransferFinished: {
        if (m_suppressSessionAutomation) {
            qInfo().noquote()
                    << QStringLiteral("[KeyManageController] ignore TicketTransferFinished during controlled key-port apply epoch=%1")
                          .arg(QString::number(m_keyPortApplyEpoch));
            break;
        }
        if (m_activeTransferTaskId.isEmpty() || !isCurrentKeySession(m_activeTransferSessionId)) {
            qInfo().noquote()
                << QStringLiteral("[KeyManageController] ignore stale TicketTransferFinished activeTransfer=%1 activeTransferSession=%2 currentSession=%3")
                          .arg(m_activeTransferTaskId.isEmpty() ? QStringLiteral("<none>") : m_activeTransferTaskId,
                               QString::number(m_activeTransferSessionId),
                               QString::number(m_keySessionId));
            break;
        }
        const QString finishedTaskId = m_activeTransferTaskId;
        const int finishedSessionId = m_activeTransferSessionId;
        const bool trackedTicket = m_ticketStore->ticketById(finishedTaskId).valid;
        const bool wasHttpTransfer = isActiveHttpTransferTransactionFor(finishedSessionId, eventBridgeEpoch);
        if (trackedTicket) {
            m_ticketStore->updateTransferState(finishedTaskId, QStringLiteral("success"));
        }
        m_activeTransferTaskId.clear();
        m_activeTransferSessionId = 0;
        if (wasHttpTransfer) {
            clearHttpTransferTransaction(false);
        }
        emit statusMessage(QStringLiteral("传票发送完成：%1").arg(finishedTaskId));
        // 传票完成后主动触发一次 Q_TASK 对账，确认任务已写入钥匙并更新 m_lastKeyTasks
        // 这样下次 success 状态检查有新鲜数据，不依赖下次 ready-edge 才能对账
        if (trackedTicket && isCurrentKeySession(finishedSessionId)) {
            onQueryTasksClicked();
        } else {
            tryAutoTransferPendingTicket();
        }
        schedulePendingCancelReconcile(QStringLiteral("transfer-finished"), 150);
        break;
    }
    case KeySessionEvent::TicketTransferFailed: {
        if (m_suppressSessionAutomation) {
            qInfo().noquote()
                    << QStringLiteral("[KeyManageController] ignore TicketTransferFailed during controlled key-port apply epoch=%1")
                          .arg(QString::number(m_keyPortApplyEpoch));
            break;
        }
        const QString what = event.data.value("what").toString();
        if (m_activeTransferTaskId.isEmpty() || !isCurrentKeySession(m_activeTransferSessionId)) {
            qInfo().noquote()
                    << QStringLiteral("[KeyManageController] ignore stale TicketTransferFailed reason=%1 activeTransfer=%2 activeTransferSession=%3 currentSession=%4")
                          .arg(what,
                               m_activeTransferTaskId.isEmpty() ? QStringLiteral("<none>") : m_activeTransferTaskId,
                               QString::number(m_activeTransferSessionId),
                               QString::number(m_keySessionId));
            break;
        }

        const QString failedTaskId = m_activeTransferTaskId;
        const int failedSessionId = m_activeTransferSessionId;
        const bool inflightBusy = what.contains(QStringLiteral("当前有命令在途"));
        const SystemTicketDto failedTicket = m_ticketStore->ticketById(failedTaskId);
        const bool trackedTicket = failedTicket.valid;
        const bool allowAutoPending = trackedTicket && isAutomaticTransferSource(failedTicket);
        const bool wasHttpTransfer = isActiveHttpTransferTransactionFor(failedSessionId, eventBridgeEpoch);
        if (inflightBusy && allowAutoPending) {
            m_ticketStore->updateTransferState(failedTaskId,
                                               QStringLiteral("auto-pending"),
                                               QStringLiteral("底层仍有命令在途，等待当前命令完成后自动重试"));
            m_activeTransferTaskId.clear();
            m_activeTransferSessionId = 0;
            if (wasHttpTransfer) {
                clearHttpTransferTransaction(false);
            }
            emit statusMessage(QStringLiteral("传票暂未发送：当前仍有命令在途，已回到自动等待队列：%1")
                                   .arg(failedTaskId));
            scheduleAutoTransferRetry(QStringLiteral("底层命令在途"));
            break;
        }
        if (inflightBusy && trackedTicket) {
            m_ticketStore->updateTransferState(failedTaskId,
                                               QStringLiteral("failed"),
                                               QStringLiteral("本次为人工触发，底层命令在途，未自动转回 auto-pending，请人工重试"));
            m_activeTransferTaskId.clear();
            m_activeTransferSessionId = 0;
            if (wasHttpTransfer) {
                clearHttpTransferTransaction(false);
            }
            m_ticketIngress->completeDeferredResponse(
                failedTaskId, false,
                QStringLiteral("传票失败：底层命令仍在途，请稍后重试"));
            emit statusMessage(QStringLiteral("人工传票失败：底层命令仍在途，系统不会自动补发，请手动重试：%1")
                                   .arg(failedTaskId));
            break;
        }

        if (!m_activeTransferTaskId.isEmpty()) {
            m_ticketStore->updateTransferState(m_activeTransferTaskId, QStringLiteral("failed"), what);
        }
        m_activeTransferTaskId.clear();
        m_activeTransferSessionId = 0;
        if (wasHttpTransfer) {
            clearHttpTransferTransaction(false);
        }
        m_ticketIngress->completeDeferredResponse(
            failedTaskId, false,
            QStringLiteral("传票失败：%1").arg(what.isEmpty() ? QStringLiteral("串口传输错误") : what));
        emit statusMessage(QStringLiteral("传票发送失败：%1").arg(what));
        break;
    }
    case KeySessionEvent::KeyStateChanged: {
        const KeySessionSnapshot snapshot = m_session->snapshot();
        const QString targetPort = m_keyPortApplyTargetPort.trimmed();
        if (m_keyPortApplyInProgress) {
            if (!targetPort.isEmpty() && snapshot.portName.trimmed() != targetPort) {
                qInfo().noquote()
                        << QStringLiteral("[KeyManageController] ignore KeyStateChanged from stale runtime during key-port apply target=%1 currentPort=%2 epoch=%3")
                              .arg(targetPort,
                                   snapshot.portName.trimmed().isEmpty() ? QStringLiteral("<none>") : snapshot.portName.trimmed(),
                                   QString::number(m_keyPortApplyEpoch));
                break;
            }

            if (!snapshot.connected) {
                m_lastReadyState = false;
                m_session->setPortSwitchInProgress(false);
                m_keyPortApplyInProgress = false;
                m_suppressSessionAutomation = false;
                m_keySerialApplyState = QStringLiteral("apply_failed");
                m_keySerialApplyMessage = QStringLiteral("配置已保存，但新钥匙串口 %1 在切换过程中断开，运行态未完成接管")
                                                  .arg(targetPort.isEmpty() ? QStringLiteral("<unknown>") : targetPort);
                m_keyPortApplyTargetPort.clear();
                emit sessionSnapshotChanged(snapshot);
                emit statusMessage(m_keySerialApplyMessage);
                refreshKeySerialApplyStatus();
                break;
            }

            if (!snapshot.keyPresent) {
                m_lastReadyState = false;
                emit sessionSnapshotChanged(snapshot);
                refreshKeySerialApplyStatus(QStringLiteral("applying"),
                                            QStringLiteral("新串口 %1 已切到运行态，当前未检测到钥匙，待放稳并完成通讯确认后结束 apply")
                                                    .arg(targetPort.isEmpty() ? snapshot.portName.trimmed() : targetPort));
                break;
            }

            if (!(snapshot.sessionReady && snapshot.protocolHealthy)) {
                m_lastReadyState = false;
                emit sessionSnapshotChanged(snapshot);
                refreshKeySerialApplyStatus(QStringLiteral("applying"),
                                            QStringLiteral("新串口 %1 已打开，正在等待 SET_COM/通讯确认完成")
                                                    .arg(targetPort.isEmpty() ? snapshot.portName.trimmed() : targetPort));
                break;
            }

            m_session->setPortSwitchInProgress(false);
            m_keyPortApplyInProgress = false;
            m_suppressSessionAutomation = false;
            m_acceptedBridgeEpoch = snapshot.bridgeEpoch;
            m_keySerialApplyState = QStringLiteral("applied");
            m_keySerialApplyMessage = QStringLiteral("钥匙串口已切换到 %1，新运行态已完成通讯确认")
                                              .arg(targetPort.isEmpty() ? snapshot.portName.trimmed() : targetPort);
            m_keyPortApplyTargetPort.clear();
        }

        const bool ready = snapshot.connected
                && snapshot.keyPresent
                && snapshot.keyStable
                && snapshot.sessionReady;
        const bool linkInterrupted = !snapshot.connected || !snapshot.keyPresent;
        if (linkInterrupted) {
            invalidateCurrentKeySession(!snapshot.connected
                    ? QStringLiteral("串口断开")
                    : QStringLiteral("钥匙移除"));
            if (!m_activeTransferTaskId.isEmpty()) {
                const QString reason = !snapshot.connected
                        ? QStringLiteral("串口已断开，当前传票已中断，请重新连接后重试")
                        : QStringLiteral("钥匙已移除，当前传票已中断，请重新放回钥匙后重试");
                m_ticketStore->updateTransferState(m_activeTransferTaskId,
                                                   QStringLiteral("failed"),
                                                   reason);
                m_activeTransferTaskId.clear();
                m_activeTransferSessionId = 0;
                m_pendingTransferHandshakeTaskId.clear();
                m_pendingTransferHandshakeJsonBytes.clear();
                m_pendingTransferHandshakeDebugChunk = 0;
                // 钥匙中断时 HTTP 事务也必须释放，否则后续请求会被永久挡住
                if (m_httpTransferTransactionActive) {
                    clearHttpTransferTransaction(false);
                }
                emit statusMessage(reason);
            }

            if (!m_activeReturnTaskId.isEmpty() || !m_pendingReturnHandshakeTaskId.isEmpty()) {
                const QString reason = !snapshot.connected
                        ? QStringLiteral("串口已断开，当前回传已中断，请重新连接后重试")
                        : QStringLiteral("钥匙已移除，当前回传已中断，请重新放回钥匙后重试");
                failActiveReturnHandshake(reason);
                emit statusMessage(reason);
            }

            if (!m_pendingDeletedSystemTicketId.isEmpty()) {
                const QString taskId = m_pendingDeletedSystemTicketId;
                if (m_pendingDeleteOrigin == DeleteOrigin::AdminClearOrphan) {
                    // AdminClearOrphan：钥匙移除即视为本轮操作中断，不自动续删，保留孤儿记录
                    m_pendingDeletedSystemTicketId.clear();
                    m_pendingDeletedKeyTaskRaw.clear();
                    m_pendingDeleteAllowsRetransfer = false;
                    m_pendingDeleteSessionId = 0;
                    m_pendingDeleteRetryCount = 0;
                    m_pendingDeleteOrigin = DeleteOrigin::None;
                    emit statusMessage(QStringLiteral("未登记任务清理中断（钥匙已移除），请重新放回钥匙后再次点击清除：%1").arg(taskId));
                } else {
                    m_ticketStore->updateReturnState(taskId,
                                                     QStringLiteral("return-delete-verifying"),
                                                     QStringLiteral("钥匙已移除，删除验证已中断，等待重新放回后继续对账"));
                    m_pendingDeleteSessionId = 0;
                    emit statusMessage(QStringLiteral("钥匙已移除，任务 %1 的钥匙清理验证已中断；重新放回后将继续检查")
                                           .arg(taskId));
                }
            }
        }

        if (linkInterrupted && !m_pendingCancelSystemTicketId.isEmpty()) {
            const QString taskId = m_pendingCancelSystemTicketId;
            const SystemTicketDto ticket = m_ticketStore->ticketById(taskId);
            if (ticket.valid) {
                m_ticketStore->updateCancelTracking(taskId,
                                                    QStringLiteral("cancel-pending"),
                                                    QStringLiteral("workbench-http"),
                                                    ticket.residentSlotId.isEmpty() ? QStringLiteral("slot-1") : ticket.residentSlotId,
                                                    ticket.residentKeyId,
                                                    ticket.cancelRequestedAt.isValid() ? ticket.cancelRequestedAt : QDateTime::currentDateTime(),
                                                    true,
                                                    QStringLiteral("钥匙已移除，撤销删除验证已中断，等待重新放回后继续对账"));
                m_ticketCancelCoordinator->recordAccepted(taskId,
                                                         m_ticketStore->ticketById(taskId),
                                                         QStringLiteral("cancel-pending"),
                                                         QStringLiteral("钥匙已移除，撤销删除验证已中断，等待重新放回后继续对账"));
            }
            m_pendingCancelSessionId = 0;
            emit statusMessage(QStringLiteral("钥匙已移除，任务 %1 的撤销删除链已中断；重新放回后将继续对账").arg(taskId));
        }

        if (ready && !m_lastReadyState) {
            openNewKeySession();
        }

        emit sessionSnapshotChanged(snapshot);
        refreshKeySerialApplyStatus();
        if (!m_suppressSessionAutomation && !m_httpTransferTransactionActive) {
            tryAutoRefreshKeyTasksOnReady(snapshot);
            schedulePendingCancelReconcile(QStringLiteral("key-state-ready"), 150);
            tryAutoTransferPendingTicket();
        } else if (m_httpTransferTransactionActive) {
            qInfo().noquote()
                    << QStringLiteral("[KeyManageController] skip ready-edge automation during active http transfer transaction taskId=%1 session=%2 epoch=%3")
                          .arg(m_httpTransferTransactionTaskId.isEmpty()
                                       ? QStringLiteral("<none>")
                                       : m_httpTransferTransactionTaskId,
                               QString::number(m_httpTransferTransactionSessionId),
                               QString::number(m_httpTransferTransactionBridgeEpoch));
        }
        break;
    }
    case KeySessionEvent::RawProtocol:
        break;
    }
}

void KeyManageController::handleLogItem(const LogItem &item)
{
    m_logManager->append(item);
    if (m_logManager->shouldDisplay(item)) {
        emit logRowAppended(item);
    }
}

void KeyManageController::handleSystemTicketsChanged(const QList<SystemTicketDto> &tickets)
{
    qInfo().noquote()
            << QStringLiteral("[KeyManageController] handleSystemTicketsChanged count=%1 selectedTaskId=%2 activeTransfer=%3 activeReturn=%4 pendingDelete=%5")
                  .arg(QString::number(tickets.size()),
                       m_selectedSystemTicketId.isEmpty() ? QStringLiteral("<none>") : m_selectedSystemTicketId,
                       m_activeTransferTaskId.isEmpty() ? QStringLiteral("<none>") : m_activeTransferTaskId,
                       m_activeReturnTaskId.isEmpty() ? QStringLiteral("<none>") : m_activeReturnTaskId,
                       m_pendingDeletedSystemTicketId.isEmpty() ? QStringLiteral("<none>") : m_pendingDeletedSystemTicketId);
    emit statusMessage(QStringLiteral("系统票列表已刷新，当前票数=%1").arg(tickets.size()));
    emit systemTicketsUpdated(tickets);
    if (!m_selectedSystemTicketId.isEmpty()) {
        for (const SystemTicketDto &ticket : tickets) {
            if (ticket.taskId == m_selectedSystemTicketId) {
                emit selectedSystemTicketChanged(ticket);
                return;
            }
        }
    }
}

void KeyManageController::handleJsonReceived(const QByteArray &jsonBytes, const QString &savedPath)
{
    QString taskId;
    QString resultMessage;
    ingestWorkbenchJson(jsonBytes, savedPath, &taskId, &resultMessage);
}

bool KeyManageController::ingestWorkbenchJson(const QByteArray &jsonBytes,
                                              const QString &savedPath,
                                              QString *taskIdOut,
                                              QString *resultMessage,
                                              bool scheduleAutoTransfer)
{
    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(jsonBytes, &pe);
    QString taskId;
    if (!doc.isNull() && doc.isObject()) {
        const QJsonObject root = doc.object();
        const QJsonObject ticketObj = (root.contains("data") && root.value("data").isObject())
                ? root.value("data").toObject()
                : root;
        taskId = ticketObj.value("taskId").toString();
    }
    if (taskIdOut) {
        *taskIdOut = taskId;
    }
    const SystemTicketDto oldTicket = m_ticketStore->ticketById(taskId);

    qInfo().noquote()
            << QStringLiteral("[KeyManageController] ingestWorkbenchJson begin taskId=%1 payloadBytes=%2 savedPath=%3 oldValid=%4 oldTransfer=%5 oldReturn=%6")
                  .arg(taskId.isEmpty() ? QStringLiteral("<empty>") : taskId,
                       QString::number(jsonBytes.size()),
                       savedPath,
                       oldTicket.valid ? QStringLiteral("true") : QStringLiteral("false"),
                       oldTicket.valid ? oldTicket.transferState : QStringLiteral("<none>"),
                       oldTicket.valid ? oldTicket.returnState : QStringLiteral("<none>"));

    QString ingestError;
    if (!m_ticketStore->ingestJson(jsonBytes, savedPath, &ingestError)) {
        if (resultMessage) {
            *resultMessage = ingestError;
        }
        qWarning().noquote()
                << QStringLiteral("[KeyManageController] ingestWorkbenchJson failed taskId=%1 reason=%2")
                      .arg(taskId.isEmpty() ? QStringLiteral("<empty>") : taskId,
                           ingestError);
        emit statusMessage(QStringLiteral("系统票入池失败：%1").arg(ingestError));
        return false;
    }
    if (resultMessage) {
        resultMessage->clear();
    }

    qInfo().noquote()
            << QStringLiteral("[KeyManageController] ingestWorkbenchJson accepted taskId=%1 autoTransfer=%2")
                  .arg(taskId,
                       autoTransferEnabled() ? QStringLiteral("true") : QStringLiteral("false"));
    emit statusMessage(QStringLiteral("系统票入池成功：taskId=%1，当前状态=received").arg(taskId));

    if (oldTicket.valid) {
        if (oldTicket.transferState == QLatin1String("success")) {
            if (resultMessage) {
                *resultMessage = QStringLiteral("ticket already transferred, duplicate ignored");
            }
            qInfo().noquote()
                    << QStringLiteral("[KeyManageController] ingestWorkbenchJson duplicate success taskId=%1 ignored=true")
                          .arg(taskId);
            emit statusMessage(QStringLiteral("系统票已存在且已传票到钥匙，本次重复请求已忽略"));
            return true;
        }
        if (oldTicket.transferState == QLatin1String("sending")) {
            if (resultMessage) {
                *resultMessage = QStringLiteral("ticket is already sending, duplicate ignored");
            }
            qInfo().noquote()
                    << QStringLiteral("[KeyManageController] ingestWorkbenchJson duplicate sending taskId=%1 ignored=true")
                          .arg(taskId);
            emit statusMessage(QStringLiteral("系统票正在传票到钥匙，本次重复请求已忽略"));
            return true;
        }
        if (oldTicket.transferState == QLatin1String("key-cleared")) {
            if (scheduleAutoTransfer && autoTransferEnabled()) {
                if (resultMessage) {
                    *resultMessage = QStringLiteral("ticket accepted, key task cleared, auto retransfer scheduled");
                }
                qInfo().noquote()
                        << QStringLiteral("[KeyManageController] ingestWorkbenchJson key-cleared taskId=%1 action=auto-retransfer")
                              .arg(taskId);
                emit statusMessage(QStringLiteral("系统票对应钥匙任务已删除，准备再次自动传票"));
                tryStartTicketTransfer(taskId, true);
            } else {
                if (resultMessage) {
                    *resultMessage = scheduleAutoTransfer
                            ? QStringLiteral("ticket accepted, key task cleared, manual retransfer allowed")
                            : QStringLiteral("ticket accepted, key task cleared, waiting for immediate transfer");
                }
                qInfo().noquote()
                        << QStringLiteral("[KeyManageController] ingestWorkbenchJson key-cleared taskId=%1 action=%2")
                              .arg(taskId,
                                   scheduleAutoTransfer
                                   ? QStringLiteral("manual-retransfer")
                                   : QStringLiteral("immediate-transfer"));
                emit statusMessage(scheduleAutoTransfer
                                   ? QStringLiteral("系统票对应钥匙任务已删除，可手动再次传票")
                                   : QStringLiteral("系统票对应钥匙任务已删除，准备立即重新传票"));
            }
            return true;
        }
    }

    if (!scheduleAutoTransfer) {
        if (resultMessage) {
            *resultMessage = QStringLiteral("ticket accepted, ready for immediate transfer");
        }
        emit statusMessage(QStringLiteral("系统票已接收并入池，准备立即发送到钥匙"));
    } else if (autoTransferEnabled()) {
        if (resultMessage) {
            *resultMessage = QStringLiteral("ticket accepted, queued for auto transfer");
        }
        emit statusMessage(QStringLiteral("系统票已接收并入池，准备自动传票"));
    } else {
        if (resultMessage) {
            *resultMessage = QStringLiteral("ticket accepted, auto transfer disabled");
        }
        emit statusMessage(QStringLiteral("系统票已接收并入池（自动传票已关闭）"));
    }
    qInfo().noquote()
            << QStringLiteral("[KeyManageController] ingestWorkbenchJson taskId=%1 nextAction=%2")
                  .arg(taskId,
                       scheduleAutoTransfer ? QStringLiteral("queue-auto-transfer")
                                            : QStringLiteral("immediate-transfer-by-caller"));
    if (scheduleAutoTransfer) {
        tryAutoTransferPendingTicket();
    }
    return true;
}

void KeyManageController::handleHttpServerLog(const QString &text)
{
    m_httpServerLogLines << text;
    if (m_httpServerLogLines.size() > 2000) {
        m_httpServerLogLines.removeFirst();
    }
    emit httpServerLogAppended(text);
}

void KeyManageController::handleHttpClientLog(const QString &text)
{
    m_httpClientLogLines << text;
    if (m_httpClientLogLines.size() > 2000) {
        m_httpClientLogLines.removeFirst();
    }
    emit httpClientLogAppended(text);
}

void KeyManageController::handleInitPayloadReady(const QByteArray &payload)
{
    CommandRequest req;
    req.id = CommandId::InitKey;
    req.opId = nextOpId();
    req.payload = payload;
    m_session->execute(req);
    emit statusMessage(QStringLiteral("初始化数据已准备完成，开始下发到钥匙"));
}

void KeyManageController::handleRfidPayloadReady(const QByteArray &payload)
{
    CommandRequest req;
    req.id = CommandId::DownloadRfid;
    req.opId = nextOpId();
    req.payload = payload;
    m_session->execute(req);
    emit statusMessage(QStringLiteral("RFID 数据已准备完成，开始下发到钥匙"));
}

void KeyManageController::handleProvisionRequestFailed(const QString &reason)
{
    emit statusMessage(reason);
}

void KeyManageController::handleReturnUploadSucceeded(const QString &taskId)
{
    emit statusMessage(QStringLiteral("回传上传成功：%1").arg(taskId));

    if (!m_activeReturnTaskIdRaw.isEmpty() && isCurrentKeySession(m_activeReturnSessionId)) {
        markReturnDeletePending(taskId, m_activeReturnTaskIdRaw);

        CommandRequest req;
        req.id = CommandId::DeleteTask;
        req.opId = nextOpId();
        req.payload = m_activeReturnTaskIdRaw;
        m_session->execute(req);
        emit statusMessage(QStringLiteral("回传上传成功，开始删除钥匙中的已完成任务"));
    } else {
        const QString reason = QStringLiteral("回传上传成功，但缺少钥匙任务原始ID，无法自动删除钥匙任务");
        m_ticketStore->updateReturnState(taskId, QStringLiteral("manual-required"), reason);
        emit statusMessage(reason);
    }

    clearActiveReturnContext();
}

void KeyManageController::handleReturnUploadFailed(const QString &taskId, const QString &reason)
{
    m_ticketStore->updateReturnState(taskId, QStringLiteral("manual-required"), reason);
    emit statusMessage(QStringLiteral("回传上传失败：%1").arg(reason));
    clearActiveReturnContext();
}

void KeyManageController::failActiveReturnHandshake(const QString &reason)
{
    const QString taskId = !m_activeReturnTaskId.isEmpty()
            ? m_activeReturnTaskId
            : m_pendingReturnHandshakeTaskId;
    if (!taskId.isEmpty()) {
        m_ticketStore->updateReturnState(taskId,
                                         QStringLiteral("return-interrupted-retryable"),
                                         reason);
    }
    clearActiveReturnContext();
}

void KeyManageController::clearActiveReturnContext(bool clearHandshakeOnly)
{
    if (!clearHandshakeOnly) {
        m_activeReturnTaskId.clear();
        m_activeReturnTaskIdRaw.clear();
        m_activeReturnSessionId = 0;
    }
    m_pendingReturnHandshakeTaskId.clear();
    m_pendingReturnHandshakeTaskIdRaw.clear();
    schedulePendingCancelReconcile(QStringLiteral("return-context-cleared"), 300);
}

void KeyManageController::markReturnDeletePending(const QString &taskId, const QByteArray &taskIdRaw)
{
    m_ticketStore->updateReturnState(taskId, QStringLiteral("return-upload-success"));
    m_ticketStore->updateReturnState(taskId, QStringLiteral("return-delete-verifying"));
    m_pendingDeletedSystemTicketId = taskId;
    m_pendingDeletedKeyTaskRaw = taskIdRaw;
    m_pendingDeleteAllowsRetransfer = false;
    m_pendingDeleteSessionId = m_activeReturnSessionId;
    m_pendingDeleteRetryCount = 0;
    m_pendingDeleteOrigin = DeleteOrigin::AutoReturnCleanup;
}

void KeyManageController::finalizePendingReturnDelete(const QList<KeyTaskDto> &tasks)
{
    if (m_pendingDeletedSystemTicketId.isEmpty() || m_pendingDeletedKeyTaskRaw.isEmpty()) {
        return;
    }
    if (m_pendingDeleteSessionId > 0 && !isCurrentKeySession(m_pendingDeleteSessionId)) {
        qInfo().noquote()
                << QStringLiteral("[KeyManageController] finalizePendingReturnDelete skipped stale session pendingDeleteSession=%1 currentSession=%2 taskId=%3")
                      .arg(QString::number(m_pendingDeleteSessionId),
                           QString::number(m_keySessionId),
                           m_pendingDeletedSystemTicketId.isEmpty() ? QStringLiteral("<none>") : m_pendingDeletedSystemTicketId);
        return;
    }

    bool stillExists = false;
    for (const KeyTaskDto &task : tasks) {
        if (task.taskId == m_pendingDeletedKeyTaskRaw) {
            stillExists = true;
            break;
        }
    }

    const bool sessionUnknown = (m_pendingDeleteSessionId == 0);
    if (sessionUnknown && !stillExists) {
        if (m_pendingDeleteOrigin == DeleteOrigin::AdminClearOrphan) {
            // AdminClearOrphan：会话已中断且任务已消失，无法确认是否同一把钥匙，保守停止
            // 孤儿记录保留，由 reconcileStaleOrphanTickets 在 session 稳定后自动确认清理
            const QString taskId = m_pendingDeletedSystemTicketId;
            m_pendingDeletedSystemTicketId.clear();
            m_pendingDeletedKeyTaskRaw.clear();
            m_pendingDeleteAllowsRetransfer = false;
            m_pendingDeleteSessionId = 0;
            m_pendingDeleteRetryCount = 0;
            m_pendingDeleteOrigin = DeleteOrigin::None;
            emit statusMessage(QStringLiteral("未登记任务清理中断（会话已中断），孤儿记录保留待下次 Q_TASK 确认：%1").arg(taskId));
            return;
        }
        if (m_pendingDeleteOrigin != DeleteOrigin::ManualDelete) {
            m_ticketStore->updateReturnState(m_pendingDeletedSystemTicketId,
                                             QStringLiteral("return-delete-verifying"),
                                             QStringLiteral("删除链路曾被钥匙移除打断，当前钥匙未观测到该任务，继续等待原钥匙回位后再确认"));
        }
        emit statusMessage(QStringLiteral("删除链路曾被钥匙移除打断，当前未确认到原任务，继续等待原钥匙回位"));
        return;
    }
    if (stillExists || sessionUnknown) {
        if (m_pendingDeleteOrigin == DeleteOrigin::AdminClearOrphan) {
            // AdminClearOrphan：任务仍存在或会话不确定，保守停止，不自动续删
            const QString taskId = m_pendingDeletedSystemTicketId;
            m_pendingDeletedSystemTicketId.clear();
            m_pendingDeletedKeyTaskRaw.clear();
            m_pendingDeleteAllowsRetransfer = false;
            m_pendingDeleteSessionId = 0;
            m_pendingDeleteRetryCount = 0;
            m_pendingDeleteOrigin = DeleteOrigin::None;
            emit statusMessage(QStringLiteral("未登记任务清理未完成（钥匙任务仍存在或会话不确定），请再次点击清除：%1").arg(taskId));
            return;
        }
        const QString retryReason = (sessionUnknown && !stillExists)
                ? QStringLiteral("删除后钥匙发生过移除，当前无法确认是否为同一把钥匙，重新发送 DEL 以避免误判成功")
                : QStringLiteral("删除后验证发现钥匙任务仍存在，重新发送 DEL 继续清理");
        if (!retryPendingReturnDelete(retryReason)) {
            emit statusMessage(retryReason);
        }
        return;
    }
    if (!stillExists && !sessionUnknown) {
        if (m_pendingDeleteOrigin == DeleteOrigin::ManualDelete) {
            m_ticketStore->markKeyTaskDeleted(m_pendingDeletedSystemTicketId);
            const SystemTicketDto ticket = m_ticketStore->ticketById(m_pendingDeletedSystemTicketId);
            if (ticket.valid) {
                callTerminationThenRemove(m_pendingDeletedSystemTicketId);
                emit statusMessage(QStringLiteral("钥匙任务已删除，正在通知工作台终止系统票：%1")
                                       .arg(m_pendingDeletedSystemTicketId));
            } else {
                emit statusMessage(QStringLiteral("钥匙残留任务已清理（本地记录已过期）：%1")
                                       .arg(m_pendingDeletedSystemTicketId));
            }
        } else if (m_pendingDeleteOrigin == DeleteOrigin::AdminClearOrphan) {
            // 孤儿任务清除：仅做本地删除，不调 callTermination，不更新 returnState
            m_ticketStore->removeTicket(m_pendingDeletedSystemTicketId);
            qInfo().noquote()
                    << QStringLiteral("[KeyManageController] AdminClearOrphan: orphan task removed taskId=%1")
                          .arg(m_pendingDeletedSystemTicketId);
            emit statusMessage(QStringLiteral("钥匙未登记任务已清除：%1").arg(m_pendingDeletedSystemTicketId));
        } else {
            // 回传清理收口 — 原有逻辑完全不变
            if (m_pendingDeleteAllowsRetransfer) {
                m_ticketStore->markKeyTaskDeleted(m_pendingDeletedSystemTicketId);
                emit statusMessage(QStringLiteral("钥匙任务已删除，可再次传票"));
            } else {
                const SystemTicketDto ticket = m_ticketStore->ticketById(m_pendingDeletedSystemTicketId);
                if (ticket.valid && (ticket.returnState == QLatin1String("return-delete-verifying")
                                     || ticket.returnState == QLatin1String("return-delete-pending")
                                     || ticket.returnState == QLatin1String("return-upload-success"))) {
                    // finish-step-batch 已上传成功，工作台已收到完成数据会自己关票
                    // 直接删除本地记录完成闭环，无需再调 callTermination
                    m_ticketStore->removeTicket(m_pendingDeletedSystemTicketId);
                    emit statusMessage(QStringLiteral("回传完成，本地记录已清除：%1")
                                           .arg(m_pendingDeletedSystemTicketId));
                } else {
                    emit statusMessage(QStringLiteral("钥匙任务已删除"));
                }
            }
        }
        m_pendingDeletedSystemTicketId.clear();
        m_pendingDeletedKeyTaskRaw.clear();
        m_pendingDeleteAllowsRetransfer = false;
        m_pendingDeleteSessionId = 0;
        m_pendingDeleteRetryCount = 0;
        m_pendingDeleteOrigin = DeleteOrigin::None;
    } else {
        if (m_pendingDeleteOrigin != DeleteOrigin::ManualDelete
                && m_pendingDeleteOrigin != DeleteOrigin::AdminClearOrphan) {
            m_ticketStore->updateReturnState(m_pendingDeletedSystemTicketId,
                                             QStringLiteral("return-delete-verifying"),
                                             QStringLiteral("删除后验证发现钥匙任务仍存在，等待后续自动清理"));
        }
        emit statusMessage(QStringLiteral("删除后验证发现钥匙任务仍存在，请继续等待自动清理"));
    }
}

void KeyManageController::reconcileStaleOrphanTickets(const QList<KeyTaskDto> &tasks)
{
    // 对所有 orphan-recovered 条目，检查对应 key task 是否已从钥匙消失
    // 条件：无 pending delete、当前条目、Q_TASK 确认任务不在 → 直接清理本地记录
    // 不发任何协议命令，不写 returnState，纯本地 TicketStore 清理
    for (const SystemTicketDto &ticket : m_ticketStore->tickets()) {
        if (ticket.transferState != QLatin1String("orphan-recovered")) {
            continue;
        }
        // 正在走 pending delete 的由 finalizePendingReturnDelete 负责，跳过
        if (ticket.taskId == m_pendingDeletedSystemTicketId) {
            continue;
        }
        // Q_TASK 确认任务不在钥匙里
        if (!taskExistsInList(tasks, ticket.taskId)) {
            m_ticketStore->removeTicket(ticket.taskId);
            qInfo().noquote()
                    << QStringLiteral("[KeyManageController] reconcileStaleOrphanTickets: orphan task confirmed absent from key, local record removed taskId=%1")
                          .arg(ticket.taskId);
            emit statusMessage(QStringLiteral("已确认未登记任务不在钥匙中，本地残留记录已清除：%1").arg(ticket.taskId));
        }
    }
}

bool KeyManageController::retryPendingReturnDelete(const QString &reason)
{
    if (m_pendingDeletedSystemTicketId.isEmpty() || m_pendingDeletedKeyTaskRaw.isEmpty()) {
        return false;
    }

    const bool isManualDelete = (m_pendingDeleteOrigin == DeleteOrigin::ManualDelete);
    const bool isOrphanClear = (m_pendingDeleteOrigin == DeleteOrigin::AdminClearOrphan);

    const KeySessionSnapshot snapshot = m_session->snapshot();
    if (!snapshot.connected || !snapshot.keyPresent || !snapshot.keyStable || !snapshot.sessionReady) {
        if (!isManualDelete && !isOrphanClear) {
            m_ticketStore->updateReturnState(m_pendingDeletedSystemTicketId,
                                             QStringLiteral("return-delete-verifying"),
                                             reason.isEmpty()
                                                     ? QStringLiteral("等待钥匙重新就绪后继续删除验证")
                                                     : reason);
        }
        return false;
    }
    if (snapshot.commandInFlight || snapshot.recoveryWindowActive) {
        if (!isManualDelete && !isOrphanClear) {
            m_ticketStore->updateReturnState(m_pendingDeletedSystemTicketId,
                                             QStringLiteral("return-delete-verifying"),
                                             reason.isEmpty()
                                                     ? QStringLiteral("等待当前协议窗口收口后继续删除验证")
                                                     : reason);
        }
        return false;
    }
    if (m_pendingDeleteRetryCount >= 3) {
        if (isManualDelete) {
            // ManualDelete：DEL 串口确认失败，不写 returnState，改写 adminDeleteStage 供 UI 提示
            m_ticketStore->updateAdminDeleteStage(
                    m_pendingDeletedSystemTicketId,
                    SystemTicketDto::AdminDeleteStage::TerminationFailed);
            m_ticketStore->updateAdminDeleteError(
                    m_pendingDeletedSystemTicketId,
                    QStringLiteral("DEL 串口命令重试 3 次仍未确认删除，钥匙任务状态未知，请人工确认"));
        } else if (isOrphanClear) {
            // AdminClearOrphan：不写任何状态，直接记日志，孤儿票保留供人工核查
            qWarning().noquote()
                    << QStringLiteral("[KeyManageController] AdminClearOrphan: DEL retry limit reached, orphan ticket preserved for manual check taskId=%1")
                          .arg(m_pendingDeletedSystemTicketId);
        } else {
            m_ticketStore->updateReturnState(m_pendingDeletedSystemTicketId,
                                             QStringLiteral("return-delete-failed"),
                                             QStringLiteral("DEL 已重试 3 次仍未确认删除，请人工处理"));
        }
        emit statusMessage(QStringLiteral("任务 %1 删除验证失败：DEL 已重试 3 次仍未收口，请人工处理")
                               .arg(m_pendingDeletedSystemTicketId));
        m_pendingDeletedSystemTicketId.clear();
        m_pendingDeletedKeyTaskRaw.clear();
        m_pendingDeleteAllowsRetransfer = false;
        m_pendingDeleteSessionId = 0;
        m_pendingDeleteRetryCount = 0;
        m_pendingDeleteOrigin = DeleteOrigin::None;
        return false;
    }

    ++m_pendingDeleteRetryCount;
    m_pendingDeleteSessionId = m_keySessionId;
    if (!isManualDelete && !isOrphanClear) {
        m_ticketStore->updateReturnState(m_pendingDeletedSystemTicketId,
                                         QStringLiteral("return-delete-pending"),
                                         QStringLiteral("DEL 重试第 %1/3 次：%2")
                                                 .arg(m_pendingDeleteRetryCount)
                                                 .arg(reason.isEmpty() ? QStringLiteral("继续确认钥匙任务清理结果") : reason));
    }

    CommandRequest req;
    req.id = CommandId::DeleteTask;
    req.opId = nextOpId();
    req.payload = m_pendingDeletedKeyTaskRaw;
    m_session->execute(req);
    emit statusMessage(QStringLiteral("任务 %1 删除验证未收口，已自动重发 DEL（第 %2/3 次）")
                           .arg(m_pendingDeletedSystemTicketId)
                           .arg(m_pendingDeleteRetryCount));
    return true;
}

void KeyManageController::callTermination(const QString &taskId)
{
    AuthServiceAdapter authAdapter;
    const QString accessToken = authAdapter.accessToken().trimmed();
    if (accessToken.isEmpty()) {
        qWarning().noquote()
                << QStringLiteral("[KeyManageController] callTermination skip: no accessToken taskId=%1").arg(taskId);
        emit statusMessage(QStringLiteral("终止系统票失败：未登录，请管理员手动在工作台终止"));
        return;
    }

    QString apiBase = ConfigManager::instance()->apiUrl();
    if (apiBase.endsWith(QLatin1String("/third-api"))) {
        apiBase.chop(10);
    }
    while (apiBase.endsWith(QLatin1Char('/'))) {
        apiBase.chop(1);
    }

    QUrl url(apiBase + QStringLiteral("/task/termination"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("id"), taskId);
    query.addQueryItem(QStringLiteral("reason"), QStringLiteral("管理员手工删除钥匙任务"));
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("kids-auth", QStringLiteral("bearer %1").arg(accessToken).toUtf8());
    request.setRawHeader("kids-requested-with", "KidsHttpRequest");

    qInfo().noquote()
            << QStringLiteral("[KeyManageController] callTermination POST taskId=%1 url=%2").arg(taskId, url.toString());

    QNetworkReply *reply = m_terminationNetwork->post(request, QByteArray("{}"));
    connect(reply, &QNetworkReply::finished, this, [this, reply, taskId]() {
        reply->deleteLater();
        const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() == QNetworkReply::NoError && httpStatus >= 200 && httpStatus < 300) {
            qInfo().noquote()
                    << QStringLiteral("[KeyManageController] callTermination success taskId=%1 http=%2").arg(taskId).arg(httpStatus);
        } else {
            qWarning().noquote()
                    << QStringLiteral("[KeyManageController] callTermination failed taskId=%1 http=%2 error=%3")
                           .arg(taskId).arg(httpStatus).arg(reply->errorString());
            emit statusMessage(QStringLiteral("终止系统票接口调用失败（HTTP %1），请管理员在工作台手动终止").arg(httpStatus));
        }
    });
}

void KeyManageController::callTerminationThenRemove(const QString &taskId)
{
    // 重入防护：仅阻止本次运行时真正已在途的 HTTP 请求，不阻止跨会话重试
    if (m_terminationInFlightTaskIds.contains(taskId)) {
        qInfo().noquote()
                << QStringLiteral("[KeyManageController] callTerminationThenRemove skip: already in-flight taskId=%1")
                      .arg(taskId);
        return;
    }

    AuthServiceAdapter authAdapter;
    const QString accessToken = authAdapter.accessToken().trimmed();
    if (accessToken.isEmpty()) {
        qWarning().noquote()
                << QStringLiteral("[KeyManageController] callTerminationThenRemove skip: no accessToken taskId=%1")
                      .arg(taskId);
        m_ticketStore->updateAdminDeleteStage(
                taskId, SystemTicketDto::AdminDeleteStage::KeyClearedAwaitingTermination);
        m_ticketStore->updateAdminDeleteError(
                taskId, QStringLiteral("未登录，等待下次登录后自动重试"));
        emit statusMessage(
                QStringLiteral("工作台通知失败：未登录，下次登录后将自动重试（任务 ID: %1）").arg(taskId));
        return;
    }

    m_ticketStore->updateAdminDeleteStage(
            taskId, SystemTicketDto::AdminDeleteStage::KeyClearedAwaitingTermination);
    m_ticketStore->updateAdminDeleteError(taskId, QString());

    QString apiBase = ConfigManager::instance()->apiUrl();
    if (apiBase.endsWith(QLatin1String("/third-api"))) {
        apiBase.chop(10);
    }
    while (apiBase.endsWith(QLatin1Char('/'))) {
        apiBase.chop(1);
    }

    QUrl url(apiBase + QStringLiteral("/task/termination"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("id"), taskId);
    query.addQueryItem(QStringLiteral("reason"), QStringLiteral("管理员手工删除钥匙任务"));
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("kids-auth", QStringLiteral("bearer %1").arg(accessToken).toUtf8());
    request.setRawHeader("kids-requested-with", "KidsHttpRequest");

    qInfo().noquote()
            << QStringLiteral("[KeyManageController] callTerminationThenRemove POST taskId=%1 url=%2")
                  .arg(taskId, url.toString());

    m_terminationInFlightTaskIds.insert(taskId);
    QNetworkReply *reply = m_terminationNetwork->post(request, QByteArray("{}"));
    connect(reply, &QNetworkReply::finished, this, [this, reply, taskId]() {
        const QByteArray body = reply->readAll();
        const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const bool transportOk = reply->error() == QNetworkReply::NoError
                && httpStatus >= 200 && httpStatus < 300;
        QString errText = reply->errorString();
        bool businessOk = false;

        if (transportOk) {
            QJsonParseError parseError;
            const QJsonDocument doc = QJsonDocument::fromJson(body, &parseError);
            if (parseError.error == QJsonParseError::NoError && doc.isObject()) {
                const QJsonObject obj = doc.object();
                const bool successFlag = obj.value(QStringLiteral("success")).toBool(false);
                const int code = obj.value(QStringLiteral("code")).toInt(-1);
                businessOk = successFlag || code == 200;
                if (!businessOk) {
                    const QString message = obj.value(QStringLiteral("message")).toString().trimmed();
                    if (!message.isEmpty()) {
                        errText = message;
                    }
                }
            } else {
                // body 非 JSON：空 body 视为成功（REST 约定），非空非 JSON 内容视为异常
                if (body.trimmed().isEmpty()) {
                    businessOk = true;
                } else {
                    errText = QStringLiteral("响应格式异常（非 JSON），HTTP %1").arg(httpStatus);
                }
            }
        }

        reply->deleteLater();
        m_terminationInFlightTaskIds.remove(taskId);

        if (businessOk) {
            qInfo().noquote()
                    << QStringLiteral("[KeyManageController] callTerminationThenRemove success taskId=%1 http=%2")
                          .arg(taskId).arg(httpStatus);
            m_ticketStore->removeTicket(taskId);
            emit statusMessage(QStringLiteral("系统票已终止并删除：%1").arg(taskId));
        } else {
            if (errText.trimmed().isEmpty()) {
                errText = transportOk
                        ? QStringLiteral("终止系统票业务返回失败")
                        : QStringLiteral("网络或接口调用失败");
            }
            qWarning().noquote()
                    << QStringLiteral("[KeyManageController] callTerminationThenRemove failed taskId=%1 http=%2 error=%3")
                          .arg(taskId).arg(httpStatus).arg(errText);
            m_ticketStore->updateAdminDeleteStage(
                    taskId, SystemTicketDto::AdminDeleteStage::TerminationFailed);
            m_ticketStore->updateAdminDeleteError(taskId, errText);
            emit statusMessage(
                    QStringLiteral("工作台通知失败，请手动在工作台关闭任务 ID: %1（错误：%2）")
                            .arg(taskId, errText));
        }
    });
}

bool KeyManageController::canProcessCancelNow(QString *blockedReason) const
{
    auto setBlockedReason = [blockedReason](const QString &reason) {
        if (blockedReason) {
            *blockedReason = reason;
        }
    };

    if (m_pendingCancelSystemTicketId.size() > 0) {
        setBlockedReason(QStringLiteral("当前已有撤销删除链在执行"));
        return false;
    }
    if (!m_activeTransferTaskId.isEmpty()) {
        setBlockedReason(QStringLiteral("当前存在传票链在途"));
        return false;
    }
    if (!m_activeReturnTaskId.isEmpty()
            || !m_pendingReturnHandshakeTaskId.isEmpty()
            || !m_pendingDeletedSystemTicketId.isEmpty()) {
        setBlockedReason(QStringLiteral("当前存在回传或删除验证链在途"));
        return false;
    }

    const KeySessionSnapshot snapshot = m_session->snapshot();
    if (!snapshot.connected || !snapshot.keyPresent || !snapshot.keyStable || !snapshot.sessionReady) {
        setBlockedReason(QStringLiteral("当前钥匙会话未就绪"));
        return false;
    }
    if (snapshot.commandInFlight) {
        setBlockedReason(QStringLiteral("当前底层仍有协议命令在途"));
        return false;
    }
    if (snapshot.recoveryWindowActive) {
        setBlockedReason(QStringLiteral("当前处于自动恢复窗口"));
        return false;
    }
    if (m_autoQuerySessionId == m_keySessionId && m_keySessionId > 0) {
        setBlockedReason(QStringLiteral("当前正在执行自动 Q_TASK 对账"));
        return false;
    }

    setBlockedReason(QString());
    return true;
}

void KeyManageController::schedulePendingCancelReconcile(const QString &reason, int delayMs)
{
    if (m_cancelReconcileScheduled || !m_ticketCancelCoordinator->hasPendingWork()) {
        return;
    }

    m_cancelReconcileScheduled = true;
    const int effectiveDelayMs = delayMs > 0 ? delayMs : 300;
    qInfo().noquote()
            << QStringLiteral("[KeyManageController] schedulePendingCancelReconcile reason=%1 delayMs=%2 currentSession=%3")
                  .arg(reason.isEmpty() ? QStringLiteral("<none>") : reason,
                       QString::number(effectiveDelayMs),
                       QString::number(m_keySessionId));

    QTimer::singleShot(effectiveDelayMs, this, [this]() {
        m_cancelReconcileScheduled = false;
        if (!m_ticketCancelCoordinator->hasPendingWork()) {
            return;
        }

        QString blockedReason;
        if (!canProcessCancelNow(&blockedReason)) {
            qInfo().noquote()
                    << QStringLiteral("[KeyManageController] pending cancel reconcile skipped reason=%1")
                          .arg(blockedReason.isEmpty() ? QStringLiteral("<none>") : blockedReason);
            return;
        }

        if (m_keySessionId <= 0) {
            return;
        }

        m_autoQuerySessionId = m_keySessionId;
        emit statusMessage(QStringLiteral("检测到待撤销任务，开始读取钥匙票列表进行对账"));
        onQueryTasksClicked();
    });
}

void KeyManageController::tryProcessPendingCancelTasks(const QList<KeyTaskDto> &tasks)
{
    if (!m_ticketCancelCoordinator->hasPendingWork()) {
        return;
    }
    if (!m_pendingCancelSystemTicketId.isEmpty()) {
        return;
    }

    QString blockedReason;
    if (!canProcessCancelNow(&blockedReason)) {
        return;
    }

    const QStringList pendingTaskIds = m_ticketCancelCoordinator->activeTaskIds();
    for (const QString &taskId : pendingTaskIds) {
        if (taskId.trimmed().isEmpty()) {
            continue;
        }

        const SystemTicketDto ticket = m_ticketStore->ticketById(taskId);
        if (ticket.valid) {
            if (ticket.returnState == QLatin1String("return-delete-success")
                    || ticket.returnState == QLatin1String("return-success")
                    || ticket.transferState == QLatin1String("key-cleared")) {
                m_ticketCancelCoordinator->recordDone(taskId, ticket, QStringLiteral("任务已完成，无需撤销"));
                continue;
            }
            if (ticket.returnState == QLatin1String("return-upload-success")
                    || ticket.returnState == QLatin1String("return-delete-pending")
                    || ticket.returnState == QLatin1String("return-delete-verifying")) {
                m_ticketCancelCoordinator->recordDone(taskId, ticket, QStringLiteral("任务已在清理中，无需重复撤销"));
                continue;
            }
        }

        quint8 keyTaskStatus = 0xFF;
        const QByteArray keyTaskRaw = findKeyTaskIdRaw(taskId, &keyTaskStatus);
        if (keyTaskRaw.isEmpty()) {
            if (ticket.valid) {
                m_ticketCancelCoordinator->recordDone(taskId, ticket, QStringLiteral("任务已取消"));
                m_ticketStore->removeTicket(taskId);
            } else {
                m_ticketCancelCoordinator->recordDoneWithoutTicket(taskId, QStringLiteral("任务已取消"));
            }
            emit statusMessage(QStringLiteral("撤销任务已确认不在钥匙中，已完成收口：%1").arg(taskId));
            continue;
        }

        m_pendingCancelSystemTicketId = taskId;
        m_pendingCancelKeyTaskRaw = keyTaskRaw;
        m_pendingCancelSessionId = m_keySessionId;

        if (ticket.valid) {
            m_ticketStore->updateCancelTracking(taskId,
                                                QStringLiteral("cancel-executing"),
                                                QStringLiteral("workbench-http"),
                                                ticket.residentSlotId.isEmpty() ? QStringLiteral("slot-1") : ticket.residentSlotId,
                                                ticket.residentKeyId,
                                                ticket.cancelRequestedAt.isValid() ? ticket.cancelRequestedAt : QDateTime::currentDateTime(),
                                                true,
                                                QStringLiteral("已确认钥匙中存在任务，开始执行 DEL"));
            m_ticketCancelCoordinator->recordExecuting(taskId,
                                                       m_ticketStore->ticketById(taskId),
                                                       QStringLiteral("已确认钥匙中存在任务，开始执行 DEL"));
        } else {
            SystemTicketDto transientTicket;
            transientTicket.taskId = taskId;
            transientTicket.cancelState = QStringLiteral("cancel-executing");
            transientTicket.residentSlotId = QStringLiteral("slot-1");
            transientTicket.cancelSource = QStringLiteral("workbench-http");
            transientTicket.cancelRequestedAt = QDateTime::currentDateTime();
            transientTicket.wasInKey = true;
            transientTicket.valid = true;
            m_ticketCancelCoordinator->recordExecuting(taskId,
                                                       transientTicket,
                                                       QStringLiteral("已确认钥匙中存在任务，开始执行 DEL"));
        }

        CommandRequest req;
        req.id = CommandId::DeleteTask;
        req.opId = nextOpId();
        req.payload = keyTaskRaw;
        m_session->execute(req);
        emit statusMessage(QStringLiteral("检测到待撤销任务仍在钥匙中，开始执行删除：%1").arg(taskId));
        return;
    }
}

void KeyManageController::finalizePendingCancelDelete(const QList<KeyTaskDto> &tasks)
{
    if (m_pendingCancelSystemTicketId.isEmpty()) {
        return;
    }
    if (m_pendingCancelSessionId > 0 && !isCurrentKeySession(m_pendingCancelSessionId)) {
        qInfo().noquote()
                << QStringLiteral("[KeyManageController] finalizePendingCancelDelete skipped stale session pendingCancelSession=%1 currentSession=%2 taskId=%3")
                      .arg(QString::number(m_pendingCancelSessionId),
                           QString::number(m_keySessionId),
                           m_pendingCancelSystemTicketId);
        return;
    }

    bool stillExists = false;
    for (const KeyTaskDto &task : tasks) {
        if (task.taskId == m_pendingCancelKeyTaskRaw) {
            stillExists = true;
            break;
        }
    }

    const QString taskId = m_pendingCancelSystemTicketId;
    const SystemTicketDto ticket = m_ticketStore->ticketById(taskId);
    if (!stillExists) {
        if (ticket.valid) {
            m_ticketCancelCoordinator->recordDone(taskId, ticket, QStringLiteral("任务已取消"));
            m_ticketStore->removeTicket(taskId);
        } else {
            m_ticketCancelCoordinator->recordDoneWithoutTicket(taskId, QStringLiteral("任务已取消"));
        }
        emit statusMessage(QStringLiteral("撤销任务已完成，钥匙侧任务已清理：%1").arg(taskId));
        emit workbenchRefreshRequested();
        m_pendingCancelSystemTicketId.clear();
        m_pendingCancelKeyTaskRaw.clear();
        m_pendingCancelSessionId = 0;
        return;
    }

    if (ticket.valid) {
        m_ticketStore->updateCancelTracking(taskId,
                                            QStringLiteral("cancel-pending"),
                                            QStringLiteral("workbench-http"),
                                            ticket.residentSlotId.isEmpty() ? QStringLiteral("slot-1") : ticket.residentSlotId,
                                            ticket.residentKeyId,
                                            ticket.cancelRequestedAt.isValid() ? ticket.cancelRequestedAt : QDateTime::currentDateTime(),
                                            true,
                                            QStringLiteral("删除验证发现钥匙任务仍存在，等待下一次安全窗口继续清理"));
        m_ticketCancelCoordinator->recordAccepted(taskId,
                                                  m_ticketStore->ticketById(taskId),
                                                  QStringLiteral("cancel-pending"),
                                                  QStringLiteral("删除验证发现钥匙任务仍存在，等待下一次安全窗口继续清理"));
    }
    emit statusMessage(QStringLiteral("撤销删除验证发现任务仍存在，等待后续继续清理：%1").arg(taskId));
    m_pendingCancelSystemTicketId.clear();
    m_pendingCancelKeyTaskRaw.clear();
    m_pendingCancelSessionId = 0;
    schedulePendingCancelReconcile(QStringLiteral("cancel-verify-still-exists"), 1500);
}

bool KeyManageController::autoTransferEnabled() const
{
    return ConfigManager::instance()->value("ticket/autoTransferEnabled", false).toBool();
}

bool KeyManageController::isKeyPresenceFresh(const SystemTicketDto &ticket) const
{
    if (ticket.transferState != QLatin1String("success")) return false;
    if (ticket.keyConfirmedAtMs <= 0)                     return false;
    if (m_lastQueryTasksAtMs <= 0)                        return false;
    // 此票的确认时间戳不早于最近一次 Q_TASK（给 500ms 缓冲），视为新鲜
    return ticket.keyConfirmedAtMs + 500 >= m_lastQueryTasksAtMs;
}

void KeyManageController::reconcileSystemTicketsFromKeyTasks(const QList<KeyTaskDto> &tasks)
{
    // P2：记录本次 Q_TASK 的时间戳（无论 tasks 是否为空）
    m_lastQueryTasksAtMs = QDateTime::currentMSecsSinceEpoch();

    QList<QString> confirmedTaskIds;
    for (const KeyTaskDto &task : tasks) {
        const QString taskId = taskIdFromRaw(task.taskId);
        const SystemTicketDto ticket = m_ticketStore->ticketById(taskId);
        if (!ticket.valid) {
            continue;
        }
        // 已受理撤销的票不做对账纠正（避免把撤销中的票又推回 success）
        if (!ticket.cancelState.isEmpty()
                && ticket.cancelState != QLatin1String("none")) {
            continue;
        }

        // P2：亲眼在钥匙中看到此票，收集待刷新的 taskId（循环后批量写盘）
        confirmedTaskIds.append(taskId);

        // Q_TASK 证实此任务在钥匙中 → 回包给挂起的 socket（不管当前 transferState 是否已是 success）
        m_ticketIngress->completeDeferredResponse(
            taskId, true,
            QStringLiteral("传票成功，已确认任务存在于钥匙中"));

        const bool needsReconcile = ticket.transferState == QLatin1String("received")
                || ticket.transferState == QLatin1String("auto-pending")
                || ticket.transferState == QLatin1String("sending")
                || ticket.transferState == QLatin1String("failed");
        if (!needsReconcile) {
            continue;
        }

        m_ticketStore->updateTransferState(taskId, QStringLiteral("success"));
        emit statusMessage(QStringLiteral("检测到钥匙中已存在系统票，已将本地状态纠正为\"已传票到钥匙\"：%1")
                               .arg(taskId));
    }

    // P2：一次 Q_TASK 只做一次落盘 / 一次 ticketsChanged
    m_ticketStore->refreshKeyConfirmedAt(confirmedTaskIds, m_lastQueryTasksAtMs);
}

void KeyManageController::recoverAutoTransferWhenKeyEmpty(const QList<KeyTaskDto> &tasks)
{
    if (!autoTransferEnabled()) {
        return;
    }
    if (!tasks.isEmpty()) {
        return;
    }
    if (!m_activeTransferTaskId.isEmpty() || !m_activeReturnTaskId.isEmpty()) {
        return;
    }

    bool requeued = false;
    const QList<SystemTicketDto> tickets = systemTickets();
    for (const SystemTicketDto &ticket : tickets) {
        // 已受理撤销的票不参与自动恢复
        if (!ticket.cancelState.isEmpty()
                && ticket.cancelState != QLatin1String("none")) {
            continue;
        }
        const bool shouldRecover = ticket.transferState == QLatin1String("sending")
                || ticket.transferState == QLatin1String("failed");
        if (!shouldRecover) {
            continue;
        }
        if (!isAutomaticTransferSource(ticket)) {
            continue;
        }
        if (ticket.returnState == QLatin1String("return-success")
                || ticket.returnState == QLatin1String("return-upload-success")
                || ticket.returnState == QLatin1String("return-delete-pending")
                || ticket.returnState == QLatin1String("return-delete-verifying")
                || ticket.returnState == QLatin1String("return-delete-success")
                || ticket.returnState == QLatin1String("return-handshake")
                || ticket.returnState == QLatin1String("return-log-requesting")
                || ticket.returnState == QLatin1String("return-log-receiving")
                || ticket.returnState == QLatin1String("return-uploading")) {
            continue;
        }

        m_ticketStore->updateTransferState(ticket.taskId, QStringLiteral("auto-pending"));
        emit statusMessage(QStringLiteral("钥匙中未检测到系统票，已将任务重新加入自动传票队列：%1")
                               .arg(ticket.taskId));
        requeued = true;
    }

    if (requeued) {
        tryAutoTransferPendingTicket();
    }
}

void KeyManageController::tryAutoCleanupReturnedTicket(const QList<KeyTaskDto> &tasks)
{
    if (!m_activeReturnTaskId.isEmpty()) {
        return;
    }
    if (!m_pendingDeletedSystemTicketId.isEmpty()) {
        return;
    }
    if (!m_pendingCancelSystemTicketId.isEmpty()) {
        return;
    }

    const KeySessionSnapshot snapshot = m_session->snapshot();
    if (!snapshot.connected || !snapshot.keyPresent || !snapshot.keyStable || !snapshot.sessionReady) {
        return;
    }

    for (const KeyTaskDto &task : tasks) {
        if (task.status != 0x02) {
            continue;
        }

        const QString taskId = taskIdFromRaw(task.taskId);
        const SystemTicketDto ticket = m_ticketStore->ticketById(taskId);
        if (!ticket.valid) {
            continue;
        }
        if (!ticket.cancelState.isEmpty() && ticket.cancelState != QLatin1String("none")) {
            continue;
        }
        if (ticket.returnState != QLatin1String("return-upload-success")
                && ticket.returnState != QLatin1String("return-delete-pending")
                && ticket.returnState != QLatin1String("return-delete-verifying")) {
            continue;
        }

        m_pendingDeletedSystemTicketId = taskId;
        m_pendingDeletedKeyTaskRaw = task.taskId;
        m_pendingDeleteAllowsRetransfer = false;
        m_pendingDeleteSessionId = m_keySessionId;
        m_pendingDeleteRetryCount = 0;
        m_pendingDeleteOrigin = DeleteOrigin::AutoReturnCleanup;

        CommandRequest req;
        req.id = CommandId::DeleteTask;
        req.opId = nextOpId();
        req.payload = task.taskId;
        m_session->execute(req);
        emit statusMessage(QStringLiteral("检测到回传已成功但钥匙任务仍存在，自动继续清理钥匙任务：%1")
                               .arg(taskId));
        return;
    }
}

void KeyManageController::tryAutoRefreshKeyTasksOnReady(const KeySessionSnapshot &snapshot)
{
    const bool ready = snapshot.connected
            && snapshot.keyPresent
            && snapshot.keyStable
            && snapshot.sessionReady;
    const bool becameReady = (!m_lastReadyState && ready);
    m_lastReadyState = ready;

    if (!becameReady) {
        return;
    }

    if (!m_activeTransferTaskId.isEmpty() || m_autoQuerySessionId == m_keySessionId) {
        return;
    }

    m_autoQuerySessionId = m_keySessionId;
    emit statusMessage(QStringLiteral("钥匙已就绪，自动读取钥匙票列表以检查待回传任务"));
    onQueryTasksClicked();
}

void KeyManageController::tryStartTicketReturn(const QString &taskId, bool automatic)
{
    QByteArray taskIdRaw;
    QString blockedReason;
    if (!canStartTicketReturn(taskId, automatic, &taskIdRaw, &blockedReason)) {
        if (!automatic && !blockedReason.trimmed().isEmpty()) {
            emit statusMessage(blockedReason);
        }
        return;
    }

    m_activeReturnTaskId = taskId;
    m_activeReturnTaskIdRaw = taskIdRaw;
    m_activeReturnSessionId = m_keySessionId;
    m_pendingReturnHandshakeTaskId = taskId;
    m_pendingReturnHandshakeTaskIdRaw = taskIdRaw;
    m_ticketStore->updateReturnState(taskId, QStringLiteral("return-handshake"));

    CommandRequest req;
    req.id = CommandId::SetCom;
    req.opId = nextOpId();
    m_session->execute(req);
    emit statusMessage(QStringLiteral("%1回传前重新握手：%2")
                           .arg(automatic ? QStringLiteral("自动") : QStringLiteral("执行"))
                           .arg(taskId));
}

void KeyManageController::tryAutoReturnCompletedTicket(const QList<KeyTaskDto> &tasks)
{
    if (!activeReturnChainTaskId().isEmpty()) {
        return;
    }
    if (!m_ticketReturnClient->isConfigured()) {
        return;
    }

    for (const KeyTaskDto &task : tasks) {
        if (task.status != 0x02) {
            continue;
        }

        const QString taskId = taskIdFromRaw(task.taskId);
        const SystemTicketDto ticket = m_ticketStore->ticketById(taskId);
        if (!ticket.valid) {
            continue;
        }
        // 已受理撤销的票不参与自动回传
        if (!ticket.cancelState.isEmpty()
                && ticket.cancelState != QLatin1String("none")) {
            continue;
        }
        if (!isKeyPresenceFresh(ticket)) {
            continue;
        }
        if (shouldSkipAutoReturnForState(ticket.returnState)) {
            continue;
        }

        qInfo().noquote()
                << QStringLiteral("[KeyManageController] tryAutoReturnCompletedTicket selected taskId=%1 returnState=%2 transferState=%3")
                      .arg(taskId,
                           ticket.returnState.isEmpty() ? QStringLiteral("idle") : ticket.returnState,
                           ticket.transferState);
        tryStartTicketReturn(taskId, true);
        return;
    }
}

bool KeyManageController::canStartTicketReturn(const QString &taskId,
                                               bool automatic,
                                               QByteArray *taskIdRaw,
                                               QString *blockedReason) const
{
    if (taskIdRaw) {
        taskIdRaw->clear();
    }

    auto setBlockedReason = [blockedReason](const QString &reason) {
        if (blockedReason) {
            *blockedReason = reason;
        }
    };

    if (taskId.trimmed().isEmpty()) {
        setBlockedReason(QStringLiteral("回传失败：taskId 为空"));
        return false;
    }

    const SystemTicketDto ticket = m_ticketStore->ticketById(taskId);
    if (!ticket.valid) {
        setBlockedReason(QStringLiteral("回传失败：系统票不存在"));
        return false;
    }
    if (!isKeyPresenceFresh(ticket)) {
        setBlockedReason(QStringLiteral("回传失败：该系统票尚未获得钥匙端确认"));
        return false;
    }
    if (isReturnCleanupState(ticket.returnState)) {
        setBlockedReason(QStringLiteral("该系统票已进入回传成功后的清理阶段，本次不重复上传"));
        return false;
    }
    if (ticket.returnState == QLatin1String("return-success")
            || ticket.returnState == QLatin1String("return-delete-success")) {
        setBlockedReason(QStringLiteral("该系统票已完成回传，本次不重复上传"));
        return false;
    }
    if (!m_ticketReturnClient->isConfigured()) {
        setBlockedReason(QStringLiteral("回传接口未配置，暂不发起回传"));
        return false;
    }

    const QString busyTaskId = activeReturnChainTaskId();
    if (!busyTaskId.isEmpty() && busyTaskId != taskId) {
        setBlockedReason(QStringLiteral("当前任务 %1 正在回传或清理，请等待当前链路完成后再试").arg(busyTaskId));
        return false;
    }
    if (busyTaskId == taskId && (isReturnInFlightState(ticket.returnState) || isReturnCleanupState(ticket.returnState))) {
        setBlockedReason(QStringLiteral("该系统票当前回传链仍在执行中，请稍后再试"));
        return false;
    }

    quint8 keyTaskStatus = 0;
    const QByteArray resolvedTaskIdRaw = findKeyTaskIdRaw(taskId, &keyTaskStatus);
    if (resolvedTaskIdRaw.isEmpty()) {
        setBlockedReason(QStringLiteral("钥匙内未找到该系统票，请先读取钥匙票列表"));
        return false;
    }
    if (keyTaskStatus != 0x02) {
        setBlockedReason(QStringLiteral("该系统票在钥匙中的状态未完成，当前任务完成后才允许回传"));
        return false;
    }

    if (taskIdRaw) {
        *taskIdRaw = resolvedTaskIdRaw;
    }
    setBlockedReason(QString());
    return true;
}

QString KeyManageController::activeReturnChainTaskId() const
{
    if (!m_activeReturnTaskId.isEmpty()) {
        return m_activeReturnTaskId;
    }
    if (!m_pendingReturnHandshakeTaskId.isEmpty()) {
        return m_pendingReturnHandshakeTaskId;
    }
    if (!m_pendingDeletedSystemTicketId.isEmpty()) {
        return m_pendingDeletedSystemTicketId;
    }
    return QString();
}

QByteArray KeyManageController::findKeyTaskIdRaw(const QString &taskId, quint8 *status) const
{
    for (const KeyTaskDto &task : m_lastKeyTasks) {
        if (taskIdFromRaw(task.taskId) == taskId) {
            if (status) {
                *status = task.status;
            }
            return task.taskId;
        }
    }
    if (status) {
        *status = 0xFF;
    }
    return QByteArray();
}

QString KeyManageController::taskIdFromRaw(const QByteArray &taskIdRaw)
{
    return rawTaskIdToDecimalString(taskIdRaw);
}

bool KeyManageController::taskExistsInList(const QList<KeyTaskDto> &tasks, const QString &taskId) const
{
    for (const KeyTaskDto &task : tasks) {
        if (taskIdFromRaw(task.taskId) == taskId) {
            return true;
        }
    }
    return false;
}

bool KeyManageController::hasBlockingIncompleteKeyTask(const QList<KeyTaskDto> &tasks,
                                                       QString *blockingTaskId,
                                                       const QByteArray &excludeTaskIdRaw) const
{
    if (blockingTaskId) {
        blockingTaskId->clear();
    }

    for (const KeyTaskDto &task : tasks) {
        if (!excludeTaskIdRaw.isEmpty() && task.taskId == excludeTaskIdRaw) {
            continue;
        }
        if (task.status == 0x02) {
            continue;
        }
        if (blockingTaskId) {
            *blockingTaskId = taskIdFromRaw(task.taskId);
        }
        return true;
    }
    return false;
}

bool KeyManageController::startDirectWorkbenchTransfer(const QString &taskId,
                                                       const QByteArray &jsonBytes,
                                                       int expectedSessionId,
                                                       int expectedBridgeEpoch,
                                                       QString *blockedReason)
{
    if (taskId.trimmed().isEmpty()) {
        if (blockedReason) {
            *blockedReason = QStringLiteral("传票发送失败：taskId 为空");
        }
        return false;
    }
    if (jsonBytes.isEmpty()) {
        if (blockedReason) {
            *blockedReason = QStringLiteral("传票发送失败：JSON 内容为空");
        }
        return false;
    }
    if (!m_activeTransferTaskId.isEmpty()) {
        if (blockedReason) {
            *blockedReason = QStringLiteral("当前已有其他传票任务正在执行，请稍后重试");
        }
        return false;
    }

    const KeySessionSnapshot snapshot = m_session->snapshot();
    if (expectedSessionId <= 0 || !isCurrentKeySession(expectedSessionId)) {
        if (blockedReason) {
            *blockedReason = QStringLiteral("当前钥匙会话未建立，无法发送传票");
        }
        return false;
    }
    if (expectedBridgeEpoch <= 0 || snapshot.bridgeEpoch != expectedBridgeEpoch) {
        if (blockedReason) {
            *blockedReason = QStringLiteral("当前钥匙会话已变化，无法发送传票");
        }
        return false;
    }
    if (!snapshot.connected) {
        if (blockedReason) {
            *blockedReason = QStringLiteral("串口未连接，无法发送传票");
        }
        return false;
    }
    if (!snapshot.keyPresent || !snapshot.keyStable) {
        if (blockedReason) {
            *blockedReason = QStringLiteral("钥匙未就绪，无法发送传票");
        }
        return false;
    }
    if (!snapshot.sessionReady) {
        if (blockedReason) {
            *blockedReason = QStringLiteral("钥匙通讯会话未就绪，无法发送传票");
        }
        return false;
    }
    if (snapshot.commandInFlight) {
        if (blockedReason) {
            *blockedReason = QStringLiteral("当前底层仍有协议命令在途，无法发送传票");
        }
        return false;
    }
    if (snapshot.recoveryWindowActive) {
        if (blockedReason) {
            *blockedReason = QStringLiteral("当前钥匙通讯正在恢复中，无法发送传票");
        }
        return false;
    }

    m_activeTransferTaskId = taskId;
    m_activeTransferSessionId = expectedSessionId;
    m_ticketStore->updateTransferTriggerSource(taskId, QStringLiteral("manual"));
    m_pendingTransferHandshakeTaskId = taskId;
    m_pendingTransferHandshakeJsonBytes = jsonBytes;
    m_pendingTransferHandshakeDebugChunk = ConfigManager::instance()
            ->value("ticket/debugFrameChunkSize", 0).toInt();
    qInfo().noquote()
            << QStringLiteral("[KeyManageController] startDirectWorkbenchTransfer SET_COM handshake before TICKET taskId=%1 session=%2 payloadBytes=%3")
                  .arg(taskId,
                       QString::number(expectedSessionId),
                       QString::number(jsonBytes.size()));
    CommandRequest req;
    req.id = CommandId::SetCom;
    req.opId = nextOpId();
    m_session->execute(req);
    emit statusMessage(QStringLiteral("传票前重新握手：%1").arg(taskId));
    if (blockedReason) {
        blockedReason->clear();
    }
    return true;
}

bool KeyManageController::isActiveHttpTransferTransactionFor(int sessionId, int bridgeEpoch) const
{
    return m_httpTransferTransactionActive
            && sessionId > 0
            && bridgeEpoch > 0
            && sessionId == m_httpTransferTransactionSessionId
            && bridgeEpoch == m_httpTransferTransactionBridgeEpoch;
}

void KeyManageController::setHttpTransferTransaction(const QString &taskId,
                                                     int sessionId,
                                                     int bridgeEpoch)
{
    m_httpTransferTransactionActive = true;
    m_httpTransferTransactionTaskId = taskId;
    m_httpTransferTransactionSessionId = sessionId;
    m_httpTransferTransactionBridgeEpoch = bridgeEpoch;
    m_httpTransactionSafetyTimer->start(); // 60 秒安全超时
    qInfo().noquote()
            << QStringLiteral("[KeyManageController] HTTP transfer transaction SET taskId=%1 session=%2 epoch=%3 safetyTimeout=60s")
                  .arg(taskId, QString::number(sessionId), QString::number(bridgeEpoch));
}

void KeyManageController::clearHttpTransferTransaction(bool clearActiveTransfer)
{
    const QString taskId = m_httpTransferTransactionTaskId;
    const int sessionId = m_httpTransferTransactionSessionId;

    if (clearActiveTransfer
            && !taskId.isEmpty()
            && m_activeTransferTaskId == taskId
            && m_activeTransferSessionId == sessionId) {
        m_activeTransferTaskId.clear();
        m_activeTransferSessionId = 0;
        m_pendingTransferHandshakeTaskId.clear();
        m_pendingTransferHandshakeJsonBytes.clear();
        m_pendingTransferHandshakeDebugChunk = 0;
    }

    m_httpTransferTransactionActive = false;
    m_httpTransferTransactionTaskId.clear();
    m_httpTransferTransactionSessionId = 0;
    m_httpTransferTransactionBridgeEpoch = 0;
    m_httpTransactionSafetyTimer->stop(); // 正常释放时停止安全计时器
    qInfo().noquote()
            << QStringLiteral("[KeyManageController] HTTP transfer transaction CLEARED taskId=%1 session=%2")
                  .arg(taskId.isEmpty() ? QStringLiteral("<none>") : taskId,
                       QString::number(sessionId));
}

bool KeyManageController::tryStartTicketTransfer(const QString &taskId,
                                                 bool automatic,
                                                 bool failInsteadOfDeferring,
                                                 QString *blockedReason)
{
    if (taskId.trimmed().isEmpty()) {
        qWarning().noquote()
                << QStringLiteral("[KeyManageController] tryStartTicketTransfer blocked reason=empty-taskId automatic=%1")
                      .arg(automatic ? QStringLiteral("true") : QStringLiteral("false"));
        emit statusMessage(QStringLiteral("传票发送失败：taskId 为空"));
        if (blockedReason) {
            *blockedReason = QStringLiteral("传票发送失败：taskId 为空");
        }
        return false;
    }

    if (!m_activeTransferTaskId.isEmpty()) {
        const QString reason = QStringLiteral("当前已有其他传票任务正在执行，请稍后重试");
        qInfo().noquote()
                << QStringLiteral("[KeyManageController] tryStartTicketTransfer blocked taskId=%1 reason=active-transfer activeTransfer=%2 automatic=%3")
                      .arg(taskId,
                           m_activeTransferTaskId,
                           automatic ? QStringLiteral("true") : QStringLiteral("false"));
        if (!automatic || failInsteadOfDeferring) {
            emit statusMessage(reason);
        }
        if (blockedReason) {
            *blockedReason = reason;
        }
        if (failInsteadOfDeferring) {
            m_ticketStore->updateTransferState(taskId, QStringLiteral("failed"), reason);
        }
        return false;
    }

    const KeySessionSnapshot snapshot = m_session->snapshot();
    const SystemTicketDto ticket = m_ticketStore->ticketById(taskId);

    if (ticket.valid
            && !ticket.cancelState.isEmpty()
            && ticket.cancelState != QLatin1String("none")) {
        const QString reason = QStringLiteral("该系统票已受理撤销，当前不再参与传票");
        qInfo().noquote()
                << QStringLiteral("[KeyManageController] tryStartTicketTransfer blocked taskId=%1 reason=cancel-pending cancelState=%2 automatic=%3")
                      .arg(taskId,
                           ticket.cancelState,
                           automatic ? QStringLiteral("true") : QStringLiteral("false"));
        if (blockedReason) {
            *blockedReason = reason;
        }
        emit statusMessage(reason);
        return false;
    }

    if (ticket.valid && ticket.transferState == QLatin1String("success")) {
        const QString reason = QStringLiteral("该系统票已传票到钥匙，本次不重复下发");
        qInfo().noquote()
                << QStringLiteral("[KeyManageController] tryStartTicketTransfer blocked taskId=%1 reason=already-success automatic=%2")
                      .arg(taskId,
                           automatic ? QStringLiteral("true") : QStringLiteral("false"));
        emit statusMessage(reason);
        if (blockedReason) {
            *blockedReason = reason;
        }
        return false;
    }
    if (ticket.valid && ticket.transferState == QLatin1String("sending")) {
        const QString reason = QStringLiteral("该系统票正在传票中，请稍后再试");
        qInfo().noquote()
                << QStringLiteral("[KeyManageController] tryStartTicketTransfer blocked taskId=%1 reason=already-sending automatic=%2")
                      .arg(taskId,
                           automatic ? QStringLiteral("true") : QStringLiteral("false"));
        emit statusMessage(reason);
        if (blockedReason) {
            *blockedReason = reason;
        }
        return false;
    }

    if (!snapshot.connected) {
        const QString reason = automatic
                ? QStringLiteral("系统票已入池，等待串口连接后自动传票")
                : QStringLiteral("串口未连接，无法发送传票");
        qInfo().noquote()
                << QStringLiteral("[KeyManageController] tryStartTicketTransfer blocked taskId=%1 reason=serial-disconnected automatic=%2")
                      .arg(taskId,
                           automatic ? QStringLiteral("true") : QStringLiteral("false"));
        if (automatic) {
            m_ticketStore->updateTransferState(taskId, QStringLiteral("auto-pending"));
            emit statusMessage(reason);
        } else {
            emit statusMessage(reason);
            if (failInsteadOfDeferring) {
                m_ticketStore->updateTransferState(taskId, QStringLiteral("failed"), reason);
            }
        }
        if (blockedReason) {
            *blockedReason = reason;
        }
        return false;
    }

    if (!snapshot.keyPresent || !snapshot.keyStable) {
        const QString reason = automatic
                ? QStringLiteral("系统票已入池，等待钥匙就绪后自动传票")
                : QStringLiteral("钥匙未就绪，无法发送传票");
        qInfo().noquote()
                << QStringLiteral("[KeyManageController] tryStartTicketTransfer blocked taskId=%1 reason=key-not-ready keyPresent=%2 keyStable=%3 automatic=%4")
                      .arg(taskId,
                           snapshot.keyPresent ? QStringLiteral("true") : QStringLiteral("false"),
                           snapshot.keyStable ? QStringLiteral("true") : QStringLiteral("false"),
                           automatic ? QStringLiteral("true") : QStringLiteral("false"));
        if (automatic) {
            m_ticketStore->updateTransferState(taskId, QStringLiteral("auto-pending"));
            emit statusMessage(reason);
        } else {
            emit statusMessage(reason);
            if (failInsteadOfDeferring) {
                m_ticketStore->updateTransferState(taskId, QStringLiteral("failed"), reason);
            }
        }
        if (blockedReason) {
            *blockedReason = reason;
        }
        return false;
    }

    if (!snapshot.sessionReady) {
        const QString reason = automatic
                ? QStringLiteral("系统票已入池，等待握手完成后自动传票")
                : QStringLiteral("钥匙通讯会话未就绪，无法发送传票");
        qInfo().noquote()
                << QStringLiteral("[KeyManageController] tryStartTicketTransfer blocked taskId=%1 reason=session-not-ready automatic=%2 currentSession=%3")
                      .arg(taskId,
                           automatic ? QStringLiteral("true") : QStringLiteral("false"),
                           QString::number(m_keySessionId));
        if (automatic) {
            m_ticketStore->updateTransferState(taskId, QStringLiteral("auto-pending"));
            emit statusMessage(reason);
        } else {
            emit statusMessage(reason);
            if (failInsteadOfDeferring) {
                m_ticketStore->updateTransferState(taskId, QStringLiteral("failed"), reason);
            }
        }
        if (blockedReason) {
            *blockedReason = reason;
        }
        return false;
    }

    if (snapshot.commandInFlight) {
        const QString reason = automatic
                ? QStringLiteral("系统票已入池，等待当前协议命令完成后自动传票")
                : QStringLiteral("当前底层仍有协议命令在途，无法发送传票");
        qInfo().noquote()
                << QStringLiteral("[KeyManageController] tryStartTicketTransfer blocked taskId=%1 reason=command-in-flight automatic=%2")
                      .arg(taskId,
                           automatic ? QStringLiteral("true") : QStringLiteral("false"));
        if (automatic) {
            m_ticketStore->updateTransferState(taskId, QStringLiteral("auto-pending"));
            emit statusMessage(reason);
        } else {
            emit statusMessage(reason);
            if (failInsteadOfDeferring) {
                m_ticketStore->updateTransferState(taskId, QStringLiteral("failed"), reason);
            }
        }
        if (blockedReason) {
            *blockedReason = reason;
        }
        return false;
    }

    if (snapshot.recoveryWindowActive) {
        const QString reason = automatic
                ? QStringLiteral("系统票已入池，等待钥匙通讯恢复后自动传票")
                : QStringLiteral("当前钥匙通讯正在恢复中，无法发送传票");
        qInfo().noquote()
                << QStringLiteral("[KeyManageController] tryStartTicketTransfer blocked taskId=%1 reason=recovery-window automatic=%2")
                      .arg(taskId,
                           automatic ? QStringLiteral("true") : QStringLiteral("false"));
        if (automatic) {
            m_ticketStore->updateTransferState(taskId, QStringLiteral("auto-pending"));
            emit statusMessage(reason);
        } else {
            emit statusMessage(reason);
            if (failInsteadOfDeferring) {
                m_ticketStore->updateTransferState(taskId, QStringLiteral("failed"), reason);
            }
        }
        if (blockedReason) {
            *blockedReason = reason;
        }
        return false;
    }

    const QByteArray jsonBytes = m_ticketStore->rawJsonById(taskId);
    if (jsonBytes.isEmpty()) {
        const QString reason = QStringLiteral("选中系统票缺少原始 JSON，无法发送传票");
        qWarning().noquote()
                << QStringLiteral("[KeyManageController] tryStartTicketTransfer blocked taskId=%1 reason=raw-json-missing automatic=%2")
                      .arg(taskId,
                           automatic ? QStringLiteral("true") : QStringLiteral("false"));
        m_ticketStore->updateTransferState(taskId, QStringLiteral("failed"),
                                           QStringLiteral("raw json missing"));
        emit statusMessage(reason);
        if (blockedReason) {
            *blockedReason = reason;
        }
        return false;
    }

    m_activeTransferTaskId = taskId;
    m_activeTransferSessionId = m_keySessionId;
    m_ticketStore->updateTransferTriggerSource(taskId,
                                               automatic ? QStringLiteral("automatic")
                                                         : QStringLiteral("manual"));
    m_pendingTransferHandshakeTaskId = taskId;
    m_pendingTransferHandshakeJsonBytes = jsonBytes;
    m_pendingTransferHandshakeDebugChunk = ConfigManager::instance()
            ->value("ticket/debugFrameChunkSize", 0).toInt();
    qInfo().noquote()
            << QStringLiteral("[KeyManageController] tryStartTicketTransfer SET_COM handshake before TICKET taskId=%1 automatic=%2 session=%3 payloadBytes=%4")
                  .arg(taskId,
                       automatic ? QStringLiteral("true") : QStringLiteral("false"),
                       QString::number(m_keySessionId),
                       QString::number(jsonBytes.size()));
    m_ticketStore->updateTransferState(taskId, QStringLiteral("sending"));
    CommandRequest req;
    req.id = CommandId::SetCom;
    req.opId = nextOpId();
    m_session->execute(req);
    emit statusMessage(QStringLiteral("%1传票前重新握手：%2")
                           .arg(automatic ? QStringLiteral("自动") : QStringLiteral("执行"))
                           .arg(taskId));
    if (blockedReason) {
        blockedReason->clear();
    }
    return true;
}

void KeyManageController::tryAutoTransferPendingTicket()
{
    if (m_suppressSessionAutomation) {
        qInfo().noquote()
                << QStringLiteral("[KeyManageController] tryAutoTransferPendingTicket skipped reason=key-port-apply-in-progress epoch=%1")
                      .arg(QString::number(m_keyPortApplyEpoch));
        return;
    }

    if (m_httpTransferTransactionActive) {
        qInfo().noquote()
                << QStringLiteral("[KeyManageController] tryAutoTransferPendingTicket skipped reason=http-transfer-transaction-active taskId=%1 session=%2 epoch=%3")
                      .arg(m_httpTransferTransactionTaskId.isEmpty()
                                   ? QStringLiteral("<none>")
                                   : m_httpTransferTransactionTaskId,
                           QString::number(m_httpTransferTransactionSessionId),
                           QString::number(m_httpTransferTransactionBridgeEpoch));
        return;
    }

    if (!autoTransferEnabled()) {
        qInfo().noquote() << QStringLiteral("[KeyManageController] tryAutoTransferPendingTicket skipped reason=auto-transfer-disabled");
        return;
    }

    if (!m_activeTransferTaskId.isEmpty()
            || !m_activeReturnTaskId.isEmpty()
            || !m_pendingReturnHandshakeTaskId.isEmpty()
            || !m_pendingDeletedSystemTicketId.isEmpty()
            || !m_pendingCancelSystemTicketId.isEmpty()) {
        // busy 状态会被高频触发，降级为 debug 避免日志刷屏
        qDebug().noquote()
                << QStringLiteral("[KeyManageController] tryAutoTransferPendingTicket skipped reason=busy activeTransfer=%1 activeReturn=%2 pendingHandshake=%3 pendingDelete=%4 pendingCancel=%5")
                      .arg(m_activeTransferTaskId.isEmpty() ? QStringLiteral("<none>") : m_activeTransferTaskId,
                           m_activeReturnTaskId.isEmpty() ? QStringLiteral("<none>") : m_activeReturnTaskId,
                           m_pendingReturnHandshakeTaskId.isEmpty() ? QStringLiteral("<none>") : m_pendingReturnHandshakeTaskId,
                           m_pendingDeletedSystemTicketId.isEmpty() ? QStringLiteral("<none>") : m_pendingDeletedSystemTicketId,
                           m_pendingCancelSystemTicketId.isEmpty() ? QStringLiteral("<none>") : m_pendingCancelSystemTicketId);
        return;
    }

    const QList<SystemTicketDto> tickets = systemTickets();
    for (const SystemTicketDto &ticket : tickets) {
        // 已受理撤销的票不参与自动传票
        if (!ticket.cancelState.isEmpty()
                && ticket.cancelState != QLatin1String("none")) {
            continue;
        }
        if (ticket.transferState == QLatin1String("received")
                || ticket.transferState == QLatin1String("auto-pending")) {
            qInfo().noquote()
                    << QStringLiteral("[KeyManageController] tryAutoTransferPendingTicket selected taskId=%1 transferState=%2 returnState=%3 poolSize=%4")
                          .arg(ticket.taskId,
                               ticket.transferState,
                               ticket.returnState,
                               QString::number(tickets.size()));
            emit statusMessage(QStringLiteral("发现可自动传票任务：%1").arg(ticket.taskId));
            tryStartTicketTransfer(ticket.taskId, true);
            return;
        }
    }
    // 空池或无符合条件的票时不打日志，避免刷屏（每次 KeyStateChanged 都会触发本函数）
}

bool KeyManageController::hasPendingApplySensitiveBusiness() const
{
    if (m_ticketCancelCoordinator->hasPendingWork()) {
        return true;
    }
    if (!m_activeTransferTaskId.isEmpty()
            || !m_activeReturnTaskId.isEmpty()
            || !m_pendingReturnHandshakeTaskId.isEmpty()
            || !m_pendingDeletedSystemTicketId.isEmpty()
            || !m_pendingCancelSystemTicketId.isEmpty()) {
        return true;
    }

    const QList<SystemTicketDto> tickets = systemTickets();
    for (const SystemTicketDto &ticket : tickets) {
        if (ticket.transferState == QLatin1String("sending")
                || ticket.transferState == QLatin1String("auto-pending")
                || ticket.returnState == QLatin1String("return-interrupted-retryable")
                || ticket.returnState == QLatin1String("return-handshake")
                || ticket.returnState == QLatin1String("return-log-requesting")
                || ticket.returnState == QLatin1String("return-log-receiving")
                || ticket.returnState == QLatin1String("return-uploading")
                || ticket.returnState == QLatin1String("return-delete-pending")
                || ticket.returnState == QLatin1String("return-delete-verifying")
                || ticket.returnState == QLatin1String("return-upload-success")
                || ticket.cancelState == QLatin1String("cancel-accepted")
                || ticket.cancelState == QLatin1String("cancel-pending")
                || ticket.cancelState == QLatin1String("cancel-executing")) {
            return true;
        }
    }

    return false;
}

bool KeyManageController::isKeyPortApplyBusy(QString *reason) const
{
    const KeySessionSnapshot snapshot = m_session->snapshot();

    QString busyReason;
    if (m_keyPortApplyInProgress) {
        busyReason = QStringLiteral("当前正在应用钥匙串口配置，请等待本次切换完成");
    } else if (hasPendingApplySensitiveBusiness()) {
        busyReason = QStringLiteral("当前钥匙业务尚未收口，包含传票/回传/删除验证中的链路，请稍后再试");
    } else if (snapshot.commandInFlight) {
        busyReason = QStringLiteral("当前底层仍有协议命令在途，请等待当前串口命令完成后再切换");
    } else if (m_keyProvisioning->isBusy()) {
        busyReason = QStringLiteral("当前正在执行 INIT 或下载 RFID，请等待当前取数与下发完成");
    } else if (snapshot.recoveryWindowActive) {
        busyReason = QStringLiteral("当前处于自动恢复窗口，暂不允许切换钥匙串口");
    } else if (snapshot.connected && snapshot.keyPresent && !snapshot.sessionReady) {
        busyReason = QStringLiteral("当前钥匙会话正在握手或等待通信确认，暂不允许切换钥匙串口");
    } else if (m_autoQuerySessionId == m_keySessionId && m_keySessionId > 0) {
        busyReason = QStringLiteral("当前钥匙刚进入 ready 边沿自动 Q_TASK 窗口，暂不允许切换钥匙串口");
    }

    if (reason) {
        *reason = busyReason;
    }
    return !busyReason.isEmpty();
}

void KeyManageController::refreshKeySerialApplyStatus(const QString &overrideState,
                                                      const QString &overrideMessage)
{
    const KeySessionSnapshot snapshot = m_session->snapshot();
    const QString configuredPort = ConfigManager::instance()->keySerialPort().trimmed();
    const QString runtimePort = snapshot.portName.trimmed();

    QString nextState = overrideState;
    QString nextMessage = overrideMessage;

    if (nextState.isEmpty()) {
        if (m_keyPortApplyInProgress) {
            nextState = QStringLiteral("applying");
        } else if (!configuredPort.isEmpty()
                   && configuredPort == runtimePort
                   && snapshot.connected
                   && (!snapshot.keyPresent || (snapshot.sessionReady && snapshot.protocolHealthy))) {
            nextState = QStringLiteral("applied");
        } else if (m_keySerialApplyState == QLatin1String("apply_failed")) {
            nextState = QStringLiteral("apply_failed");
        } else {
            nextState = QStringLiteral("saved_not_applied");
        }
    }

    if (nextMessage.isEmpty()) {
        if (nextState == QLatin1String("applying")) {
            const QString targetPort = m_keyPortApplyTargetPort.isEmpty() ? configuredPort : m_keyPortApplyTargetPort;
            if (!targetPort.isEmpty() && runtimePort == targetPort && snapshot.connected) {
                if (snapshot.keyPresent && (!snapshot.sessionReady || !snapshot.protocolHealthy)) {
                    nextMessage = QStringLiteral("新串口 %1 已打开，正在等待 SET_COM/通讯确认完成").arg(targetPort);
                } else {
                    nextMessage = QStringLiteral("新串口 %1 已接管运行态，等待最终状态确认").arg(targetPort);
                }
            } else {
                nextMessage = QStringLiteral("正在应用钥匙串口配置，请等待切换结果");
            }
        } else if (nextState == QLatin1String("applied")) {
            if (configuredPort.isEmpty()) {
                nextMessage = QStringLiteral("当前未配置钥匙串口");
            } else if (!snapshot.keyPresent) {
                nextMessage = QStringLiteral("运行态已切换到 %1，当前未检测到钥匙，待放稳后再完成通讯确认").arg(configuredPort);
            } else {
                nextMessage = QStringLiteral("配置态与运行态一致，当前运行串口：%1").arg(configuredPort);
            }
        } else if (nextState == QLatin1String("apply_failed")) {
            nextMessage = m_keySerialApplyMessage.isEmpty()
                    ? QStringLiteral("配置已保存，但尚未成功应用到运行态")
                    : m_keySerialApplyMessage;
        } else {
            nextMessage = QStringLiteral("配置已保存，当前运行态仍锚定旧串口，需手动\"应用并重连\"");
        }
    }

    m_keySerialApplyState = nextState;
    m_keySerialApplyMessage = nextMessage;
    emit keySerialApplyStatusChanged(keySerialApplyStatus());
}

bool KeyManageController::isCurrentKeySession(int sessionId) const
{
    return sessionId > 0 && sessionId == m_keySessionId;
}

void KeyManageController::openNewKeySession()
{
    m_keySessionId = ++m_keySessionSeed;
    m_acceptedBridgeEpoch = m_session->snapshot().bridgeEpoch;
    m_autoQuerySessionId = 0;
    emit statusMessage(QStringLiteral("钥匙会话已建立：session=%1").arg(m_keySessionId));
}

void KeyManageController::invalidateCurrentKeySession(const QString &reason)
{
    if (m_keySessionId <= 0) {
        return;
    }

    const int expiredSessionId = m_keySessionId;
    m_keySessionId = 0;
    m_autoQuerySessionId = 0;
    // 会话失效时清空钥匙任务缓存，防止陈旧数据影响下次 success 状态判断
    m_lastKeyTasks.clear();
    m_pendingDeleteRetryCount = 0;

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

void KeyManageController::tryAutoConnectKeyPort()
{
    const int cfgBaud = ConfigManager::instance()->baudRate();
    const int baud = (cfgBaud > 0) ? cfgBaud : 115200;
    const QString configPort = ConfigManager::instance()->keySerialPort().trimmed();

    const KeySessionSnapshot snap = m_session->snapshot();
    QStringList candidates;
    if (m_keySerialApplyState == QLatin1String("saved_not_applied")) {
        if (!snap.verifiedPortName.isEmpty()) {
            candidates << snap.verifiedPortName;
        } else if (!snap.portName.trimmed().isEmpty()) {
            candidates << snap.portName.trimmed();
        }
    } else if (!configPort.isEmpty()) {
        candidates << configPort;
    } else if (!snap.verifiedPortName.isEmpty()) {
        candidates << snap.verifiedPortName;
    }

    if (candidates.isEmpty()) {
        emit statusMessage(QStringLiteral("未检测到可用串口"));
        emit sessionSnapshotChanged(m_session->snapshot());
        return;
    }

    for (const QString &port : candidates) {
        if (m_session->connectPort(port, baud)) {
            if (m_keySerialApplyState == QLatin1String("saved_not_applied")) {
                emit statusMessage(QStringLiteral("串口继续锚定旧运行端口: %1").arg(port));
            } else if (!configPort.isEmpty()) {
                emit statusMessage(QStringLiteral("串口已连接(配置端口): %1").arg(port));
            } else {
                emit statusMessage(QStringLiteral("串口已连接(已验证端口): %1").arg(port));
            }
            emit sessionSnapshotChanged(m_session->snapshot());
            refreshKeySerialApplyStatus();
            return;
        }
    }

    emit statusMessage(QStringLiteral("串口连接失败，请检查底座或配置"));
    emit sessionSnapshotChanged(m_session->snapshot());
    refreshKeySerialApplyStatus();
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

void KeyManageController::tryAutoSeat1MaintenanceAfterTasksUpdated(
        const KeySessionSnapshot &snapshot,
        const QList<KeyTaskDto> &tasks)
{
    Q_UNUSED(tasks);
    if (!snapshot.keyPresent || !snapshot.keyStable || !snapshot.sessionReady) {
        return;
    }
    if (snapshot.commandInFlight) {
        return;
    }
    if (!m_activeTransferTaskId.isEmpty()
            || !m_activeReturnTaskId.isEmpty()
            || !m_pendingReturnHandshakeTaskId.isEmpty()
            || !m_pendingDeletedSystemTicketId.isEmpty()
            || !m_pendingCancelSystemTicketId.isEmpty()) {
        return;
    }
    m_session->startAuxSequence(0x01,
                                static_cast<int>(KeySerialClient::AuxSequenceBatteryThenSync),
                                static_cast<int>(KeySerialClient::AuxOriginAutoAfterTasks),
                                QStringLiteral("auto_after_tasks"));
}

void KeyManageController::onAutoBatteryRefreshTimeout()
{
    const KeySessionSnapshot snapshot = m_session->snapshot();
    if (!snapshot.connected || !snapshot.keyPresent
            || !snapshot.keyStable || !snapshot.sessionReady) {
        return;
    }
    if (snapshot.commandInFlight) {
        return;
    }
    if (!m_activeTransferTaskId.isEmpty()
            || !m_activeReturnTaskId.isEmpty()
            || !m_pendingReturnHandshakeTaskId.isEmpty()
            || !m_pendingDeletedSystemTicketId.isEmpty()
            || !m_pendingCancelSystemTicketId.isEmpty()) {
        return;
    }
    m_session->startAuxSequence(0x01,
                                static_cast<int>(KeySerialClient::AuxSequenceBatteryOnly),
                                static_cast<int>(KeySerialClient::AuxOriginPeriodicRefresh),
                                QStringLiteral("periodic_refresh"));
}

void KeyManageController::onAutoBatteryRefreshTimeoutB()
{
    const KeySessionSnapshot snapshot = m_session->snapshot();
    // B座刷新完全独立于 A座业务：只要 B座在位且无命令在途即执行
    if (!snapshot.connected || !snapshot.bKeyPresent) {
        return;
    }
    if (snapshot.commandInFlight) {
        return;
    }
    m_session->startAuxSequence(0x02,
                                static_cast<int>(KeySerialClient::AuxSequenceBatteryOnly),
                                static_cast<int>(KeySerialClient::AuxOriginPeriodicRefresh),
                                QStringLiteral("b_periodic_refresh"));
}
