/**
 * @file KeyManageController.h
 * @brief 钥匙管理页控制器（业务编排层）
 *
 * 职责边界：
 * 1. 接收 UI 动作并驱动会话服务。
 * 2. 管理串口日志数据层（SerialLogManager）。
 * 3. 输出可直接渲染的状态给页面，避免页面直接依赖协议细节。
 */
#ifndef KEYMANAGECONTROLLER_H
#define KEYMANAGECONTROLLER_H

#include <QObject>
#include <QList>
#include <QVariantList>

#include "shared/contracts/IKeySessionService.h"
#include "shared/contracts/KeyTaskDto.h"
#include "shared/contracts/SystemTicketDto.h"
#include "features/key/application/SerialLogManager.h"

class TicketStore;
class TicketIngressService;
class TicketReturnHttpClient;
class KeyProvisioningService;

class KeyManageController : public QObject
{
    Q_OBJECT
public:
    explicit KeyManageController(IKeySessionService *session = nullptr,
                                 SerialLogManager *logManager = nullptr,
                                 QObject *parent = nullptr);
    ~KeyManageController() override = default;

    void start();

    void onInitClicked();
    void onDownloadRfidClicked();
    void onQueryTasksClicked();
    void onDeleteClicked();
    void onTransferSelectedTicket();
    void onReturnSelectedTicket();
    void onGetSystemTicketListClicked();
    void onSystemTicketSelected(int row);
    void onKeyTaskSelected(int row);

    void onExpertModeChanged(bool enabled);
    void onShowHexChanged(bool enabled);
    void onClearLogsClicked();
    void onClearHttpClientLogsClicked();
    void onClearHttpServerLogsClicked();
    bool exportLogs(const QString &path, QString *error) const;

    QList<LogItem> visibleLogs() const;
    bool shouldDisplay(const LogItem &item) const;
    KeySessionSnapshot currentSnapshot() const;
    QList<SystemTicketDto> systemTickets() const;
    QString httpServerLogText() const;
    QString httpClientLogText() const;

signals:
    void statusMessage(const QString &message);
    void sessionSnapshotChanged(const KeySessionSnapshot &snapshot);
    void tasksUpdated(const QList<KeyTaskDto> &tasks);
    void systemTicketsUpdated(const QList<SystemTicketDto> &tickets);
    void selectedSystemTicketChanged(const SystemTicketDto &ticket);
    void httpServerLogAppended(const QString &text);
    void httpClientLogAppended(const QString &text);
    void ackReceived(quint8 ackedCmd);
    void nakReceived(quint8 origCmd, quint8 errCode);
    void timeoutOccurred(const QString &what);
    void logRowAppended(const LogItem &item);
    void logTableRefreshRequested();
    void logsCleared();

private slots:
    void handleSessionEvent(const KeySessionEvent &event);
    void handleLogItem(const LogItem &item);
    void handleSystemTicketsChanged(const QList<SystemTicketDto> &tickets);
    void handleJsonReceived(const QByteArray &jsonBytes, const QString &savedPath);
    void handleHttpServerLog(const QString &text);
    void handleHttpClientLog(const QString &text);
    void handleInitPayloadReady(const QByteArray &payload);
    void handleRfidPayloadReady(const QByteArray &payload);
    void handleProvisionRequestFailed(const QString &reason);
    void handleReturnUploadSucceeded(const QString &taskId);
    void handleReturnUploadFailed(const QString &taskId, const QString &reason);

private:
    void tryAutoConnectKeyPort();
    bool autoTransferEnabled() const;
    void tryAutoRefreshKeyTasksOnReady(const KeySessionSnapshot &snapshot);
    void reconcileSystemTicketsFromKeyTasks(const QList<KeyTaskDto> &tasks);
    void recoverAutoTransferWhenKeyEmpty(const QList<KeyTaskDto> &tasks);
    void tryAutoCleanupReturnedTicket(const QList<KeyTaskDto> &tasks);
    void tryStartTicketTransfer(const QString &taskId, bool automatic);
    void tryAutoTransferPendingTicket();
    void tryStartTicketReturn(const QString &taskId, bool automatic);
    void tryAutoReturnCompletedTicket(const QList<KeyTaskDto> &tasks);
    void failActiveReturnHandshake(const QString &reason);
    void clearActiveReturnContext(bool clearHandshakeOnly = false);
    void markReturnDeletePending(const QString &taskId, const QByteArray &taskIdRaw);
    void finalizePendingReturnDelete(const QList<KeyTaskDto> &tasks);
    bool hasBlockingIncompleteKeyTask(const QList<KeyTaskDto> &tasks,
                                      QString *blockingTaskId = nullptr,
                                      const QByteArray &excludeTaskIdRaw = QByteArray()) const;
    QByteArray findKeyTaskIdRaw(const QString &taskId, quint8 *status = nullptr) const;
    static QString taskIdFromRaw(const QByteArray &taskIdRaw);
    int nextOpId();
    QList<KeyTaskDto> toTaskDtos(const QVariantList &taskList) const;

    IKeySessionService *m_session;
    SerialLogManager *m_logManager;
    TicketStore *m_ticketStore;
    TicketIngressService *m_ticketIngress;
    TicketReturnHttpClient *m_ticketReturnClient;
    KeyProvisioningService *m_keyProvisioning;
    int m_nextOpId;
    QString m_selectedSystemTicketId;
    QByteArray m_selectedKeyTaskRaw;
    QString m_activeTransferTaskId;
    QString m_activeReturnTaskId;
    QByteArray m_activeReturnTaskIdRaw;
    QString m_pendingReturnHandshakeTaskId;
    QByteArray m_pendingReturnHandshakeTaskIdRaw;
    QString m_pendingDeletedSystemTicketId;
    QByteArray m_pendingDeletedKeyTaskRaw;
    bool m_pendingDeleteAllowsRetransfer = false;
    QList<KeyTaskDto> m_lastKeyTasks;
    bool m_lastReadyState = false;
    QStringList m_httpClientLogLines;
    QStringList m_httpServerLogLines;
};

#endif // KEYMANAGECONTROLLER_H
