/**
 * @file keymanagepage.cpp
 * @brief 钥匙管理页面实现
 *
 * 四个 Tab：钥匙 / 串口报文 / HTTP客户端报文 / HTTP服务端报文
 * 按钮功能先留接口 (TODO stub)，优先保证 UI 可运行。
 */

#include "keymanagepage.h"
#include "ui_keymanagepage.h"

#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QFileDialog>
#include <QHeaderView>
#include <QTableWidgetItem>
#include <QTextStream>

#include "services/LogService.h"

// ====================================================================
// 构造 / 析构
// ====================================================================

KeyManagePage::KeyManagePage(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::KeyManagePage)
    , m_logOverflowNotified(false)
    , m_expertMode(false)
    , m_showHex(true)
    , m_nextOpId(1)
{
    ui->setupUi(this);
    initUi();
    initConnections();

    updateStatusBar(QStringLiteral("就绪"));
    LOG_INFO("KeyManagePage", "钥匙管理页面初始化完成");
}

KeyManagePage::~KeyManagePage()
{
    delete ui;
}

// ====================================================================
// 初始化
// ====================================================================

void KeyManagePage::initUi()
{
    // ---- 顶部 Tab 初始化 ----
    ui->tabKey->setChecked(true);
    onTabChanged(0);

    // ---- 串口日志表初始化 ----
    ui->serialLogTable->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->serialLogTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->serialLogTable->setWordWrap(false);
    ui->serialLogTable->verticalHeader()->setVisible(false);
    ui->serialLogTable->horizontalHeader()->setStretchLastSection(true);

    ui->chkShowHex->setChecked(true);
    ui->chkExpertMode->setChecked(false);

    // ---- 票据表格初始化 ----
    ui->systemTicketTable->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->systemTicketTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->systemTicketTable->verticalHeader()->setVisible(false);
    ui->systemTicketTable->horizontalHeader()->setStretchLastSection(true);

    ui->keyTicketTable->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->keyTicketTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->keyTicketTable->verticalHeader()->setVisible(false);
    ui->keyTicketTable->horizontalHeader()->setStretchLastSection(true);

    // ---- 初始状态标签 ----
    ui->lblBatteryInfo->setText(QStringLiteral("电量: 0%"));
    ui->lblTaskInfo->setText(QStringLiteral("任务数: 0"));
    ui->lblCommSeatNo->setText(QStringLiteral("当前通讯座号: --"));
    ui->lblCommParams->setText(QStringLiteral("串口: -- OFF"));
}

void KeyManagePage::initConnections()
{
    // ---- 顶部 Tab 切换 (4个Tab) ----
    connect(ui->tabKey,        &QPushButton::clicked, this, [this]() { onTabChanged(0); });
    connect(ui->tabSerial,     &QPushButton::clicked, this, [this]() { onTabChanged(1); });
    connect(ui->tabHttpClient, &QPushButton::clicked, this, [this]() { onTabChanged(2); });
    connect(ui->tabHttpServer, &QPushButton::clicked, this, [this]() { onTabChanged(3); });

    // ---- 钥匙 Tab: 设备操作按钮 ----
    connect(ui->btnDownloadRfid,    &QPushButton::clicked, this, &KeyManagePage::onDownloadRfid);
    connect(ui->btnInitDevice,      &QPushButton::clicked, this, &KeyManagePage::onInitDevice);
    connect(ui->btnQueryBattery,    &QPushButton::clicked, this, &KeyManagePage::onQueryBattery);
    connect(ui->btnQueryTaskCount,  &QPushButton::clicked, this, &KeyManagePage::onQueryTaskCount);
    connect(ui->btnSyncTime,        &QPushButton::clicked, this, &KeyManagePage::onSyncDeviceTime);

    // ---- 钥匙 Tab: 票据操作按钮 ----
    connect(ui->btnTransferTicket,  &QPushButton::clicked, this, &KeyManagePage::onTransferTicket);
    connect(ui->btnDeleteTicket,    &QPushButton::clicked, this, &KeyManagePage::onDeleteTicket);
    connect(ui->btnReturnTicket,    &QPushButton::clicked, this, &KeyManagePage::onReturnTicket);

    // ---- 钥匙 Tab: 获取票列表按钮 ----
    connect(ui->btnGetSystemTicketList, &QPushButton::clicked, this, &KeyManagePage::onGetSystemTicketList);
    connect(ui->btnReadKeyTicketList,   &QPushButton::clicked, this, &KeyManagePage::onReadKeyTicketList);

    // ---- 串口报文 Tab ----
    connect(ui->chkExpertMode, &QCheckBox::toggled,   this, &KeyManagePage::onExpertModeToggled);
    connect(ui->chkShowHex,    &QCheckBox::toggled,   this, &KeyManagePage::onShowHexToggled);
    connect(ui->btnExportLog,  &QPushButton::clicked,  this, &KeyManagePage::onExportSerialLog);
    connect(ui->btnClearSerial,&QPushButton::clicked,  this, &KeyManagePage::onClearSerialLog);

    // ---- HTTP Tab ----
    connect(ui->btnClearHttpClient, &QPushButton::clicked, this, &KeyManagePage::onClearHttpClient);
    connect(ui->btnClearHttpServer, &QPushButton::clicked, this, &KeyManagePage::onClearHttpServer);
}

// ====================================================================
// 顶部 Tab 切换
// ====================================================================

void KeyManagePage::onTabChanged(int index)
{
    ui->contentStack->setCurrentIndex(index);
    ui->tabKey->setChecked(index == 0);
    ui->tabSerial->setChecked(index == 1);
    ui->tabHttpClient->setChecked(index == 2);
    ui->tabHttpServer->setChecked(index == 3);
}

// ====================================================================
// 钥匙 Tab: 设备操作按钮 (TODO stub)
// ====================================================================

void KeyManagePage::onDownloadRfid()
{
    updateStatusBar(QStringLiteral("下载RFID...（未实现）"));
    qDebug() << "[KeyManagePage] onDownloadRfid - TODO";
}

void KeyManagePage::onInitDevice()
{
    updateStatusBar(QStringLiteral("初始化设备...（未实现）"));
    qDebug() << "[KeyManagePage] onInitDevice - TODO";
}

void KeyManagePage::onQueryBattery()
{
    updateStatusBar(QStringLiteral("获取电量...（未实现）"));
    qDebug() << "[KeyManagePage] onQueryBattery - TODO";
}

void KeyManagePage::onQueryTaskCount()
{
    updateStatusBar(QStringLiteral("获取任务数...（未实现）"));
    qDebug() << "[KeyManagePage] onQueryTaskCount - TODO";
}

void KeyManagePage::onSyncDeviceTime()
{
    updateStatusBar(QStringLiteral("钥匙校时...（未实现）"));
    qDebug() << "[KeyManagePage] onSyncDeviceTime - TODO";
}

// ====================================================================
// 钥匙 Tab: 票据操作按钮 (TODO stub)
// ====================================================================

void KeyManagePage::onTransferTicket()
{
    updateStatusBar(QStringLiteral("传票...（未实现）"));
    qDebug() << "[KeyManagePage] onTransferTicket - TODO";
}

void KeyManagePage::onDeleteTicket()
{
    updateStatusBar(QStringLiteral("删除票...（未实现）"));
    qDebug() << "[KeyManagePage] onDeleteTicket - TODO";
}

void KeyManagePage::onReturnTicket()
{
    updateStatusBar(QStringLiteral("回传...（未实现）"));
    qDebug() << "[KeyManagePage] onReturnTicket - TODO";
}

void KeyManagePage::onGetSystemTicketList()
{
    updateStatusBar(QStringLiteral("获取系统票列表...（未实现）"));
    qDebug() << "[KeyManagePage] onGetSystemTicketList - TODO";
}

void KeyManagePage::onReadKeyTicketList()
{
    updateStatusBar(QStringLiteral("读取钥匙票列表...（未实现）"));
    qDebug() << "[KeyManagePage] onReadKeyTicketList - TODO";
}

// ====================================================================
// 串口报文 Tab
// ====================================================================

void KeyManagePage::onExpertModeToggled(bool checked)
{
    m_expertMode = checked;
    refreshSerialLogTable();
}

void KeyManagePage::onShowHexToggled(bool checked)
{
    m_showHex = checked;
    ui->serialLogTable->setColumnHidden(6, !checked);
}

void KeyManagePage::onExportSerialLog()
{
    const QString defaultName = QStringLiteral("serial_log_%1.csv")
                                    .arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss"));
    const QString path = QFileDialog::getSaveFileName(this,
                                                      QStringLiteral("导出串口日志"),
                                                      defaultName,
                                                      QStringLiteral("CSV Files (*.csv)"));
    if (path.isEmpty())
        return;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        updateStatusBar(QStringLiteral("导出失败：无法写入文件"));
        return;
    }

    QTextStream out(&file);
    out.setCodec("UTF-8");
    out << QStringLiteral("时间,方向,OpId,命令,长度,摘要,Hex\n");

    for (const LogItem &item : m_logItems) {
        const QString hexText = item.hex.isEmpty()
                                    ? QStringLiteral("--")
                                    : item.hex.toHex(' ').toUpper();
        QString summaryEscaped = item.summary;
        summaryEscaped.replace("\"", "\"\"");
        out << item.timestamp.toString("hh:mm:ss.zzz") << ","
            << dirText(item.dir) << ","
            << item.opId << ","
            << cmdText(item.cmd) << ","
            << item.length << ",\""
            << summaryEscaped << "\",\""
            << hexText << "\"\n";
    }

    updateStatusBar(QStringLiteral("日志已导出: %1").arg(path));
}

void KeyManagePage::onClearSerialLog()
{
    m_logItems.clear();
    m_logOverflowNotified = false;
    ui->serialLogTable->setRowCount(0);
    updateStatusBar(QStringLiteral("串口日志已清空"));
}

// ====================================================================
// HTTP Tab
// ====================================================================

void KeyManagePage::onClearHttpClient()
{
    ui->httpClientLogText->clear();
}

void KeyManagePage::onClearHttpServer()
{
    ui->httpServerLogText->clear();
}

// ====================================================================
// 串口日志辅助
// ====================================================================

void KeyManagePage::appendSerialLog(LogDir dir,
                                    quint8 cmd,
                                    int len,
                                    const QString &summary,
                                    const QByteArray &hex,
                                    bool expertOnly)
{
    if (m_logItems.size() >= kMaxSerialLogItems) {
        if (!m_logOverflowNotified) {
            m_logOverflowNotified = true;
            qWarning() << "[KeyManagePage] Serial log overflow, oldest entries dropped.";
        }
        m_logItems.removeFirst();
    }

    LogItem item;
    item.timestamp  = QDateTime::currentDateTime();
    item.dir        = dir;
    item.cmd        = cmd;
    item.length     = static_cast<quint16>(len);
    item.summary    = summary;
    item.hex        = hex;
    item.crcOk      = true;
    item.expertOnly = expertOnly;
    item.opId       = m_nextOpId++;

    m_logItems.append(item);

    if (shouldDisplayLogItem(item)) {
        QTableWidget *t = ui->serialLogTable;
        int row = t->rowCount();
        t->insertRow(row);

        t->setItem(row, 0, new QTableWidgetItem(item.timestamp.toString("hh:mm:ss.zzz")));

        QTableWidgetItem *dirItem = new QTableWidgetItem(dirText(dir));
        dirItem->setForeground(dirColor(dir));
        t->setItem(row, 1, dirItem);

        t->setItem(row, 2, new QTableWidgetItem(QString::number(item.opId)));
        t->setItem(row, 3, new QTableWidgetItem(cmdText(cmd)));
        t->setItem(row, 4, new QTableWidgetItem(QString::number(len)));
        t->setItem(row, 5, new QTableWidgetItem(summary));
        t->setItem(row, 6, new QTableWidgetItem(QString(hex.toHex(' ').toUpper())));

        t->scrollToBottom();
    }
}

void KeyManagePage::refreshSerialLogTable()
{
    QTableWidget *t = ui->serialLogTable;
    t->setRowCount(0);

    for (const LogItem &item : m_logItems) {
        if (!shouldDisplayLogItem(item))
            continue;

        int row = t->rowCount();
        t->insertRow(row);

        t->setItem(row, 0, new QTableWidgetItem(item.timestamp.toString("hh:mm:ss.zzz")));

        QTableWidgetItem *dirItem = new QTableWidgetItem(dirText(item.dir));
        dirItem->setForeground(dirColor(item.dir));
        t->setItem(row, 1, dirItem);

        t->setItem(row, 2, new QTableWidgetItem(QString::number(item.opId)));
        t->setItem(row, 3, new QTableWidgetItem(cmdText(item.cmd)));
        t->setItem(row, 4, new QTableWidgetItem(QString::number(item.length)));
        t->setItem(row, 5, new QTableWidgetItem(item.summary));
        t->setItem(row, 6, new QTableWidgetItem(QString(item.hex.toHex(' ').toUpper())));
    }
}

bool KeyManagePage::shouldDisplayLogItem(const LogItem &item) const
{
    if (item.expertOnly && !m_expertMode)
        return false;
    return true;
}

QColor KeyManagePage::dirColor(LogDir dir) const
{
    switch (dir) {
    case LogDir::TX:    return QColor("#2E7D32");
    case LogDir::RX:    return QColor("#1565C0");
    case LogDir::RAW:   return QColor("#78909C");
    case LogDir::EVENT: return QColor("#6A1B9A");
    case LogDir::WARN:  return QColor("#EF6C00");
    case LogDir::ERROR: return QColor("#C62828");
    default:            return QColor("#5D6B7A");
    }
}

QString KeyManagePage::dirText(LogDir dir) const
{
    switch (dir) {
    case LogDir::TX:    return QStringLiteral("TX");
    case LogDir::RX:    return QStringLiteral("RX");
    case LogDir::RAW:   return QStringLiteral("RAW");
    case LogDir::EVENT: return QStringLiteral("EVT");
    case LogDir::WARN:  return QStringLiteral("WARN");
    case LogDir::ERROR: return QStringLiteral("ERR");
    default:            return QStringLiteral("?");
    }
}

QString KeyManagePage::cmdText(quint8 cmd) const
{
    return QStringLiteral("0x%1").arg(cmd, 2, 16, QLatin1Char('0')).toUpper();
}

void KeyManagePage::updateStatusBar(const QString &message)
{
    ui->lblFeedback->setText(message);
}
