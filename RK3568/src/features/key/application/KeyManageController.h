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
    void onQueryTasksClicked();
    void onDeleteClicked();
    void onTransferSelectedTicket();
    void onGetSystemTicketListClicked();
    void onSystemTicketSelected(int row);

    void onExpertModeChanged(bool enabled);
    void onShowHexChanged(bool enabled);
    void onClearLogsClicked();
    bool exportLogs(const QString &path, QString *error) const;

    QList<LogItem> visibleLogs() const;
    bool shouldDisplay(const LogItem &item) const;
    KeySessionSnapshot currentSnapshot() const;
    QList<SystemTicketDto> systemTickets() const;
    QString httpServerLogText() const;

signals:
    void statusMessage(const QString &message);
    void sessionSnapshotChanged(const KeySessionSnapshot &snapshot);
    void tasksUpdated(const QList<KeyTaskDto> &tasks);
    void systemTicketsUpdated(const QList<SystemTicketDto> &tickets);
    void selectedSystemTicketChanged(const SystemTicketDto &ticket);
    void httpServerLogAppended(const QString &text);
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

private:
    void tryAutoConnectKeyPort();
    bool autoTransferEnabled() const;
    void tryStartTicketTransfer(const QString &taskId, bool automatic);
    void tryAutoTransferPendingTicket();
    int nextOpId();
    QList<KeyTaskDto> toTaskDtos(const QVariantList &taskList) const;

    IKeySessionService *m_session;
    SerialLogManager *m_logManager;
    TicketStore *m_ticketStore;
    TicketIngressService *m_ticketIngress;
    int m_nextOpId;
    QString m_selectedSystemTicketId;
    QString m_activeTransferTaskId;
    QStringList m_httpServerLogLines;
};

#endif // KEYMANAGECONTROLLER_H
