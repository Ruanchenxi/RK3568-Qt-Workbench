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

#include "core/ConfigManager.h"
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

QString extractPositionFromTaskName(const QString &ticketNo, const QString &taskName)
{
    if (taskName.startsWith(ticketNo) && taskName.size() > ticketNo.size() + 1) {
        return taskName.mid(ticketNo.size() + 1);
    }
    return taskName;
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
    ui->tabHttpClient->setText(QStringLiteral("HTTP客户端"));
    ui->tabHttpServer->setText(QStringLiteral("HTTP服务端"));
    ui->tabKey->setMinimumWidth(84);
    ui->tabSerial->setMinimumWidth(92);
    ui->tabHttpClient->setMinimumWidth(116);
    ui->tabHttpServer->setMinimumWidth(116);
    ui->lblSelectedTicketTitle->setText(QStringLiteral("当前选中系统票"));
    ui->lblSystemTicketTitle->setText(QStringLiteral("系统票数据"));
    ui->lblKeyTicketTitle->setText(QStringLiteral("钥匙票数据"));
    ui->lblTicketOpTitle->setText(QStringLiteral("票据操作"));
    ui->btnTransferTicket->setText(QStringLiteral("传票到钥匙"));
    ui->btnDeleteTicket->setText(QStringLiteral("删除钥匙票"));
    ui->btnQueryTaskCount->setText(QStringLiteral("Q_TASK诊断"));
    ui->btnGetSystemTicketList->setText(QStringLiteral("刷新本地系统票列表"));
    ui->lblCommSeatNo->setText(QStringLiteral("当前配置座号: --"));
    ui->btnQueryTaskCount->setToolTip(QStringLiteral("发送一次 Q_TASK 诊断查询；与“读取钥匙票列表”使用同一协议命令"));
    ui->btnDeleteTicket->setToolTip(QStringLiteral("删除钥匙中已存在的票，请先读取钥匙票列表并选中。"));
    ui->lblReturnHint->setText(QStringLiteral("回传在钥匙任务完成后自动触发；接口未配置时不会上传"));

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
    ui->systemTicketTable->horizontalHeader()->setStretchLastSection(false);
    ui->systemTicketTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->systemTicketTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    ui->systemTicketTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    ui->systemTicketTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    ui->systemTicketTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
    ui->systemTicketTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    ui->systemTicketTable->setColumnWidth(0, 56);
    ui->systemTicketTable->setColumnWidth(1, 180);
    ui->systemTicketTable->setColumnWidth(2, 90);
    ui->systemTicketTable->setColumnWidth(3, 100);
    ui->systemTicketTable->setColumnWidth(4, 260);
    if (QTableWidgetItem *h = ui->systemTicketTable->horizontalHeaderItem(0)) h->setTextAlignment(Qt::AlignCenter);
    if (QTableWidgetItem *h = ui->systemTicketTable->horizontalHeaderItem(1)) h->setTextAlignment(Qt::AlignCenter);
    if (QTableWidgetItem *h = ui->systemTicketTable->horizontalHeaderItem(2)) h->setTextAlignment(Qt::AlignCenter);
    if (QTableWidgetItem *h = ui->systemTicketTable->horizontalHeaderItem(3)) h->setTextAlignment(Qt::AlignCenter);
    if (QTableWidgetItem *h = ui->systemTicketTable->horizontalHeaderItem(4)) h->setTextAlignment(Qt::AlignCenter);

    ui->keyTicketTable->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->keyTicketTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->keyTicketTable->verticalHeader()->setVisible(false);
    ui->keyTicketTable->horizontalHeader()->setStretchLastSection(false);
    ui->keyTicketTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->keyTicketTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    ui->keyTicketTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    ui->keyTicketTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    ui->keyTicketTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
    ui->keyTicketTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    ui->keyTicketTable->setColumnWidth(0, 56);
    ui->keyTicketTable->setColumnWidth(1, 180);
    ui->keyTicketTable->setColumnWidth(2, 90);
    ui->keyTicketTable->setColumnWidth(3, 100);
    ui->keyTicketTable->setColumnWidth(4, 260);
    if (QTableWidgetItem *h = ui->keyTicketTable->horizontalHeaderItem(0)) h->setTextAlignment(Qt::AlignCenter);
    if (QTableWidgetItem *h = ui->keyTicketTable->horizontalHeaderItem(1)) h->setTextAlignment(Qt::AlignCenter);
    if (QTableWidgetItem *h = ui->keyTicketTable->horizontalHeaderItem(2)) h->setTextAlignment(Qt::AlignCenter);
    if (QTableWidgetItem *h = ui->keyTicketTable->horizontalHeaderItem(3)) h->setTextAlignment(Qt::AlignCenter);
    if (QTableWidgetItem *h = ui->keyTicketTable->horizontalHeaderItem(4)) h->setTextAlignment(Qt::AlignCenter);

    ui->lblTicketPosition->setWordWrap(true);
    ui->lblTicketPosition->setMinimumHeight(54);
    ui->lblTicketReturnStatus->setWordWrap(true);
    ui->lblTicketReturnStatus->setMinimumHeight(60);
    ui->ticketInfoCard->setMinimumHeight(220);

    ui->lblBatteryInfo->setText(QStringLiteral("电量: --"));
    ui->lblTaskInfo->setText(QStringLiteral("任务数: --"));
    refreshCommSeatLabel();
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
    connect(ui->systemTicketTable, &QTableWidget::itemSelectionChanged,
            this, &KeyManagePage::onSystemTicketSelectionChanged);
    connect(ui->keyTicketTable, &QTableWidget::itemSelectionChanged,
            this, &KeyManagePage::onKeyTicketSelectionChanged);

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
    connect(m_controller, &KeyManageController::systemTicketsUpdated,
            this, &KeyManagePage::onSystemTicketsUpdated);
    connect(m_controller, &KeyManageController::selectedSystemTicketChanged,
            this, &KeyManagePage::onSelectedSystemTicketChanged);
    connect(m_controller, &KeyManageController::httpServerLogAppended,
            this, &KeyManagePage::onHttpServerLogAppended);
    connect(m_controller, &KeyManageController::httpClientLogAppended,
            this, &KeyManagePage::onHttpClientLogAppended);
    connect(ConfigManager::instance(), &ConfigManager::configChanged,
            this, [this](const QString &key, const QVariant &) {
        if (key == QLatin1String("system/stationId")) {
            refreshCommSeatLabel();
        }
    });

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
    if (index == 0 || index == 2 || index == 3)
        refreshSystemTicketViews();
}

// ====================================================================
// 钥匙 Tab: 设备操作按钮
// ====================================================================

void KeyManagePage::onDownloadRfid()
{
    m_controller->onDownloadRfidClicked();
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
    updateStatusBar(QStringLiteral("执行 Q_TASK 诊断查询"));
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
    m_controller->onTransferSelectedTicket();
}

void KeyManagePage::onDeleteTicket()
{
    m_controller->onDeleteClicked();
}

void KeyManagePage::onReturnTicket()
{
    m_controller->onReturnSelectedTicket();
}

void KeyManagePage::onGetSystemTicketList()
{
    m_controller->onGetSystemTicketListClicked();
    updateStatusBar(QStringLiteral("刷新本地主程序系统票列表"));
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
    const QString defaultName = QStringLiteral("serial_log_%1.txt")
                                    .arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss"));
    const QString path = QFileDialog::getSaveFileName(this,
                                                      QStringLiteral("导出串口日志"),
                                                      defaultName,
                                                      QStringLiteral("Text Files (*.txt)"));
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
    updateCommIndicators(snapshot);
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
    if (shouldSuppressStatusBarMessage(what)) {
        return;
    }
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

void KeyManagePage::onSystemTicketsUpdated(const QList<SystemTicketDto> &tickets)
{
    populateSystemTicketTable(tickets);
}

void KeyManagePage::onSelectedSystemTicketChanged(const SystemTicketDto &ticket)
{
    updateSelectedSystemTicketCard(ticket);
}

void KeyManagePage::onHttpServerLogAppended(const QString &text)
{
    ui->httpServerLogText->append(text);
}

void KeyManagePage::onHttpClientLogAppended(const QString &text)
{
    ui->httpClientLogText->append(text);
}

// ====================================================================
// HTTP Tab
// ====================================================================

void KeyManagePage::onClearHttpClient()
{
    m_controller->onClearHttpClientLogsClicked();
    ui->httpClientLogText->clear();
}

void KeyManagePage::onClearHttpServer()
{
    m_controller->onClearHttpServerLogsClicked();
    ui->httpServerLogText->clear();
}

void KeyManagePage::onSystemTicketSelectionChanged()
{
    const int row = ui->systemTicketTable->currentRow();
    m_controller->onSystemTicketSelected(row);
}

void KeyManagePage::onKeyTicketSelectionChanged()
{
    const int row = ui->keyTicketTable->currentRow();
    m_controller->onKeyTaskSelected(row);
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
    case LogCmdNone:                return QStringLiteral("--");
    case KeyProtocol::CmdInit:     return QStringLiteral("INIT");
    case static_cast<quint8>(KeyProtocol::CmdInit | 0x80): return QStringLiteral("INIT_MORE");
    case KeyProtocol::CmdSetCom:   return QStringLiteral("SET_COM");
    case KeyProtocol::CmdQTask:    return QStringLiteral("Q_TASK");
    case KeyProtocol::CmdITaskLog: return QStringLiteral("I_TASK_LOG");
    case KeyProtocol::CmdDel:      return QStringLiteral("DEL");
    case KeyProtocol::CmdDownloadRfid:return QStringLiteral("DN_RFID");
    case static_cast<quint8>(KeyProtocol::CmdDownloadRfid | 0x80): return QStringLiteral("DN_RFID_MORE");
    case KeyProtocol::CmdTicket:   return QStringLiteral("TICKET");
    case KeyProtocol::CmdTicketMore:return QStringLiteral("TICKET_MORE");
    case KeyProtocol::CmdUpTaskLog:return QStringLiteral("UP_TASK_LOG");
    case KeyProtocol::CmdAck:      return QStringLiteral("ACK");
    case KeyProtocol::CmdNak:      return QStringLiteral("NAK");
    case KeyProtocol::CmdKeyEvent: return QStringLiteral("KEY_EVT");
    default:
        return QStringLiteral("0x%1").arg(cmd, 2, 16, QLatin1Char('0')).toUpper();
    }
}

void KeyManagePage::populateSystemTicketTable(const QList<SystemTicketDto> &tickets)
{
    const QString prevSelectedTaskId = ui->systemTicketTable->currentRow() >= 0
            ? ui->systemTicketTable->item(ui->systemTicketTable->currentRow(), 0)->data(Qt::UserRole).toString()
            : QString();
    ui->systemTicketTable->setRowCount(0);
    for (int i = 0; i < tickets.size(); ++i) {
        const SystemTicketDto &ticket = tickets.at(i);
        const int row = ui->systemTicketTable->rowCount();
        ui->systemTicketTable->insertRow(row);
        QTableWidgetItem *noItem = new QTableWidgetItem(QString::number(i + 1));
        noItem->setTextAlignment(Qt::AlignCenter);
        ui->systemTicketTable->setItem(row, 0, noItem);
        ui->systemTicketTable->setItem(row, 1, new QTableWidgetItem(ticket.ticketNo));
        QTableWidgetItem *typeItem = new QTableWidgetItem(ticketTypeText(ticket.taskType));
        typeItem->setTextAlignment(Qt::AlignCenter);
        ui->systemTicketTable->setItem(row, 2, typeItem);
        QTableWidgetItem *sourceItem = new QTableWidgetItem(QStringLiteral("工作台"));
        sourceItem->setTextAlignment(Qt::AlignCenter);
        ui->systemTicketTable->setItem(row, 3, sourceItem);
        const QString displayState = (ticket.returnState.isEmpty() || ticket.returnState == QLatin1String("idle"))
                ? ticket.transferState
                : ticket.returnState;
        QTableWidgetItem *stateItem = new QTableWidgetItem(ticketStateText(displayState));
        stateItem->setTextAlignment(Qt::AlignCenter);
        ui->systemTicketTable->setItem(row, 4, stateItem);
        ui->systemTicketTable->item(row, 0)->setData(Qt::UserRole, ticket.taskId);
        if (!prevSelectedTaskId.isEmpty() && prevSelectedTaskId == ticket.taskId)
            ui->systemTicketTable->selectRow(row);
    }

    if (tickets.isEmpty()) {
        updateSelectedSystemTicketCard(SystemTicketDto{});
    } else if (ui->systemTicketTable->currentRow() >= 0) {
        m_controller->onSystemTicketSelected(ui->systemTicketTable->currentRow());
    }
}

void KeyManagePage::updateSelectedSystemTicketCard(const SystemTicketDto &ticket)
{
    if (!ticket.valid) {
        ui->lblTicketNo->setText(QStringLiteral("票号: <font color='#2E7D32'>未选择</font>"));
        ui->lblTicketType->setText(QStringLiteral("类型: --"));
        ui->lblTicketPosition->setText(QStringLiteral("位置: --"));
        ui->lblTicketReturnStatus->setText(QStringLiteral("传票状态: <font color='#C62828'>未选择系统票</font>"));
        ui->lblTicketPosition->setToolTip(QString());
        ui->btnReturnTicket->setEnabled(false);
        ui->lblReturnHint->setText(QStringLiteral("回传在钥匙任务完成后自动触发；接口未配置时不会上传"));
        return;
    }

    QString stateColor = "#C62828";
    if (ticket.returnState == QLatin1String("return-requesting-log")
            || ticket.returnState == QLatin1String("return-uploading")) {
        stateColor = "#1565C0";
    } else if (ticket.returnState == QLatin1String("return-success")) {
        stateColor = "#2E7D32";
    } else if (ticket.returnState == QLatin1String("return-failed")) {
        stateColor = "#C62828";
    } else if (ticket.transferState == QLatin1String("received")) {
        stateColor = "#C62828";
    } else if (ticket.transferState == QLatin1String("auto-pending")) {
        stateColor = "#1565C0";
    } else if (ticket.transferState == QLatin1String("sending")) {
        stateColor = "#EF6C00";
    } else if (ticket.transferState == QLatin1String("success")) {
        stateColor = "#2E7D32";
    } else if (ticket.transferState == QLatin1String("key-cleared")) {
        stateColor = "#1565C0";
    } else if (ticket.transferState == QLatin1String("failed")) {
        stateColor = "#C62828";
    }

    ui->lblTicketNo->setText(QStringLiteral("票号: <font color='#2E7D32'>%1</font>").arg(ticket.ticketNo));
    ui->lblTicketType->setText(QStringLiteral("类型: %1  |  步骤: %2")
                               .arg(ticketTypeText(ticket.taskType))
                               .arg(ticket.stepNum));
    const QString positionText = ticketPositionText(ticket.ticketNo, ticket.taskName);
    ui->lblTicketPosition->setText(QStringLiteral("位置: %1").arg(positionText));
    const QString statusText = ticketStateDescription(ticket);
    ui->lblTicketReturnStatus->setText(
        QStringLiteral("传票状态: <font color='%1'>%2</font>").arg(stateColor, statusText));

    const bool canManualReturn = ticket.transferState == QLatin1String("success")
            && ticket.returnState != QLatin1String("return-success")
            && ticket.returnState != QLatin1String("return-requesting-log")
            && ticket.returnState != QLatin1String("return-uploading");
    ui->btnReturnTicket->setEnabled(canManualReturn);

    if (ticket.returnState == QLatin1String("return-failed")) {
        ui->lblReturnHint->setText(QStringLiteral("自动回传失败，可先读取钥匙票列表，再手动点击“回传(重试)”"));
    } else if (ticket.returnState == QLatin1String("return-requesting-log")
               || ticket.returnState == QLatin1String("return-uploading")) {
        ui->lblReturnHint->setText(QStringLiteral("回传进行中，请等待当前回传链完成"));
    } else if (canManualReturn) {
        ui->lblReturnHint->setText(QStringLiteral("该任务允许手动回传；若自动链未触发，可先读取钥匙票列表再执行"));
    } else {
        ui->lblReturnHint->setText(QStringLiteral("回传在钥匙任务完成后自动触发；接口未配置时不会上传"));
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
        QTableWidgetItem *noItem = new QTableWidgetItem(QString::number(i + 1));
        noItem->setTextAlignment(Qt::AlignCenter);
        ui->keyTicketTable->setItem(row, 0, noItem);
        ui->keyTicketTable->setItem(row, 1, new QTableWidgetItem(taskIdHex.left(16)));
        QTableWidgetItem *typeItem = new QTableWidgetItem(QStringLiteral("类型%1").arg(task.type));
        typeItem->setTextAlignment(Qt::AlignCenter);
        ui->keyTicketTable->setItem(row, 2, typeItem);
        QTableWidgetItem *sourceItem = new QTableWidgetItem(QStringLiteral("来源%1").arg(task.source));
        sourceItem->setTextAlignment(Qt::AlignCenter);
        ui->keyTicketTable->setItem(row, 3, sourceItem);
        QTableWidgetItem *stateItem = new QTableWidgetItem(keyTaskStatusText(task.status));
        stateItem->setTextAlignment(Qt::AlignCenter);
        ui->keyTicketTable->setItem(row, 4, stateItem);
    }
}

void KeyManagePage::refreshSystemTicketViews()
{
    populateSystemTicketTable(m_controller->systemTickets());
    ui->httpClientLogText->setPlainText(m_controller->httpClientLogText());
    ui->httpServerLogText->setPlainText(m_controller->httpServerLogText());
}

void KeyManagePage::updateCommIndicators(const KeySessionSnapshot &snapshot)
{
    refreshCommSeatLabel();

    if (!snapshot.connected) {
        ui->lblCommStatus->setText(QStringLiteral("通讯: <font color='#C62828'>断开</font>"));
        ui->lblCommParams->setText(QStringLiteral("串口: -- OFF"));
        ui->lblKeyPosition->setText(QStringLiteral("在位: <font color='#EF6C00'>未插</font>"));
        ui->lblTaskInfo->setText(QStringLiteral("任务数: --"));
        ui->lblCommStatus->setToolTip(QStringLiteral("串口未连接，当前无法与底座通讯"));
        return;
    }

    const QString activePort = !snapshot.verifiedPortName.isEmpty()
            ? snapshot.verifiedPortName
            : snapshot.portName;
    ui->lblCommParams->setText(QStringLiteral("串口: %1 ON")
                                   .arg(activePort.isEmpty() ? QStringLiteral("--") : activePort));

    if (!snapshot.keyPresent) {
        ui->lblCommStatus->setText(QStringLiteral("通讯: <font color='#EF6C00'>待钥匙</font>"));
        ui->lblKeyPosition->setText(QStringLiteral("在位: <font color='#EF6C00'>未插</font>"));
        ui->lblTaskInfo->setText(QStringLiteral("任务数: --"));
        ui->lblCommStatus->setToolTip(QStringLiteral("串口已连接，底座在线，但当前未检测到钥匙在位"));
        return;
    }

    if (!snapshot.keyStable) {
        ui->lblCommStatus->setText(QStringLiteral("通讯: <font color='#EF6C00'>稳定中</font>"));
        ui->lblKeyPosition->setText(QStringLiteral("在位: <font color='#EF6C00'>已插（稳定中）</font>"));
        ui->lblCommStatus->setToolTip(QStringLiteral("已检测到钥匙，但接触仍在稳定窗口内，请保持底座接触稳定"));
        return;
    }

    if (!snapshot.sessionReady) {
        if (snapshot.protocolConfirmedOnce) {
            ui->lblCommStatus->setText(QStringLiteral("通讯: <font color='#EF6C00'>恢复中</font>"));
            ui->lblCommStatus->setToolTip(QStringLiteral("此前已确认过业务通讯，当前正在重新握手恢复"));
        } else {
            ui->lblCommStatus->setText(QStringLiteral("通讯: <font color='#1565C0'>待握手</font>"));
            ui->lblCommStatus->setToolTip(QStringLiteral("钥匙已稳定，但尚未完成通讯握手"));
        }
        ui->lblKeyPosition->setText(QStringLiteral("在位: <font color='#2E7D32'>已插（已稳定）</font>"));
        return;
    }

    if (!snapshot.protocolHealthy) {
        if (snapshot.protocolConfirmedOnce) {
            ui->lblCommStatus->setText(QStringLiteral("通讯: <font color='#EF6C00'>恢复中</font>"));
            ui->lblCommStatus->setToolTip(QStringLiteral("此前已确认过业务通讯，但近期出现超时/接触波动，系统正在自动恢复"));
        } else {
            ui->lblCommStatus->setText(QStringLiteral("通讯: <font color='#EF6C00'>待确认</font>"));
            ui->lblCommStatus->setToolTip(QStringLiteral("握手已完成，但尚未拿到第一次真实业务响应"));
        }
        ui->lblKeyPosition->setText(QStringLiteral("在位: <font color='#2E7D32'>已插（已稳定）</font>"));
        return;
    }

    ui->lblCommStatus->setText(QStringLiteral("通讯: <font color='#2E7D32'>钥匙已就绪</font>"));
    ui->lblKeyPosition->setText(QStringLiteral("在位: <font color='#2E7D32'>已插（可通讯）</font>"));
    ui->lblCommStatus->setToolTip(QStringLiteral("已完成握手，并已收到一次真实业务响应，可执行传票/回传等操作"));
}

void KeyManagePage::refreshCommSeatLabel()
{
    const QString stationId = ConfigManager::instance()->stationId().trimmed();
    if (stationId.isEmpty()) {
        ui->lblCommSeatNo->setText(QStringLiteral("当前配置座号: --"));
        ui->lblCommSeatNo->setToolTip(QStringLiteral("当前未配置站号/座号"));
        return;
    }

    ui->lblCommSeatNo->setText(QStringLiteral("当前配置座号: %1").arg(stationId));
    ui->lblCommSeatNo->setToolTip(QStringLiteral("显示当前配置的站号/座号，用于协议站号映射，不是底座自动探测值"));
}

bool KeyManagePage::shouldSuppressStatusBarMessage(const QString &message)
{
    const QString text = message.trimmed();
    if (text.isEmpty()) {
        return true;
    }

    return text == QLatin1String("钥匙已稳定，等待通讯确认")
            || text == QLatin1String("钥匙已就绪，可发送 Q_TASK/DEL")
            || text == QLatin1String("自动传票已开启，但当前没有待发送系统票");
}

QString KeyManagePage::ticketTypeText(int taskType)
{
    switch (taskType) {
    case 0: return QStringLiteral("停电");
    case 1: return QStringLiteral("送电");
    default: return QStringLiteral("其他");
    }
}

QString KeyManagePage::keyTaskStatusText(quint8 status)
{
    switch (status) {
    case 0x00: return QStringLiteral("未操作");
    case 0x01: return QStringLiteral("操作中");
    case 0x02: return QStringLiteral("操作完成");
    case 0x04: return QStringLiteral("已删除(钥匙)");
    case 0x05: return QStringLiteral("已删除(图形)");
    default:
        return QStringLiteral("状态0x%1").arg(status, 2, 16, QLatin1Char('0')).toUpper();
    }
}

QString KeyManagePage::ticketStateText(const QString &state)
{
    if (state == QLatin1String("return-requesting-log"))
        return QStringLiteral("钥匙已完成，正在读取回传日志");
    if (state == QLatin1String("return-uploading"))
        return QStringLiteral("日志已读取，正在回传到服务端");
    if (state == QLatin1String("return-success"))
        return QStringLiteral("已回传到系统，并准备清理钥匙任务");
    if (state == QLatin1String("return-failed"))
        return QStringLiteral("回传失败，请查看HTTP客户端报文");
    if (state == QLatin1String("received"))
        return QStringLiteral("主程序已接收，尚未开始传票");
    if (state == QLatin1String("auto-pending"))
        return QStringLiteral("等待自动传票（钥匙/握手未就绪）");
    if (state == QLatin1String("sending"))
        return QStringLiteral("正在传票到钥匙");
    if (state == QLatin1String("success"))
        return QStringLiteral("已传票到钥匙，等待设备后续执行");
    if (state == QLatin1String("key-cleared"))
        return QStringLiteral("钥匙任务已删除，可再次传票");
    if (state == QLatin1String("failed"))
        return QStringLiteral("传票失败，请查看串口报文");
    return state;
}

QString KeyManagePage::ticketStateDescription(const SystemTicketDto &ticket)
{
    if (ticket.returnState == QLatin1String("return-requesting-log")) {
        return QStringLiteral("钥匙任务已完成，主程序正在读取回传日志");
    }
    if (ticket.returnState == QLatin1String("return-uploading")) {
        return QStringLiteral("主程序已读取钥匙日志，正在回传到服务端");
    }
    if (ticket.returnState == QLatin1String("return-success")) {
        return QStringLiteral("主程序已完成回传上传，钥匙任务将进入清理流程");
    }
    if (ticket.returnState == QLatin1String("return-failed")) {
        if (!ticket.returnError.trimmed().isEmpty()) {
            return QStringLiteral("回传失败：%1").arg(ticket.returnError);
        }
        return QStringLiteral("回传失败，请查看HTTP客户端报文");
    }
    if (ticket.transferState == QLatin1String("received")) {
        return QStringLiteral("主程序已接收到工作台JSON，尚未开始向钥匙传票");
    }
    if (ticket.transferState == QLatin1String("auto-pending")) {
        return QStringLiteral("主程序已接收JSON，等待串口连接和钥匙就绪后自动传票");
    }
    if (ticket.transferState == QLatin1String("sending")) {
        return QStringLiteral("主程序已开始向钥匙发送传票");
    }
    if (ticket.transferState == QLatin1String("success")) {
        return QStringLiteral("主程序已完成传票发送；同一任务再次触发钥匙传票时将默认忽略重复下发");
    }
    if (ticket.transferState == QLatin1String("key-cleared")) {
        return QStringLiteral("钥匙中的同任务已被删除；当前系统票可再次手动传票，也可在工作台重新触发后再次自动传票");
    }
    if (ticket.transferState == QLatin1String("failed")) {
        if (!ticket.lastError.trimmed().isEmpty()) {
            return QStringLiteral("传票失败：%1").arg(ticket.lastError);
        }
        return QStringLiteral("传票失败，请查看串口报文日志");
    }
    return ticket.transferState;
}

QString KeyManagePage::ticketPositionText(const QString &ticketNo, const QString &taskName)
{
    return extractPositionFromTaskName(ticketNo, taskName);
}

void KeyManagePage::updateStatusBar(const QString &message)
{
    ui->lblFeedback->setText(message);
}
