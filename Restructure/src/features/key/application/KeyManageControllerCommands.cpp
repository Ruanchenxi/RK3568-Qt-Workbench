/**
 * @file KeyManageControllerCommands.cpp
 * @brief Thin L1 handlers for page commands and maintenance actions.
 *
 * Each handler validates presentation input, delegates decisions to the
 * existing L2/L3 owner, and executes only the selected adapter effect.
 */
#include "features/key/application/KeyManageController.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFutureWatcher>
#include <QSysInfo>
#include <QTextStream>
#include <QUrl>
#include <QtConcurrent/QtConcurrentRun>

#include <QDebug>
#include <QTimer>
#include "core/AppContext.h"
#include "core/ConfigManager.h"
#include "features/key/application/KeyManageObservabilityService.h"
#include "features/key/application/TicketCancelWorkflowService.h"
#include "features/key/application/KeyMaintenanceService.h"
#include "features/key/application/KeyManageSelectionService.h"
#include "features/key/application/KeyProvisioningService.h"
#include "features/key/application/KeyTaskObservationService.h"
#include "features/key/application/KeyTaskPresentationService.h"
#include "features/key/application/CaptureTicketWorkflowService.h"
#include "features/key/application/TicketDeleteWorkflowService.h"
#include "features/key/application/TicketReturnWorkflowService.h"
#include "features/key/application/TicketStore.h"
#include "features/key/application/TicketTransferWorkflowService.h"
#include "platform/logging/LogService.h"
#include "features/key/domain/TicketReturnState.h"

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

bool isSessionReadySnapshot(const KeySessionSnapshot &snapshot)
{
    return snapshot.connected
            && snapshot.keyPresent
            && snapshot.keyStable
            && snapshot.sessionReady;
}
bool currentUserIsAdministrator()
{
    const UserInfo &user = AppContext::instance()->currentUser();
    const QString role = user.role.trimmed();
    return user.isValid()
            && (role.contains(QStringLiteral("admin"), Qt::CaseInsensitive)
                || role == QStringLiteral("\u7BA1\u7406\u5458"));
}

struct LogExportResult
{
    LogExportResult(bool ok = false, const QString &text = QString())
        : success(ok), detail(text)
    {
    }

    bool success;
    QString detail;
};

bool writeUtf8File(const QString &filePath,
                   const QStringList &lines,
                   QString *error)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error) {
            *error = QStringLiteral("无法写入文件：%1").arg(filePath);
        }
        return false;
    }

    file.write("\xEF\xBB\xBF");
    QTextStream stream(&file);
    stream.setCodec("UTF-8");
    for (const QString &line : lines) {
        stream << line;
        if (!line.endsWith(QLatin1Char('\n'))) {
            stream << '\n';
        }
    }
    return stream.status() == QTextStream::Ok;
}

// Formats and writes only immutable snapshots; no live QObject is touched here.
LogExportResult writeLogBundle(const QString &parentPath,
                               const QString &timestamp,
                               const QList<LogItem> &serialItems,
                               const QStringList &httpClientLines,
                               const QStringList &httpServerLines,
                               const QStringList &appLogLines,
                               const QStringList &manifestLines)
{
    const QString bundleName = QStringLiteral("RK3568_logs_%1").arg(timestamp);
    QDir parent(parentPath);
    if (!parent.mkpath(bundleName)) {
        return {false, QStringLiteral("无法创建日志目录：%1")
                    .arg(parent.filePath(bundleName))};
    }

    const QString bundlePath = parent.filePath(bundleName);
    QDir bundle(bundlePath);
    QString error;
    if (!SerialLogManager::exportItemsText(
                serialItems,
                bundle.filePath(QStringLiteral("serial_log_%1.txt").arg(timestamp)),
                &error)
            || !writeUtf8File(
                bundle.filePath(QStringLiteral("http_client_log_%1.txt").arg(timestamp)),
                httpClientLines, &error)
            || !writeUtf8File(
                bundle.filePath(QStringLiteral("http_server_log_%1.txt").arg(timestamp)),
                httpServerLines, &error)
            || !writeUtf8File(
                bundle.filePath(QStringLiteral("app_log_%1.txt").arg(timestamp)),
                appLogLines, &error)
            || !writeUtf8File(bundle.filePath(QStringLiteral("manifest.txt")),
                              manifestLines, &error)) {
        return {false, error};
    }

    return {true, bundlePath};
}

} // namespace
void KeyManageController::onInitClicked()
{
    const KeyManualActionDecision decision =
            evaluateManualAction(KeyManualAction::Initialize);
    if (!decision.allowed) {
        emit statusMessage(decision.message);
        return;
    }

    const int stationNo = resolveBusinessStationNo();
    emit statusMessage(QStringLiteral(
                           "\u521D\u59CB\u5316\u94A5\u5319\uFF0C"
                           "\u6B63\u5728\u83B7\u53D6\u521D\u59CB\u5316\u6570\u636E..."));
    m_keyProvisioning->requestInitPayload(stationNo);
}

void KeyManageController::onDownloadRfidClicked()
{
    const KeyManualActionDecision decision =
            evaluateManualAction(KeyManualAction::DownloadRfid);
    if (!decision.allowed) {
        emit statusMessage(decision.message);
        return;
    }

    const int stationNo = resolveBusinessStationNo();
    emit statusMessage(QStringLiteral(
                           "\u4E0B\u8F7D RFID\uFF0C"
                           "\u6B63\u5728\u83B7\u53D6\u540E\u7AEF\u6570\u636E..."));
    m_keyProvisioning->requestRfidPayload(stationNo);
}

void KeyManageController::onQueryBatteryClicked()
{
    const KeyManualActionDecision decision =
            evaluateManualAction(KeyManualAction::QueryBattery);
    if (!decision.allowed) {
        emit statusMessage(decision.message);
        return;
    }

    CommandRequest req;
    req.opId = nextOpId();
    req.id = CommandId::QueryBattery;
    req.options.insert(QStringLiteral("seat"), static_cast<uint>(0x01));
    m_session->execute(req);
    emit statusMessage(QStringLiteral(
                           "\u6267\u884C\u83B7\u53D6\u7535\u91CF (Q_KEYEQ)\uFF0C"
                           "\u76EE\u6807 A\u5EA7"));
}

void KeyManageController::onSyncDeviceTimeClicked()
{
    const KeyManualActionDecision decision =
            evaluateManualAction(KeyManualAction::SyncTime);
    if (!decision.allowed) {
        emit statusMessage(decision.message);
        return;
    }

    CommandRequest req;
    req.opId = nextOpId();
    req.id = CommandId::SyncDeviceTime;
    req.options.insert(QStringLiteral("seat"), static_cast<uint>(0x01));
    m_session->execute(req);
    emit statusMessage(QStringLiteral(
                           "\u6267\u884C\u94A5\u5319\u6821\u65F6 (SET_TIME)\uFF0C"
                           "\u76EE\u6807 A\u5EA7"));
}

void KeyManageController::onQueryTasksClicked()
{
    const KeyManualActionDecision decision =
            evaluateManualAction(KeyManualAction::QueryTasks);
    if (!decision.allowed) {
        emit statusMessage(decision.message);
        return;
    }

    // A new read invalidates the previous admission proof until its Q_TASK
    // response is correlated with the current physical-key session.
    m_keyTaskObservation->beginQuery();
    CommandRequest req;
    req.id = CommandId::QueryTasks;
    req.opId = nextOpId();
    m_session->execute(req);
    emit statusMessage(QStringLiteral(
                           "\u6267\u884C\u83B7\u53D6\u4EFB\u52A1\u6570 (Q_TASK)"));
}

// Starts administrator deletion only when the selected task has fresh,
// same-session Q_TASK presence proof and the key workflow is otherwise idle.
void KeyManageController::onDeleteClicked()
{
    const KeySessionSnapshot snapshot = m_session->snapshot();
    const QByteArray taskIdRaw = m_selection->selectedKeyTaskRaw();
    const QString taskId = taskIdFromRaw(taskIdRaw);
    const SystemTicketDto mappedTicket =
            m_ticketStore->ticketById(taskId);
    quint8 selectedTaskType = 0xFF;
    const QList<KeyTaskDto> observedTasks = m_keyTaskObservation->tasks();
    for (const KeyTaskDto &task : observedTasks) {
        if (task.taskId == taskIdRaw) {
            selectedTaskType = task.type;
            break;
        }
    }
    const bool captureTask =
            CaptureTicketUseCase::isCaptureTaskType(selectedTaskType);
    const bool allowsRetransfer = mappedTicket.valid
            && ticketReturnStateFromStorage(mappedTicket.returnState)
               == TicketReturnState::Idle;

    TicketDeleteStartRequest request;
    // Q_TASK type is the authority here: capture tickets use the same verified
    // device removal, but never call the normal outage termination endpoint.
    request.origin = captureTask
            ? TicketDeleteOrigin::ManualCaptureDelete
            : TicketDeleteOrigin::ManualDelete;
    request.administratorAuthorized = currentUserIsAdministrator();
    request.serialConnected = snapshot.connected;
    request.taskSelected = !taskIdRaw.isEmpty();
    request.keyReady = isSessionReadySnapshot(snapshot);
    request.freshTaskObservation =
            m_keyTaskObservation->hasFreshSelectedTask(
                taskIdRaw, m_keySessionId);
    request.commandBusy = snapshot.commandInFlight;
    request.deleteBusy = m_ticketDeleteWorkflow->hasActiveOperation();
    request.returnBusy = m_ticketReturnWorkflow->hasReturnWork();
    request.transferBusy = m_ticketTransferWorkflow->hasActiveOperation();
    request.cancelBusy = m_ticketCancelWorkflow->hasActiveDelete();

    const TicketDeleteStartOutcome outcome =
            m_ticketDeleteWorkflow->start(
                request,
                taskId,
                taskIdRaw,
                m_keySessionId,
                allowsRetransfer);
    if (!outcome.decision.allowed) {
        emit statusMessage(outcome.decision.message);
        return;
    }

    CommandRequest command;
    command.id = CommandId::DeleteTask;
    command.opId = nextOpId();
    command.payload = outcome.effect.taskIdRaw;
    m_ticketDeleteWorkflow->recordDeleteDispatched();
    m_session->execute(command);
    emit statusMessage(QStringLiteral("执行删除票 (DEL)"));
}

bool KeyManageController::isSelectedKeyTaskCapture() const
{
    const QByteArray selectedTask = m_selection->selectedKeyTaskRaw();
    if (selectedTask.isEmpty()) return false;

    const QList<KeyTaskDto> observedTasks = m_keyTaskObservation->tasks();
    for (const KeyTaskDto &task : observedTasks) {
        if (task.taskId == selectedTask) {
            return CaptureTicketUseCase::isCaptureTaskType(task.type);
        }
    }
    return false;
}
void KeyManageController::onTransferSelectedTicket()
{
    const QString taskId = m_selection->selectedSystemTicketId().trimmed();
    if (taskId.isEmpty()) {
        emit statusMessage(QStringLiteral(
                               "\u672A\u9009\u4E2D\u7CFB\u7EDF\u7968\uFF0C\u65E0\u6CD5\u4F20\u7968"));
        return;
    }

    tryStartTicketTransfer(taskId, false);
}
void KeyManageController::onReturnSelectedTicket()
{
    const QString taskId = m_selection->selectedSystemTicketId().trimmed();
    if (taskId.isEmpty()) {
        emit statusMessage(QStringLiteral(
                               "\u672A\u9009\u4E2D\u7CFB\u7EDF\u7968\uFF0C\u65E0\u6CD5\u6267\u884C\u56DE\u4F20"));
        return;
    }

    tryStartTicketReturn(taskId, false);
}

void KeyManageController::onSystemTicketSelected(int row)
{
    const SystemTicketDto selected =
            m_selection->selectSystemTicket(row, systemTickets());
    emit selectedSystemTicketChanged(selected);
}

void KeyManageController::onKeyTaskSelected(int row)
{
    m_selection->selectKeyTask(row, m_keyTaskObservation->tasks());
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
    m_observability->clearHttpClientLogs();
    emit statusMessage(QStringLiteral("HTTP客户端报文已清空"));
}

void KeyManageController::onClearHttpServerLogsClicked()
{
    m_observability->clearHttpServerLogs();
    emit statusMessage(QStringLiteral("HTTP服务端报文已清空"));
}

bool KeyManageController::exportLogsAsync(const QString &path)
{
    if (m_logExportInProgress || path.trimmed().isEmpty()) {
        return false;
    }

    const QString timestamp = QDateTime::currentDateTime()
            .toString(QStringLiteral("yyyyMMdd_hhmmss"));

    ConfigManager *config = ConfigManager::instance();
    QUrl apiUrl(config->apiUrl());
    apiUrl.setUserName(QString());
    apiUrl.setPassword(QString());
    apiUrl.setQuery(QString());
    const KeySessionSnapshot snapshot = m_session->snapshot();
    const QString manifest = QStringLiteral(
                "日志导出时间：%1\n"
                "程序：%2\n"
                "程序版本：%3\n"
                "操作系统：%4\n"
                "API地址：%5\n"
                "业务站号：%6\n"
                "钥匙串口：%7\n"
                "波特率：%8\n"
                "钥匙会话：%9\n"
                "钥匙在位：%10\n"
                "通讯就绪：%11\n")
            .arg(QDateTime::currentDateTime().toString(Qt::ISODateWithMs),
                 QCoreApplication::applicationName(),
                 QCoreApplication::applicationVersion().isEmpty()
                    ? QStringLiteral("未设置") : QCoreApplication::applicationVersion(),
                 QSysInfo::prettyProductName(),
                 apiUrl.toString(QUrl::RemoveUserInfo | QUrl::RemoveQuery | QUrl::RemoveFragment),
                 config->stationId(),
                 config->keySerialPort(),
                 QString::number(config->baudRate()),
                 QString::number(m_keySessionId),
                 snapshot.keyPresent ? QStringLiteral("是") : QStringLiteral("否"),
                 snapshot.sessionReady ? QStringLiteral("是") : QStringLiteral("否"));

    const QList<LogItem> serialItems = m_logManager->items();
    const QStringList httpClientLines = m_observability->httpClientLogLines();
    const QStringList httpServerLines = m_observability->httpServerLogLines();
    const QStringList appLogLines = LogService::instance()->currentRunLogLines();
    const QStringList manifestLines = manifest.split(QLatin1Char('\n'), Qt::SkipEmptyParts);

    m_logExportInProgress = true;
    auto *watcher = new QFutureWatcher<LogExportResult>(this);
    connect(watcher, &QFutureWatcher<LogExportResult>::finished,
            this, [this, watcher]() {
        const LogExportResult result = watcher->result();
        watcher->deleteLater();
        m_logExportInProgress = false;
        emit logExportFinished(result.success, result.detail);
    });
    watcher->setFuture(QtConcurrent::run(
        [path, timestamp, serialItems, httpClientLines, httpServerLines,
         appLogLines, manifestLines]() {
            return writeLogBundle(path, timestamp, serialItems,
                                  httpClientLines, httpServerLines,
                                  appLogLines, manifestLines);
        }));
    return true;
}

QList<LogItem> KeyManageController::visibleLogs() const
{
    return m_logManager->filteredItems();
}

bool KeyManageController::shouldDisplay(const LogItem &item) const
{
    return m_logManager->shouldDisplay(item);
}

QString KeyManageController::serialCommandText(quint8 cmd) const
{
    return SerialLogManager::commandText(cmd);
}

KeySessionSnapshot KeyManageController::currentSnapshot() const
{
    return m_session->snapshot();
}

KeyManualActionDecision KeyManageController::evaluateManualAction(
        KeyManualAction action) const
{
    const KeySessionSnapshot snapshot = m_session->snapshot();
    KeyManualActionRequest request;
    request.action = action;
    request.workbenchConfirmationActive =
            m_ticketTransferWorkflow->hasActiveWorkbenchTransaction();
    request.serialConnected = snapshot.connected;
    request.keyReady = isSessionReadySnapshot(snapshot);
    request.provisioningBusy = m_keyProvisioning->isBusy();
    return KeyManualActionUseCase::evaluate(request);
}
KeyCommandEventContext KeyManageController::commandEventContext() const
{
    KeyCommandEventContext context;
    context.transferAwaitingConfirmation =
            m_ticketTransferWorkflow->isAwaitingDeviceConfirmation();
    context.deleteActive = m_ticketDeleteWorkflow->hasActiveOperation();
    context.cancelDeleteActive = m_ticketCancelWorkflow->hasActiveDelete();
    context.transferHandshakePending =
            m_ticketTransferWorkflow->hasPendingHandshake();
    context.returnActive = m_ticketReturnWorkflow->hasActiveReturn();
    context.returnHandshakePending =
            m_ticketReturnWorkflow->hasPendingHandshake();
    return context;
}
QList<SystemTicketDto> KeyManageController::systemTickets() const
{
    return m_ticketStore->tickets();
}

QList<KeyTaskDisplayDto> KeyManageController::keyTaskDisplayRows(
        const QList<KeyTaskDto> &tasks) const
{
    QHash<QString, CaptureTicketPhase> capturePhases;
    for (const KeyTaskDto &task : tasks) {
        const QString taskId = taskIdFromRaw(task.taskId);
        if (m_captureWorkflow->ownsTask(taskId)) {
            capturePhases.insert(taskId, m_captureWorkflow->phaseForTask(taskId));
        }
    }
    return KeyTaskPresentationService::present(
                tasks, m_ticketStore->tickets(), capturePhases);
}

QString KeyManageController::httpServerLogText() const
{
    return m_observability->httpServerLogText();
}

QString KeyManageController::httpClientLogText() const
{
    return m_observability->httpClientLogText();
}

bool KeyManageController::canStartManualReturn(const QString &taskId, QString *blockedReason) const
{
    const SystemTicketDto ticket = m_ticketStore->ticketById(taskId);
    if (progressiveStepReturnEnabled() && isProgressiveStepTicket(ticket)) {
        if (blockedReason) {
            *blockedReason = QStringLiteral("该多步骤票已启用逐步回传，将在钥匙就绪后自动处理");
        }
        return false;
    }
    return canStartTicketReturn(taskId, false, nullptr, blockedReason);
}


bool KeyManageController::isSelectedKeyTaskUnregistered() const
{
    if (m_selection->selectedKeyTaskRaw().isEmpty()) {
        return false;
    }
    const QString taskId = taskIdFromRaw(m_selection->selectedKeyTaskRaw());
    if (taskId.isEmpty()) {
        return false;
    }
    const SystemTicketDto ticket = m_ticketStore->ticketById(taskId);
    return !ticket.valid && !m_captureWorkflow->ownsTask(taskId);
}

// Starts unregistered-task cleanup only when the pure delete policy grants
// exclusive ownership of the ready physical-key session.
void KeyManageController::onClearUnregisteredKeyTaskClicked()
{
    const QByteArray taskIdRaw = m_selection->selectedKeyTaskRaw();
    const QString taskId = taskIdFromRaw(taskIdRaw);
    const SystemTicketDto ticket = m_ticketStore->ticketById(taskId);
    const KeySessionSnapshot snapshot = m_session->snapshot();

    TicketDeleteStartRequest request;
    request.origin = TicketDeleteOrigin::AdminClearUnregisteredTask;
    request.administratorAuthorized = currentUserIsAdministrator();
    request.serialConnected = snapshot.connected;
    request.taskSelected = !taskIdRaw.isEmpty();
    // Capture tickets are registered in their own process-local workflow and
    // must never enter the ordinary orphan-task deletion path.
    request.ticketExists = ticket.valid || m_captureWorkflow->ownsTask(taskId);
    request.keyReady = isSessionReadySnapshot(snapshot);
    request.freshTaskObservation =
            m_keyTaskObservation->hasFreshSelectedTask(
                taskIdRaw, m_keySessionId);
    request.commandBusy = snapshot.commandInFlight;
    request.deleteBusy = m_ticketDeleteWorkflow->hasActiveOperation();
    request.returnBusy = m_ticketReturnWorkflow->hasReturnWork();
    request.transferBusy = m_ticketTransferWorkflow->hasActiveOperation();
    request.cancelBusy = m_ticketCancelWorkflow->hasActiveDelete();

    const TicketDeleteStartOutcome outcome =
            m_ticketDeleteWorkflow->start(
                request,
                taskId,
                taskIdRaw,
                m_keySessionId);
    if (!outcome.decision.allowed) {
        emit statusMessage(outcome.decision.message);
        return;
    }

    CommandRequest command;
    command.id = CommandId::DeleteTask;
    command.opId = nextOpId();
    command.payload = outcome.effect.taskIdRaw;
    m_ticketDeleteWorkflow->recordDeleteDispatched();
    m_session->execute(command);
    qInfo().noquote()
            << QStringLiteral("[KeyManageController] onClearUnregisteredKeyTaskClicked taskId=%1 session=%2")
                  .arg(taskId, QString::number(m_keySessionId));
    emit statusMessage(QStringLiteral(
                           "\u5DF2\u53D1\u9001 DEL \u547D\u4EE4\uFF0C"
                           "\u6B63\u5728\u6E05\u9664\u672A\u767B\u8BB0\u4EFB\u52A1\uFF1A%1")
                       .arg(taskId));
}
