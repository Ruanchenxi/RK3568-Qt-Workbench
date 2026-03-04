/**
 * @file keymanagepage.cpp
 * @brief 钥匙管理页面实现
 *
 * 四个 Tab：钥匙 / 串口报文 / HTTP客户端报文 / HTTP服务端报文
 * 说明：
 * 1. 本文件仅保留 UI 组织与渲染职责。
 * 2. 串口业务编排由 KeyManageController 负责。
 * 3. 页面层不再直连具体协议实现，避免协议细节渗透到 UI。
 */

#include "keymanagepage.h"
#include "ui_keymanagepage.h"

#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QFileDialog>
#include <QHeaderView>
#include <QPalette>
#include <QStringList>
#include <QTableWidgetItem>

#include "platform/logging/LogService.h"

namespace {
QString formatHexMultiline(const QByteArray &bytes, int bytesPerLine = 16)
{
    if (bytes.isEmpty()) {
        return QStringLiteral("--");
    }
    QStringList lines;
    lines.reserve((bytes.size() + bytesPerLine - 1) / bytesPerLine);
    for (int i = 0; i < bytes.size(); i += bytesPerLine) {
        lines << QString(bytes.mid(i, bytesPerLine).toHex(' ').toUpper());
    }
    return lines.join('\n');
}
}

// ====================================================================
// 构造 / 析构
// ====================================================================

KeyManagePage::KeyManagePage(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::KeyManagePage)
    , m_controller(new KeyManageController(nullptr, nullptr, this))
    , m_expertMode(false)
    , m_showHex(true)
{
    ui->setupUi(this);
    initUi();
    initConnections();
    initController();

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
    ui->tabKey->setChecked(true);
    onTabChanged(0);

    ui->serialLogTable->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->serialLogTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->serialLogTable->setWordWrap(true);
    ui->serialLogTable->setShowGrid(true);
    ui->serialLogTable->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    ui->serialLogTable->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    ui->serialLogTable->setTextElideMode(Qt::ElideRight);
    ui->serialLogTable->verticalHeader()->setVisible(false);
    ui->serialLogTable->verticalHeader()->setDefaultSectionSize(28);
    QHeaderView *serialHeader = ui->serialLogTable->horizontalHeader();
    serialHeader->setSectionsMovable(false);
    serialHeader->setMinimumSectionSize(48);
    serialHeader->setStretchLastSection(false);
    serialHeader->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    serialHeader->setSectionResizeMode(0, QHeaderView::Fixed);
    serialHeader->setSectionResizeMode(1, QHeaderView::Fixed);
    serialHeader->setSectionResizeMode(2, QHeaderView::Fixed);
    serialHeader->setSectionResizeMode(3, QHeaderView::Fixed);
    serialHeader->setSectionResizeMode(4, QHeaderView::Fixed);
    serialHeader->setSectionResizeMode(5, QHeaderView::Fixed);
    serialHeader->setSectionResizeMode(6, QHeaderView::Fixed);
    ui->serialLogTable->setColumnWidth(0, 120);  // 时间
    ui->serialLogTable->setColumnWidth(1, 72);   // 方向
    ui->serialLogTable->setColumnWidth(2, 64);   // OpId
    ui->serialLogTable->setColumnWidth(3, 96);   // 命令
    ui->serialLogTable->setColumnWidth(4, 72);   // 长度
    ui->serialLogTable->setColumnWidth(5, 280);  // 摘要
    ui->serialLogTable->setColumnWidth(6, 560);  // Hex
    if (QTableWidgetItem *h = ui->serialLogTable->horizontalHeaderItem(2)) h->setText(QStringLiteral("OpId"));
    if (QTableWidgetItem *h = ui->serialLogTable->horizontalHeaderItem(1)) h->setTextAlignment(Qt::AlignCenter);
    if (QTableWidgetItem *h = ui->serialLogTable->horizontalHeaderItem(2)) h->setTextAlignment(Qt::AlignCenter);
    if (QTableWidgetItem *h = ui->serialLogTable->horizontalHeaderItem(3)) h->setTextAlignment(Qt::AlignCenter);
    if (QTableWidgetItem *h = ui->serialLogTable->horizontalHeaderItem(4)) h->setTextAlignment(Qt::AlignCenter);
    // 强制覆盖高亮文本颜色，避免部分主题下选中行出现“白底白字”不可读。
    QPalette serialPalette = ui->serialLogTable->palette();
    serialPalette.setColor(QPalette::Highlight, QColor("#E8F1FF"));
    serialPalette.setColor(QPalette::HighlightedText, QColor("#1F2937"));
    ui->serialLogTable->setPalette(serialPalette);
    ui->serialLogTable->setStyleSheet(
        ui->serialLogTable->styleSheet()
        + QStringLiteral(
            "QTableWidget#serialLogTable { gridline-color: #E3E8EE; }"
            "QTableWidget#serialLogTable QHeaderView::section {"
            " background: #F7F9FB; border-right: 1px solid #E3E8EE; border-bottom: 1px solid #E3E8EE;"
            " padding: 2px 6px; color: #7B8794; font-weight: 600; }"
            "QTableWidget#serialLogTable::item {"
            " border-right: 1px solid #EEF1F4; border-bottom: 1px solid #EEF1F4; padding: 1px 6px; }"
            "QTableWidget#serialLogTable::item:selected { background: #E8F1FF; color: #1F2937; }"
            "QTableWidget#serialLogTable::item:selected:active { background: #DCEBFF; color: #1F2937; }"
            "QTableWidget#serialLogTable::item:selected:!active { background: #EEF4FF; color: #1F2937; }"));

    ui->chkShowHex->setChecked(true);
    ui->chkExpertMode->setChecked(false);
    onShowHexToggled(true);

    ui->systemTicketTable->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->systemTicketTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->systemTicketTable->verticalHeader()->setVisible(false);
    ui->systemTicketTable->horizontalHeader()->setStretchLastSection(true);

    ui->keyTicketTable->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->keyTicketTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->keyTicketTable->verticalHeader()->setVisible(false);
    ui->keyTicketTable->horizontalHeader()->setStretchLastSection(true);

    ui->lblBatteryInfo->setText(QStringLiteral("电量: --"));
    ui->lblTaskInfo->setText(QStringLiteral("任务数: --"));
    ui->lblCommSeatNo->setText(QStringLiteral("当前通讯座号: --"));
    ui->lblCommParams->setText(QStringLiteral("串口: -- OFF"));
    ui->lblCommStatus->setText(QStringLiteral("通讯: <font color='#EF6C00'>未连接</font>"));
    ui->lblKeyPosition->setText(QStringLiteral("在位: <font color='#EF6C00'>未插</font>"));
}

void KeyManagePage::initConnections()
{
    connect(ui->tabKey,        &QPushButton::clicked, this, [this]() { onTabChanged(0); });
    connect(ui->tabSerial,     &QPushButton::clicked, this, [this]() { onTabChanged(1); });
    connect(ui->tabHttpClient, &QPushButton::clicked, this, [this]() { onTabChanged(2); });
    connect(ui->tabHttpServer, &QPushButton::clicked, this, [this]() { onTabChanged(3); });

    connect(ui->btnDownloadRfid,    &QPushButton::clicked, this, &KeyManagePage::onDownloadRfid);
    connect(ui->btnInitDevice,      &QPushButton::clicked, this, &KeyManagePage::onInitDevice);
    connect(ui->btnQueryBattery,    &QPushButton::clicked, this, &KeyManagePage::onQueryBattery);
    connect(ui->btnQueryTaskCount,  &QPushButton::clicked, this, &KeyManagePage::onQueryTaskCount);
    connect(ui->btnSyncTime,        &QPushButton::clicked, this, &KeyManagePage::onSyncDeviceTime);

    connect(ui->btnTransferTicket,  &QPushButton::clicked, this, &KeyManagePage::onTransferTicket);
    connect(ui->btnDeleteTicket,    &QPushButton::clicked, this, &KeyManagePage::onDeleteTicket);
    connect(ui->btnReturnTicket,    &QPushButton::clicked, this, &KeyManagePage::onReturnTicket);

    connect(ui->btnGetSystemTicketList, &QPushButton::clicked, this, &KeyManagePage::onGetSystemTicketList);
    connect(ui->btnReadKeyTicketList,   &QPushButton::clicked, this, &KeyManagePage::onReadKeyTicketList);

    connect(ui->chkExpertMode, &QCheckBox::toggled, this, &KeyManagePage::onExpertModeToggled);
    connect(ui->chkShowHex,    &QCheckBox::toggled, this, &KeyManagePage::onShowHexToggled);
    connect(ui->btnExportLog,  &QPushButton::clicked, this, &KeyManagePage::onExportSerialLog);
    connect(ui->btnClearSerial,&QPushButton::clicked, this, &KeyManagePage::onClearSerialLog);
    connect(ui->serialLogTable, &QTableWidget::cellDoubleClicked,
            this, &KeyManagePage::onSerialLogCellDoubleClicked);

    connect(ui->btnClearHttpClient, &QPushButton::clicked, this, &KeyManagePage::onClearHttpClient);
    connect(ui->btnClearHttpServer, &QPushButton::clicked, this, &KeyManagePage::onClearHttpServer);
}

void KeyManagePage::initController()
{
    connect(m_controller, &KeyManageController::sessionSnapshotChanged,
            this, &KeyManagePage::onSessionSnapshotChanged);
    connect(m_controller, &KeyManageController::tasksUpdated,
            this, &KeyManagePage::onTasksUpdated);
    connect(m_controller, &KeyManageController::ackReceived,
            this, &KeyManagePage::onAckReceived);
    connect(m_controller, &KeyManageController::nakReceived,
            this, &KeyManagePage::onNakReceived);
    connect(m_controller, &KeyManageController::timeoutOccurred,
            this, &KeyManagePage::onControllerTimeout);
    connect(m_controller, &KeyManageController::statusMessage,
            this, &KeyManagePage::onControllerStatusMessage);
    connect(m_controller, &KeyManageController::logRowAppended,
            this, &KeyManagePage::onLogRowAppended);
    connect(m_controller, &KeyManageController::logTableRefreshRequested,
            this, &KeyManagePage::onLogTableRefreshRequested);
    connect(m_controller, &KeyManageController::logsCleared,
            this, &KeyManagePage::onLogsCleared);

    m_controller->start();
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
// 钥匙 Tab: 设备操作按钮
// ====================================================================

void KeyManagePage::onDownloadRfid()
{
    updateStatusBar(QStringLiteral("下载RFID...（待接协议命令）"));
}

void KeyManagePage::onInitDevice()
{
    m_controller->onInitClicked();
}

void KeyManagePage::onQueryBattery()
{
    updateStatusBar(QStringLiteral("获取电量...（待接协议命令）"));
}

void KeyManagePage::onQueryTaskCount()
{
    m_controller->onQueryTasksClicked();
}

void KeyManagePage::onSyncDeviceTime()
{
    updateStatusBar(QStringLiteral("钥匙校时...（待接协议命令）"));
}

// ====================================================================
// 钥匙 Tab: 票据操作按钮
// ====================================================================

void KeyManagePage::onTransferTicket()
{
    updateStatusBar(QStringLiteral("传票...（HTTP流程待对接）"));
}

void KeyManagePage::onDeleteTicket()
{
    m_controller->onDeleteClicked();
}

void KeyManagePage::onReturnTicket()
{
    updateStatusBar(QStringLiteral("回传...（HTTP流程待对接）"));
}

void KeyManagePage::onGetSystemTicketList()
{
    updateStatusBar(QStringLiteral("获取系统票列表...（HTTP流程待对接）"));
}

void KeyManagePage::onReadKeyTicketList()
{
    m_controller->onQueryTasksClicked();
    updateStatusBar(QStringLiteral("读取钥匙票列表 (Q_TASK)"));
}

// ====================================================================
// 串口报文 Tab
// ====================================================================

void KeyManagePage::onExpertModeToggled(bool checked)
{
    m_expertMode = checked;
    m_controller->onExpertModeChanged(checked);
    updateStatusBar(checked
                        ? QStringLiteral("专家模式已开启：显示RAW/诊断日志")
                        : QStringLiteral("专家模式已关闭：隐藏RAW/诊断日志"));
}

void KeyManagePage::onShowHexToggled(bool checked)
{
    m_showHex = checked;
    m_controller->onShowHexChanged(checked);
    QTableWidget *table = ui->serialLogTable;
    QHeaderView *header = table->horizontalHeader();
    table->setColumnHidden(6, !checked);
    if (checked) {
        table->setWordWrap(true);
        header->setSectionResizeMode(5, QHeaderView::Fixed);
        header->setSectionResizeMode(6, QHeaderView::Fixed);
        table->setColumnWidth(5, 280);
        table->setColumnWidth(6, 560);
        table->resizeRowsToContents();
        updateStatusBar(QStringLiteral("Hex列已显示（固定宽，多行显示）"));
    } else {
        table->setWordWrap(false);
        header->setSectionResizeMode(5, QHeaderView::Stretch);
        const int rowHeight = table->verticalHeader()->defaultSectionSize();
        for (int row = 0; row < table->rowCount(); ++row) {
            table->setRowHeight(row, rowHeight);
        }
        updateStatusBar(QStringLiteral("Hex列已隐藏"));
    }
}

void KeyManagePage::onExportSerialLog()
{
    const QString defaultName = QStringLiteral("serial_log_%1.csv")
                                    .arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss"));
    const QString path = QFileDialog::getSaveFileName(this,
                                                      QStringLiteral("导出串口日志"),
                                                      defaultName,
                                                      QStringLiteral("CSV Files (*.csv)"));
    if (path.isEmpty()) {
        return;
    }

    QString error;
    if (!m_controller->exportLogs(path, &error)) {
        updateStatusBar(QStringLiteral("导出失败：%1").arg(error));
        return;
    }

    updateStatusBar(QStringLiteral("日志已导出: %1").arg(path));
}

void KeyManagePage::onClearSerialLog()
{
    m_controller->onClearLogsClicked();
}

void KeyManagePage::onSerialLogCellDoubleClicked(int row, int column)
{
    if (column != 6 || row < 0) {
        return;
    }
    QTableWidgetItem *item = ui->serialLogTable->item(row, column);
    if (!item) {
        return;
    }
    QString text = item->data(Qt::UserRole).toString().trimmed();
    if (text.isEmpty()) {
        text = item->text().trimmed();
    }
    if (text.isEmpty() || text == QStringLiteral("--")) {
        return;
    }
    if (QClipboard *cb = QApplication::clipboard()) {
        cb->setText(text);
        updateStatusBar(QStringLiteral("Hex 已复制到剪贴板"));
    }
}

// ====================================================================
// Controller 回调
// ====================================================================

void KeyManagePage::onSessionSnapshotChanged(const KeySessionSnapshot &snapshot)
{
    if (snapshot.connected) {
        ui->lblCommStatus->setText(QStringLiteral("通讯: <font color='#2E7D32'>正常</font>"));
        ui->lblCommParams->setText(QStringLiteral("串口: %1 ON")
                                       .arg(snapshot.portName.isEmpty() ? "--" : snapshot.portName));
    } else {
        ui->lblCommStatus->setText(QStringLiteral("通讯: <font color='#C62828'>断开</font>"));
        ui->lblCommParams->setText(QStringLiteral("串口: -- OFF"));
    }

    if (snapshot.keyPresent) {
        ui->lblKeyPosition->setText(QStringLiteral("在位: <font color='#2E7D32'>已插</font>"));
    } else {
        ui->lblKeyPosition->setText(QStringLiteral("在位: <font color='#EF6C00'>未插</font>"));
        ui->lblTaskInfo->setText(QStringLiteral("任务数: --"));
    }

    if (snapshot.keyPresent && !snapshot.keyStable) {
        updateStatusBar(QStringLiteral("等待钥匙稳定..."));
    }
}

void KeyManagePage::onTasksUpdated(const QList<KeyTaskDto> &tasks)
{
    ui->lblTaskInfo->setText(QStringLiteral("任务数: %1").arg(tasks.size()));
    populateKeyTicketTable(tasks);
}

void KeyManagePage::onAckReceived(quint8 ackedCmd)
{
    updateStatusBar(QStringLiteral("收到 ACK: %1").arg(cmdText(ackedCmd)));
}

void KeyManagePage::onNakReceived(quint8 origCmd, quint8 errCode)
{
    updateStatusBar(QStringLiteral("收到 NAK: %1 err=0x%2")
                        .arg(cmdText(origCmd))
                        .arg(errCode, 2, 16, QLatin1Char('0')));
}

void KeyManagePage::onControllerTimeout(const QString &what)
{
    if (what.contains("Q_TASK", Qt::CaseInsensitive)) {
        ui->lblTaskInfo->setText(QStringLiteral("任务数: 查询超时"));
    }
    updateStatusBar(what);
}

void KeyManagePage::onControllerStatusMessage(const QString &what)
{
    updateStatusBar(what);
}

void KeyManagePage::onLogRowAppended(const LogItem &item)
{
    appendSerialLogRow(item);
}

void KeyManagePage::onLogTableRefreshRequested()
{
    refreshSerialLogTable();
}

void KeyManagePage::onLogsCleared()
{
    ui->serialLogTable->setRowCount(0);
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

void KeyManagePage::appendSerialLogRow(const LogItem &item)
{
    QTableWidget *t = ui->serialLogTable;
    const int row = t->rowCount();
    t->insertRow(row);

    QTableWidgetItem *timeItem = new QTableWidgetItem(item.timestamp.toString("hh:mm:ss.zzz"));
    timeItem->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    t->setItem(row, 0, timeItem);

    QTableWidgetItem *dirItem = new QTableWidgetItem(dirText(item.dir));
    dirItem->setForeground(dirColor(item.dir));
    dirItem->setTextAlignment(Qt::AlignCenter);
    t->setItem(row, 1, dirItem);

    QTableWidgetItem *opItem = new QTableWidgetItem(QString::number(item.opId));
    opItem->setTextAlignment(Qt::AlignCenter);
    t->setItem(row, 2, opItem);

    QTableWidgetItem *cmdItem = new QTableWidgetItem(cmdText(item.cmd));
    cmdItem->setTextAlignment(Qt::AlignCenter);
    t->setItem(row, 3, cmdItem);

    QTableWidgetItem *lenItem = new QTableWidgetItem(QString::number(item.length));
    lenItem->setTextAlignment(Qt::AlignCenter);
    t->setItem(row, 4, lenItem);

    QTableWidgetItem *summaryItem = new QTableWidgetItem(item.summary);
    summaryItem->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    summaryItem->setToolTip(item.summary);
    t->setItem(row, 5, summaryItem);

    const QString rawHex = item.hex.isEmpty()
                               ? QStringLiteral("--")
                               : QString(item.hex.toHex(' ').toUpper());
    QTableWidgetItem *hexItem = new QTableWidgetItem(formatHexMultiline(item.hex, 16));
    hexItem->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    hexItem->setToolTip(rawHex);
    hexItem->setData(Qt::UserRole, rawHex);
    t->setItem(row, 6, hexItem);

    if (m_showHex && !t->isColumnHidden(6)) {
        t->resizeRowToContents(row);
    } else {
        t->setRowHeight(row, t->verticalHeader()->defaultSectionSize());
    }
    t->scrollToBottom();
}

void KeyManagePage::refreshSerialLogTable()
{
    QTableWidget *t = ui->serialLogTable;
    t->setRowCount(0);

    const QList<LogItem> visible = m_controller->visibleLogs();
    for (const LogItem &item : visible) {
        appendSerialLogRow(item);
    }
    if (m_showHex && !t->isColumnHidden(6)) {
        t->resizeRowsToContents();
    }
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
    case LogDir::TX:    return QStringLiteral("TX>");
    case LogDir::RX:    return QStringLiteral("RX<");
    case LogDir::RAW:   return QStringLiteral("RAW~");
    case LogDir::EVENT: return QStringLiteral("EVT*");
    case LogDir::WARN:  return QStringLiteral("WARN!");
    case LogDir::ERROR: return QStringLiteral("ERR!");
    default:            return QStringLiteral("?");
    }
}

QString KeyManagePage::cmdText(quint8 cmd) const
{
    switch (cmd) {
    case KeyProtocol::CmdSetCom:   return QStringLiteral("SET_COM");
    case KeyProtocol::CmdQTask:    return QStringLiteral("Q_TASK");
    case KeyProtocol::CmdDel:      return QStringLiteral("DEL");
    case KeyProtocol::CmdAck:      return QStringLiteral("ACK");
    case KeyProtocol::CmdNak:      return QStringLiteral("NAK");
    case KeyProtocol::CmdKeyEvent: return QStringLiteral("KEY_EVT");
    default:
        return QStringLiteral("0x%1").arg(cmd, 2, 16, QLatin1Char('0')).toUpper();
    }
}

void KeyManagePage::populateKeyTicketTable(const QList<KeyTaskDto> &tasks)
{
    ui->keyTicketTable->setRowCount(0);
    for (int i = 0; i < tasks.size(); ++i) {
        const KeyTaskDto &task = tasks.at(i);
        const int row = ui->keyTicketTable->rowCount();
        ui->keyTicketTable->insertRow(row);

        const QString taskIdHex = QString(task.taskId.toHex().toUpper());
        ui->keyTicketTable->setItem(row, 0, new QTableWidgetItem(QString::number(i + 1)));
        ui->keyTicketTable->setItem(row, 1, new QTableWidgetItem(taskIdHex.left(16)));
        ui->keyTicketTable->setItem(row, 2, new QTableWidgetItem(QStringLiteral("类型%1").arg(task.type)));
        ui->keyTicketTable->setItem(row, 3, new QTableWidgetItem(QStringLiteral("来源%1").arg(task.source)));
        ui->keyTicketTable->setItem(row, 4, new QTableWidgetItem(QStringLiteral("状态%1").arg(task.status)));
    }
}

void KeyManagePage::updateStatusBar(const QString &message)
{
    ui->lblFeedback->setText(message);
}
