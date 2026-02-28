/**
 * @file keymanagepage.h
 * @brief 钥匙管理页面
 *
 * 四个 Tab：钥匙 / 串口报文 / HTTP客户端报文 / HTTP服务端报文
 *   - 钥匙 Tab：设备信息区 + 圆形操作按钮 + 左侧票据操作边栏 + 右侧数据表
 *   - 串口报文 Tab：专家模式/Hex 选项 + 串口日志表
 *   - HTTP Tab：只读文本日志 + 清空按钮
 * 按钮功能先留接口 (TODO stub)，优先保证 UI 可运行。
 */

#ifndef KEYMANAGEPAGE_H
#define KEYMANAGEPAGE_H

#include <QColor>
#include <QList>
#include <QWidget>

#include "protocol/LogItem.h"

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

    // ---- HTTP Tab ----
    void onClearHttpClient();
    void onClearHttpServer();

private:
    static const int kMaxSerialLogItems = 5000;

    void initUi();
    void initConnections();

    // 串口日志辅助
    void appendSerialLog(LogDir dir,
                         quint8 cmd,
                         int len,
                         const QString &summary,
                         const QByteArray &hex = QByteArray(),
                         bool expertOnly = false);
    void refreshSerialLogTable();
    bool shouldDisplayLogItem(const LogItem &item) const;
    QColor dirColor(LogDir dir) const;
    QString dirText(LogDir dir) const;
    QString cmdText(quint8 cmd) const;

    void updateStatusBar(const QString &message);

    Ui::KeyManagePage *ui;

    QList<LogItem> m_logItems;
    bool m_logOverflowNotified;
    bool m_expertMode;
    bool m_showHex;
    int m_nextOpId;
};

#endif // KEYMANAGEPAGE_H
