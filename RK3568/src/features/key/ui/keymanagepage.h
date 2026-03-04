/**
 * @file keymanagepage.h
 * @brief 钥匙管理页面
 *
 * 四个 Tab：钥匙 / 串口报文 / HTTP客户端报文 / HTTP服务端报文
 *   - 钥匙 Tab：设备信息区 + 圆形操作按钮 + 左侧票据操作边栏 + 右侧数据表
 *   - 串口报文 Tab：专家模式/Hex 选项 + 串口日志表
 *   - HTTP Tab：只读文本日志 + 清空按钮
 * 当前策略：
 *   - 页面层通过 KeyManageController 访问串口会话与日志数据
 *   - 串口会话由 IKeySessionService 抽象，默认实现由服务层注入
 *   - 页面保留 UI 组织和状态展示职责
 */

#ifndef KEYMANAGEPAGE_H
#define KEYMANAGEPAGE_H

#include <QColor>
#include <QList>
#include <QWidget>

#include "shared/contracts/KeyTaskDto.h"
#include "shared/contracts/IKeySessionService.h"
#include "features/key/application/KeyManageController.h"
#include "features/key/protocol/LogItem.h"
#include "features/key/protocol/KeyProtocolDefs.h"

namespace Ui {
class KeyManagePage;
}

class KeyManagePage : public QWidget
{
    Q_OBJECT

public:
    explicit KeyManagePage(QWidget *parent = nullptr);
    ~KeyManagePage();

private slots:
    // ---- 顶部 Tab 切换 ----
    void onTabChanged(int index);

    // ---- 钥匙 Tab: 设备操作按钮 ----
    void onDownloadRfid();
    void onInitDevice();
    void onQueryBattery();
    void onQueryTaskCount();
    void onSyncDeviceTime();

    // ---- 钥匙 Tab: 票据操作按钮 ----
    void onTransferTicket();
    void onDeleteTicket();
    void onReturnTicket();

    // ---- 钥匙 Tab: 获取票列表按钮 ----
    void onGetSystemTicketList();
    void onReadKeyTicketList();

    // ---- 串口报文 Tab ----
    void onExpertModeToggled(bool checked);
    void onShowHexToggled(bool checked);
    void onExportSerialLog();
    void onClearSerialLog();
    void onSerialLogCellDoubleClicked(int row, int column);

    // ---- Controller 回调 ----
    void onSessionSnapshotChanged(const KeySessionSnapshot &snapshot);
    void onTasksUpdated(const QList<KeyTaskDto> &tasks);
    void onAckReceived(quint8 ackedCmd);
    void onNakReceived(quint8 origCmd, quint8 errCode);
    void onControllerTimeout(const QString &what);
    void onControllerStatusMessage(const QString &what);
    void onLogRowAppended(const LogItem &item);
    void onLogTableRefreshRequested();
    void onLogsCleared();

    // ---- HTTP Tab ----
    void onClearHttpClient();
    void onClearHttpServer();

private:
    void initUi();
    void initConnections();
    void initController();

    // 串口日志辅助
    void appendSerialLogRow(const LogItem &item);
    void refreshSerialLogTable();
    QColor dirColor(LogDir dir) const;
    QString dirText(LogDir dir) const;
    QString cmdText(quint8 cmd) const;
    void populateKeyTicketTable(const QList<KeyTaskDto> &tasks);

    void updateStatusBar(const QString &message);

    Ui::KeyManagePage *ui;
    KeyManageController *m_controller;
    bool m_expertMode;
    bool m_showHex;
};

#endif // KEYMANAGEPAGE_H
