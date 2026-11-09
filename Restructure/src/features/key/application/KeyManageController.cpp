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

#include "core/AppContext.h"
#include "core/ConfigManager.h"
#include "features/key/application/KeyProvisioningService.h"
#include "features/key/application/KeyMaintenanceService.h"
#include "features/key/application/KeyPortRuntimeCoordinator.h"
#include "features/key/application/KeyTaskObservationService.h"
#include "features/key/application/KeyTaskUpdateUseCase.h"
#include "features/key/application/KeyManageObservabilityService.h"
#include "features/key/application/KeyManageSelectionService.h"
#include "features/key/application/TicketCancelWorkflowService.h"
#include "features/key/application/TicketDeleteWorkflowService.h"
#include "features/key/application/TicketTransferWorkflowService.h"
#include "features/key/application/TicketReturnWorkflowService.h"
#include "features/key/application/TicketStore.h"
#include "features/key/application/TicketIngressService.h"
#include "features/key/application/TicketReturnUploadService.h"
#include "features/key/application/AdminTicketTerminationService.h"
#include "features/key/application/TicketTransferUseCase.h"
#include "features/key/application/TicketCancelUseCase.h"
#include "features/key/application/TicketDeleteUseCase.h"
#include "features/key/application/VerifiedKeyTaskRemoval.h"
#include "features/key/application/CaptureTicketWorkflowService.h"
#include "features/key/application/CaptureTicketHttpClient.h"
#include "features/key/protocol/KeyProtocolDefs.h"

namespace
{

        bool isSessionReadySnapshot(const KeySessionSnapshot &snapshot)
        {
                return snapshot.connected && snapshot.keyPresent && snapshot.keyStable && snapshot.sessionReady;
        }

        // 兼容根对象和 data 对象两种票据包装；取字符串 taskId 供后续准入与流程关联。
        // 解析失败或字段无效时返回空串，由业务准入统一决定如何拒绝。
        QString extractTaskIdFromWorkbenchJson(const QByteArray &jsonBytes)
        {
                QJsonParseError pe;
                const QJsonDocument doc = QJsonDocument::fromJson(jsonBytes, &pe);
                if (doc.isNull() || !doc.isObject())
                {
                        return QString();
                }

                const QJsonObject root = doc.object();
                const QJsonObject ticketObj = (root.contains("data") && root.value("data").isObject())
                                                  ? root.value("data").toObject()
                                                  : root;
                return ticketObj.value("taskId").toString().trimmed();
        }

}

bool KeyManageDependencies::isComplete() const
{
        return session && logManager && ticketStore && ticketTransferWorkflow && ticketReturnWorkflow && ticketIngress && returnUploadService && keyProvisioning && ticketCancelWorkflow && ticketDeleteWorkflow && adminTerminationService && maintenanceService && keyPortRuntime && keyTaskObservation && observability && selection && captureWorkflow && captureHttpClient;
}

// Receives an already assembled module so business orchestration never
// chooses concrete serial, HTTP, authentication, or storage implementations.
KeyManageController::KeyManageController(
    const KeyManageDependencies &dependencies,
    QObject *parent)
    : QObject(parent), m_session(dependencies.session), m_logManager(dependencies.logManager), m_ticketStore(dependencies.ticketStore), m_ticketTransferWorkflow(dependencies.ticketTransferWorkflow), m_ticketReturnWorkflow(dependencies.ticketReturnWorkflow), m_ticketIngress(dependencies.ticketIngress), m_returnUploadService(dependencies.returnUploadService), m_keyProvisioning(dependencies.keyProvisioning), m_ticketCancelWorkflow(dependencies.ticketCancelWorkflow), m_ticketDeleteWorkflow(dependencies.ticketDeleteWorkflow), m_adminTerminationService(dependencies.adminTerminationService), m_maintenanceService(dependencies.maintenanceService), m_keyPortRuntime(dependencies.keyPortRuntime), m_keyTaskObservation(dependencies.keyTaskObservation), m_observability(dependencies.observability), m_selection(dependencies.selection), m_captureWorkflow(dependencies.captureWorkflow), m_captureHttpClient(dependencies.captureHttpClient), m_nextOpId(1), m_httpTransactionSafetyTimer(new QTimer(this))
{
        Q_ASSERT(dependencies.isComplete());
        m_maintenanceService->setBusinessBusyProvider([this]()
                                                      { return m_ticketTransferWorkflow->hasActiveOperation() || m_ticketReturnWorkflow->hasReturnWork() || m_ticketDeleteWorkflow->hasActiveOperation() || m_ticketCancelWorkflow->hasActiveDelete() || m_captureWorkflow->hasForegroundOperation(); });
        m_httpTransactionSafetyTimer->setSingleShot(true);
        m_httpTransactionSafetyTimer->setInterval(60000); // 60 秒安全超时
        connect(m_httpTransactionSafetyTimer, &QTimer::timeout, this, [this]()
                {
        const QString taskId =
                m_ticketTransferWorkflow->activeWorkbenchTaskId();
        qWarning().noquote()
                << QStringLiteral("[KeyManageController] HTTP transfer transaction safety timeout taskId=%1 session=%2 epoch=%3")
                      .arg(taskId.isEmpty() ? QStringLiteral("<none>") : taskId,
                           QString::number(m_ticketTransferWorkflow->activeWorkbenchSessionId()),
                           QString::number(m_ticketTransferWorkflow->activeWorkbenchBridgeEpoch()));
        const QString reason = QStringLiteral(
                    "\u4F20\u7968\u786E\u8BA4\u8D85\u65F6\uFF0C"
                    "\u5DF2\u7ED3\u675F\u672C\u6B21\u5DE5\u4F5C\u53F0\u8BF7\u6C42");
        applyTransferEffect(
                    m_ticketTransferWorkflow->expireWorkbenchTransaction(reason)); });

        connect(m_session, &IKeySessionService::eventOccurred,
                this, &KeyManageController::handleSessionEvent);
        connect(m_session, &IKeySessionService::logItemEmitted,
                this, &KeyManageController::handleLogItem);
        connect(m_ticketStore, &TicketStore::ticketsChanged,
                this, &KeyManageController::handleSystemTicketsChanged);
        connect(m_ticketIngress, &TicketIngressService::httpServerLogAppended,
                this, &KeyManageController::handleHttpServerLog);
        connect(m_ticketIngress, &TicketIngressService::cancelDeferredRequestExpired,
                this, &KeyManageController::handleCancelDeferredRequestExpired);
        // 构造时将处理函数绑定到当前 Controller；Ingress 收齐请求 body 后才调用它，
        // 将原始 JSON 交给 handleTicketIngress，并取得业务受理结果。
        m_ticketIngress->setIngressHandler(
            [this](const QByteArray &jsonBytes)
            {
                    return handleTicketIngress(jsonBytes);
            });

        // ── cancelTicket ingress handler ──
        m_ticketIngress->setCancelHandler(
            [this](const QString &taskId)
            {
                    return handleCancelIngress(taskId);
            });
        connect(m_returnUploadService, &TicketReturnUploadService::logAppended,
                this, &KeyManageController::handleHttpClientLog);
        connect(m_returnUploadService, &TicketReturnUploadService::uploadSucceeded,
                this, &KeyManageController::handleReturnUploadSucceeded);
        connect(m_returnUploadService, &TicketReturnUploadService::uploadFailed,
                this, &KeyManageController::handleReturnUploadFailed);
        connect(m_captureHttpClient, &CaptureTicketHttpClient::logAppended,
                this, &KeyManageController::handleHttpClientLog);
        connect(m_captureHttpClient, &CaptureTicketHttpClient::claimSucceeded,
                this, &KeyManageController::handleCaptureClaimSucceeded);
        connect(m_captureHttpClient, &CaptureTicketHttpClient::claimFailed,
                this, &KeyManageController::handleCaptureClaimFailed);
        connect(m_captureHttpClient, &CaptureTicketHttpClient::finishSucceeded,
                this, &KeyManageController::handleCaptureFinishSucceeded);
        connect(m_captureHttpClient, &CaptureTicketHttpClient::finishFailed,
                this, &KeyManageController::handleCaptureFinishFailed);
        connect(m_adminTerminationService, &AdminTicketTerminationService::logAppended,
                this, &KeyManageController::handleHttpClientLog);
        connect(m_adminTerminationService, &AdminTicketTerminationService::statusMessage,
                this, &KeyManageController::statusMessage);
        connect(m_adminTerminationService,
                &AdminTicketTerminationService::terminationCompleted,
                this, [this](const QString &)
                { emit workbenchRefreshRequested(); });
        connect(m_keyProvisioning, &KeyProvisioningService::logAppended,
                this, &KeyManageController::handleHttpClientLog);
        connect(m_keyProvisioning, &KeyProvisioningService::initPayloadReady,
                this, &KeyManageController::handleInitPayloadReady);
        connect(m_keyProvisioning, &KeyProvisioningService::rfidPayloadReady,
                this, &KeyManageController::handleRfidPayloadReady);
        connect(m_keyProvisioning, &KeyProvisioningService::requestFailed,
                this, &KeyManageController::handleProvisionRequestFailed);
        connect(m_logManager, &SerialLogManager::overflowDropped, this, [this]()
                { emit statusMessage(QStringLiteral("日志达到上限，已自动丢弃旧日志")); });
}

// 汇集票据和当前运行状态交给 Transfer Workflow；执行其动作并返回 HTTP 受理结果。
// accepted 表示业务受理，设备成功仍须由后续 Q_TASK 确认。
TicketIngressService::IngressResult KeyManageController::handleTicketIngress(
    const QByteArray &jsonBytes)
{
        // HTTP 层只保证 body 已收齐；在此提取业务身份，保留原始 JSON 供入池和后续编码。
        const QString taskId = extractTaskIdFromWorkbenchJson(jsonBytes);
        // 读取协议层当前记录的连接、在位和就绪等状态；snapshot() 本身不发起设备查询。
        const KeySessionSnapshot ingressSnapshot = m_session->snapshot();

        // 将票据与准入所需事实放入一次调用的输入结构；跨事件的流程状态由 Workflow 另行保存。
        TicketTransferWorkbenchRequest request;
        // 票据身份和原始内容。
        request.taskId = taskId;
        request.jsonBytes = jsonBytes;
        // 记录该任务的撤销状态及业务入口读取的钥匙状态，供准入检查冲突和就绪条件。
        request.trackedCancelState = m_ticketCancelWorkflow->trackedState(taskId);
        request.ingressSession = ingressSnapshot;
        // 再取一份启动检查使用的状态；Workflow 会比较桥接身份，避免沿用已变化的连接。
        request.startSession = m_session->snapshot();
        // 读取已保存的设备任务列表：是否非空、是否包含目标任务；此处不发起新的 Q_TASK。
        request.hasKeyTaskData = m_keyTaskObservation->hasTasks();
        request.taskStillInKey = m_keyTaskObservation->containsTaskId(taskId);
        // Return/Delete/Cancel/Capture 等冲突业务是否正占用钥匙，以及自动查询是否繁忙。
        request.returnOrDeleteBusy =
            m_ticketReturnWorkflow->hasReturnWork() || m_ticketDeleteWorkflow->hasActiveOperation() || m_ticketCancelWorkflow->hasPendingWork() || m_ticketCancelWorkflow->hasActiveDelete() || m_captureWorkflow->hasForegroundOperation();
        request.autoQueryBusy =
            m_keyTaskObservation->automaticQueryActive(m_keySessionId);
        // 保存本次判断所属的本机会话编号；后续流程据此关联钥匙会话，不是票据 taskId。
        request.currentKeySessionId = m_keySessionId;
        // 分帧配置随原始 JSON 向后传递；0 表示不覆盖后续 builder 的默认分块大小。
        request.debugFrameChunkSize = ConfigManager::instance()
                                          ->value("ticket/debugFrameChunkSize", 0)
                                          .toInt();

        // Workflow 根据票据和当前事实作出接收/拒绝及下一步动作的决定；它不直接进行串口通信。
        const TicketTransferWorkbenchOutcome outcome =
            m_ticketTransferWorkflow->admitWorkbench(request);
        TicketIngressService::IngressResult result;
        result.statusCode = 200;
        result.accepted = outcome.accepted;
        result.deferred = outcome.deferred;
        result.message = outcome.message;
        result.taskId = outcome.includeTaskId ? outcome.taskId : QString();

        // effect 是 Workflow 告诉 Controller“下一步要执行什么”的动作说明；真正执行仍由 Controller 发起。
        if (outcome.effect.handled)
        {
                applyTransferEffect(outcome.effect);
        }

        qInfo().noquote()
            << QStringLiteral("[KeyManageController] ticket ingress taskId=%1 accepted=%2 ingressReason=%3 startReason=%4")
                   .arg(taskId.isEmpty() ? QStringLiteral("<empty>") : taskId,
                        outcome.accepted ? QStringLiteral("true")
                                         : QStringLiteral("false"),
                        QString::number(static_cast<int>(
                            outcome.ingressDecision.reason)),
                        QString::number(static_cast<int>(
                            outcome.startDecision.reason)));
        return result;
}

TicketIngressService::IngressResult KeyManageController::handleCancelIngress(
    const QString &taskId)
{
        const KeySessionSnapshot snapshot = m_session->snapshot();
        QString blockedReason;
        const bool canProcessNow = canProcessCancelNow(&blockedReason);

        TicketCancelIngressObservation observation;
        observation.taskId = taskId;
        observation.keyReady = snapshot.connected && snapshot.keyPresent && snapshot.sessionReady;
        observation.hasTaskList = m_keyTaskObservation->hasTasks();
        observation.taskInKey = m_keyTaskObservation->containsTaskId(taskId);
        observation.canProcessNow = canProcessNow;

        const TicketCancelIngressOutcome outcome =
            m_ticketCancelWorkflow->handleIngress(observation);
        const TicketCancelIngressDecision &decision = outcome.decision;

        TicketIngressService::IngressResult result;
        result.statusCode = 200;
        result.taskId = taskId;
        result.accepted = decision.accepted;
        result.deferred = decision.deferred;
        result.message = decision.message;

        if (outcome.scheduleReconcile)
        {
                schedulePendingCancelReconcile(
                    QStringLiteral("cancel-request-accepted"), 150);
        }

        m_observability->writeBusinessEvent(
            QStringLiteral("termination"),
            QStringLiteral("accepted"),
            decision.accepted ? QStringLiteral("accepted")
                              : QStringLiteral("rejected"),
            taskId,
            m_keySessionId,
            -1,
            QStringLiteral("decision-reason-%1")
                .arg(static_cast<int>(decision.reason)),
            decision.accepted ? BusinessLogSeverity::Info
                              : BusinessLogSeverity::Warning);

        qInfo().noquote()
            << QStringLiteral("[KeyManageController] cancel ingress taskId=%1 reason=%2 action=%3 accepted=%4 blockedReason=%5")
                   .arg(taskId.isEmpty() ? QStringLiteral("<empty>") : taskId,
                        QString::number(static_cast<int>(decision.reason)),
                        QString::number(static_cast<int>(decision.action)),
                        decision.accepted ? QStringLiteral("true")
                                          : QStringLiteral("false"),
                        blockedReason.isEmpty() ? QStringLiteral("<none>")
                                                : blockedReason);
        return result;
}
void KeyManageController::start()
{
        if (m_started)
        {
                return;
        }
        m_started = true;

        auto retryPendingTerminations = [this]()
        {
                for (const SystemTicketDto &ticket : m_ticketStore->tickets())
                {
                        if (ticket.adminDeleteStage == SystemTicketDto::AdminDeleteStage::KeyClearedAwaitingTermination)
                        {
                                m_adminTerminationService->terminate(ticket.taskId, m_keySessionId);
                        }
                }
        };
        connect(AppContext::instance(), &AppContext::userLoggedIn,
                this, [retryPendingTerminations](const UserInfo &user)
                {
        // 只有管理员 token 才有权限调工作台 termination 接口
        if (!user.role.contains(QStringLiteral("admin"), Qt::CaseInsensitive)) {
            return;
        }
        retryPendingTerminations(); });
        // 控制器启动时若管理员已登录，立即重试（覆盖"先登录后启动"场景）
        const UserInfo &currentUser = AppContext::instance()->currentUser();
        if (currentUser.isValid() && currentUser.role.contains(QStringLiteral("admin"), Qt::CaseInsensitive))
        {
                retryPendingTerminations();
        }

        m_ticketCancelWorkflow->start();
        m_keyPortRuntime->autoConnect();

        m_maintenanceService->start();

        // [LEARNING][TicketIngress 2/3：开始监听]
        // 应用顶层启动走到这里时，才要求已经创建好的 TicketIngressService 开始监听。
        if (!m_ticketIngress->start())
        {
                emit statusMessage(m_ticketIngress->lastError());
        }
        const KeySessionSnapshot snapshot = currentSnapshot();
        m_keyTaskObservation->initializeReadyState(
            isSessionReadySnapshot(snapshot));
        m_keyPortRuntime->acceptBridgeEpoch(snapshot.bridgeEpoch);
        emit sessionSnapshotChanged(snapshot);
        emit systemTicketsUpdated(systemTickets());
        schedulePendingCancelReconcile(QStringLiteral("startup"), 500);
}
