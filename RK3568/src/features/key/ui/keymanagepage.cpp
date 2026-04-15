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
#include <QDir>
#include <QFileDialog>
#include <QHideEvent>
#include <QHeaderView>
#include <QMessageBox>
#include <QPalette>
#include <QSignalBlocker>
#include <QShowEvent>
#include <QStringList>
#include <QTableWidgetItem>
#include <QTimer>

#include "core/AppContext.h"
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
    , m_uiFlushTimer(new QTimer(this))
    , m_pendingSystemTicketRefresh(false)
    , m_pendingKeyTaskRefresh(false)
    , m_pendingHttpClientRefresh(false)
    , m_pendingHttpServerRefresh(false)
    , m_pageVisible(false)
{
    ui->setupUi(this);
    m_uiFlushTimer->setSingleShot(true);
    m_uiFlushTimer->setInterval(120);
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

void KeyManagePage::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    m_pageVisible = true;
    scheduleUiFlush();
}

void KeyManagePage::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);
    m_pageVisible = false;
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
    ui->btnClearOrphanKeyTask->setToolTip(QStringLiteral("清除钥匙中的未登记任务；需先读取钥匙票列表，选中孤儿任务后方可操作"));
    ui->btnForceCleanFailedTicket->setToolTip(QStringLiteral("强制删除本地失败系统票记录；仅限 TerminationFailed / return-delete-failed / manual-required 状态；不会操作钥匙或通知工作台"));
    ui->lblReturnHint->setText(QStringLiteral("回传按任务独立触发；当前任务完成后可自动或手动回传；接口未配置时不会上传"));

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
    ui->httpClientLogText->document()->setMaximumBlockCount(1500);
    ui->httpServerLogText->document()->setMaximumBlockCount(1500);
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
    connect(ui->btnClearOrphanKeyTask,    &QPushButton::clicked, this, &KeyManagePage::onClearOrphanKeyTask);
    connect(ui->btnForceCleanFailedTicket, &QPushButton::clicked, this, &KeyManagePage::onForceCleanFailedTicket);
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
    connect(m_uiFlushTimer, &QTimer::timeout, this, &KeyManagePage::flushPendingUiRefreshes);

    connect(AppContext::instance(), &AppContext::userLoggedIn, this, [this](const UserInfo &user) {
        m_isAdmin = user.role.contains(QStringLiteral("admin"), Qt::CaseInsensitive);
        updateAdminButtons();
    });
    connect(AppContext::instance(), &AppContext::userLoggedOut, this, [this]() {
        m_isAdmin = false;
        updateAdminButtons();
    });
    // 初始化：页面构造时若已登录则读取当前角色
    m_isAdmin = AppContext::instance()->currentUser().role.contains(
            QStringLiteral("admin"), Qt::CaseInsensitive);
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

    connect(m_controller, &KeyManageController::workbenchRefreshRequested,
            this, &KeyManagePage::workbenchRefreshNeeded);

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
    if (index == 0 || index == 1 || index == 2 || index == 3) {
        scheduleUiFlush();
    }
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
    m_controller->onQueryBatteryClicked();
}

void KeyManagePage::onQueryTaskCount()
{
    m_controller->onQueryTasksClicked();
    updateStatusBar(QStringLiteral("执行 Q_TASK 诊断查询"));
}

void KeyManagePage::onSyncDeviceTime()
{
    m_controller->onSyncDeviceTimeClicked();
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

void KeyManagePage::onForceCleanFailedTicket()
{
    // 根据当前票的失败状态给出针对性的风险提示
    QString extraWarning;
    const SystemTicketDto &t = m_selectedSystemTicket;
    if (t.valid) {
        const bool keyTaskMayStillExist =
                (t.returnState == QLatin1String("return-delete-failed"))
                || (t.returnState == QLatin1String("manual-required"));
        if (keyTaskMayStillExist) {
            extraWarning = QStringLiteral(
                "· 对应钥匙任务可能仍存在于钥匙中；\n"
                "  下次读取钥匙后会重新出现为未登记任务，届时可使用\"清除未登记任务\"处理\n");
        } else {
            // transferState == "failed" 或 TerminationFailed：钥匙侧已无此任务
            extraWarning = QStringLiteral(
                "· 对应钥匙任务已不存在，不会重新出现为未登记任务\n");
        }
    }

    const QMessageBox::StandardButton reply = QMessageBox::warning(
        this,
        QStringLiteral("强制清理失败系统票"),
        QStringLiteral("确认要强制删除这条失败的系统票记录吗？\n\n注意：\n")
            + QStringLiteral("· 此操作仅删除本地记录，不会通知工作台\n")
            + extraWarning
            + QStringLiteral("· 请确认已了解当前状态后再操作"),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    if (reply != QMessageBox::Yes) {
        return;
    }
    m_controller->onForceCleanFailedTicketClicked();
}

void KeyManagePage::onClearOrphanKeyTask()
{
    const QMessageBox::StandardButton reply = QMessageBox::warning(
        this,
        QStringLiteral("清除未登记任务"),
        QStringLiteral("确认从钥匙中清除该未登记任务？\n\n此操作将直接发送 DEL 命令删除钥匙中的任务，无法撤销。"),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    if (reply != QMessageBox::Yes) {
        return;
    }
    m_controller->onClearOrphanKeyTaskClicked();
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
#ifdef Q_OS_LINUX
    const QString exportDir = QCoreApplication::applicationDirPath() + QStringLiteral("/logs/serial");
    QDir().mkpath(exportDir);
    const QString defaultPath = exportDir + QChar('/') + defaultName;
#else
    const QString defaultPath = defaultName;
#endif
    const QString path = QFileDialog::getSaveFileName(this,
                                                      QStringLiteral("导出串口日志"),
                                                      defaultPath,
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
    emit sessionSnapshotForwarded(snapshot);
}

void KeyManagePage::onTasksUpdated(const QList<KeyTaskDto> &tasks)
{
    ui->lblTaskInfo->setText(QStringLiteral("任务数: %1").arg(tasks.size()));
    m_latestKeyTasks = tasks;
    m_pendingKeyTaskRefresh = true;
    scheduleUiFlush();
    updateClearOrphanButton();
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
    m_pendingSerialLogs.append(item);
    scheduleUiFlush();
}

void KeyManagePage::onLogTableRefreshRequested()
{
    m_pendingSerialLogs.clear();
    if (m_pageVisible && isSerialTabVisible()) {
        refreshSerialLogTable();
    }
}

void KeyManagePage::onLogsCleared()
{
    m_pendingSerialLogs.clear();
    ui->serialLogTable->setRowCount(0);
}

void KeyManagePage::onSystemTicketsUpdated(const QList<SystemTicketDto> &tickets)
{
    Q_UNUSED(tickets);
    m_pendingSystemTicketRefresh = true;
    scheduleUiFlush();
    updateClearOrphanButton();
    updateForceCleanButton();
}

void KeyManagePage::onSelectedSystemTicketChanged(const SystemTicketDto &ticket)
{
    m_selectedSystemTicket = ticket;
    updateSelectedSystemTicketCard(ticket);
    updateForceCleanButton();
}

void KeyManagePage::onHttpServerLogAppended(const QString &text)
{
    Q_UNUSED(text);
    m_pendingHttpServerRefresh = true;
    scheduleUiFlush();
}

void KeyManagePage::onHttpClientLogAppended(const QString &text)
{
    Q_UNUSED(text);
    m_pendingHttpClientRefresh = true;
    scheduleUiFlush();
}

// ====================================================================
// HTTP Tab
// ====================================================================

void KeyManagePage::onClearHttpClient()
{
    m_controller->onClearHttpClientLogsClicked();
    m_pendingHttpClientRefresh = false;
    ui->httpClientLogText->clear();
}

void KeyManagePage::onClearHttpServer()
{
    m_controller->onClearHttpServerLogsClicked();
    m_pendingHttpServerRefresh = false;
    ui->httpServerLogText->clear();
}

void KeyManagePage::onSystemTicketSelectionChanged()
{
    const int row = ui->systemTicketTable->currentRow();
    m_controller->onSystemTicketSelected(row);
    updateForceCleanButton();
}

void KeyManagePage::onKeyTicketSelectionChanged()
{
    const int row = ui->keyTicketTable->currentRow();
    m_controller->onKeyTaskSelected(row);
    updateClearOrphanButton();

    if (row < 0 || row >= m_latestKeyTasks.size()) {
        return;
    }

    const QString selectedTaskId = rawTaskIdToDecimalString(m_latestKeyTasks.at(row).taskId);
    if (selectedTaskId.isEmpty()) {
        return;
    }

    int matchedSystemRow = -1;
    for (int i = 0; i < ui->systemTicketTable->rowCount(); ++i) {
        QTableWidgetItem *noItem = ui->systemTicketTable->item(i, 0);
        if (!noItem) {
            continue;
        }
        if (noItem->data(Qt::UserRole).toString() == selectedTaskId) {
            matchedSystemRow = i;
            break;
        }
    }

    if (matchedSystemRow < 0) {
        // 选中的是未登记孤儿任务，系统票侧无对应行，主动清空系统票选区避免左侧显示残留
        if (m_controller->isSelectedKeyTaskOrphan()) {
            const QSignalBlocker blocker(ui->systemTicketTable);
            ui->systemTicketTable->clearSelection();
            m_controller->onSystemTicketSelected(-1);
            updateSelectedSystemTicketCard(SystemTicketDto{});
        }
        return;
    }

    if (ui->systemTicketTable->currentRow() == matchedSystemRow) {
        m_controller->onSystemTicketSelected(matchedSystemRow);
        return;
    }

    const QSignalBlocker blocker(ui->systemTicketTable);
    ui->systemTicketTable->selectRow(matchedSystemRow);
    m_controller->onSystemTicketSelected(matchedSystemRow);
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
}

void KeyManagePage::refreshSerialLogTable()
{
    QTableWidget *t = ui->serialLogTable;
    t->setUpdatesEnabled(false);
    t->setRowCount(0);

    const QList<LogItem> visible = m_controller->visibleLogs();
    for (const LogItem &item : visible) {
        appendSerialLogRow(item);
    }
    if (m_showHex && !t->isColumnHidden(6)) {
        t->resizeRowsToContents();
    }
    t->setUpdatesEnabled(true);
    t->scrollToBottom();
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
    case KeyProtocol::CmdSetTime:  return QStringLiteral("SET_TIME");
    case KeyProtocol::CmdQTask:    return QStringLiteral("Q_TASK");
    case KeyProtocol::CmdQKeyEq:   return QStringLiteral("Q_KEYEQ");
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
        // 孤儿占位票不在系统票表中显示，仅在钥匙票表中标注
        if (ticket.transferState == QLatin1String("orphan-recovered")) {
            continue;
        }
        const int row = ui->systemTicketTable->rowCount();
        ui->systemTicketTable->insertRow(row);
        QTableWidgetItem *noItem = new QTableWidgetItem(QString::number(row + 1));
        noItem->setTextAlignment(Qt::AlignCenter);
        ui->systemTicketTable->setItem(row, 0, noItem);
        ui->systemTicketTable->setItem(row, 1, new QTableWidgetItem(ticket.ticketNo));
        QTableWidgetItem *typeItem = new QTableWidgetItem(ticketTypeText(ticket.taskType));
        typeItem->setTextAlignment(Qt::AlignCenter);
        ui->systemTicketTable->setItem(row, 2, typeItem);
        QTableWidgetItem *sourceItem = new QTableWidgetItem(QStringLiteral("工作台"));
        sourceItem->setTextAlignment(Qt::AlignCenter);
        ui->systemTicketTable->setItem(row, 3, sourceItem);
        QString displayState;
        if (ticket.adminDeleteStage == SystemTicketDto::AdminDeleteStage::TerminationFailed) {
            displayState = QStringLiteral("admin-delete-failed");
        } else if (ticket.adminDeleteStage == SystemTicketDto::AdminDeleteStage::KeyClearedAwaitingTermination) {
            displayState = QStringLiteral("admin-delete-pending");
        } else {
            displayState = (ticket.returnState.isEmpty() || ticket.returnState == QLatin1String("idle"))
                    ? ticket.transferState
                    : ticket.returnState;
        }
        QTableWidgetItem *stateItem = new QTableWidgetItem(ticketStateText(displayState));
        stateItem->setTextAlignment(Qt::AlignCenter);
        ui->systemTicketTable->setItem(row, 4, stateItem);
        ui->systemTicketTable->item(row, 0)->setData(Qt::UserRole, ticket.taskId);
        if (!prevSelectedTaskId.isEmpty() && prevSelectedTaskId == ticket.taskId)
            ui->systemTicketTable->selectRow(row);
    }

    if (ui->systemTicketTable->rowCount() == 0) {
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
        ui->lblReturnHint->setText(QStringLiteral("回传按任务独立触发；当前任务完成后可自动或手动回传；接口未配置时不会上传"));
        return;
    }

    QString stateColor = "#C62828";
    if (isReturnInFlightState(ticket.returnState)) {
        stateColor = "#1565C0";
    } else if (isReturnCleanupState(ticket.returnState)) {
        stateColor = "#EF6C00";
    } else if (ticket.returnState == QLatin1String("return-delete-success")) {
        stateColor = "#2E7D32";
    } else if (ticket.returnState == QLatin1String("return-success")) {
        stateColor = "#2E7D32";
    } else if (ticket.returnState == QLatin1String("return-interrupted-retryable")) {
        stateColor = "#EF6C00";
    } else if (ticket.returnState == QLatin1String("manual-required")
               || ticket.returnState == QLatin1String("upload_uncertain")) {
        stateColor = "#C62828";
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

    quint8 keyTaskStatus = 0xFF;
    const bool hasSelectedKeyTask = findKeyTaskStatusForTicket(ticket.taskId, &keyTaskStatus);
    QString manualReturnBlockedReason;
    const bool canManualReturn = m_controller->canStartManualReturn(ticket.taskId, &manualReturnBlockedReason);
    ui->btnReturnTicket->setEnabled(canManualReturn && m_isAdmin);

    if (ticket.returnState == QLatin1String("return-delete-failed")) {
        if (!ticket.returnError.trimmed().isEmpty()) {
            ui->lblReturnHint->setText(QStringLiteral("钥匙删除验证失败：%1").arg(ticket.returnError));
        } else {
            ui->lblReturnHint->setText(QStringLiteral("钥匙删除验证失败，请先确认钥匙状态后再决定是否手动重试"));
        }
        return;
    }
    if (ticket.transferState == QLatin1String("success") && keyTaskStatus != 0x02) {
        ui->lblReturnHint->setText(QStringLiteral("当前选中任务在钥匙中尚未完成，完成后才允许回传"));
        return;
    }
    if (canManualReturn) {
        ui->lblReturnHint->setText(QStringLiteral("当前选中任务已完成，可执行手动回传；其他未完成任务不影响本任务回传"));
        return;
    }
    if (ticket.transferState == QLatin1String("success") && !manualReturnBlockedReason.trimmed().isEmpty()) {
        ui->lblReturnHint->setText(manualReturnBlockedReason);
        return;
    }

    ui->lblReturnHint->setText(QStringLiteral("手动回传仅要求当前选中任务完成；接口未配置时不会上传"));
}

void KeyManagePage::populateKeyTicketTable(const QList<KeyTaskDto> &tasks)
{
    const QList<SystemTicketDto> sysTickets = m_controller->systemTickets();
    ui->keyTicketTable->setRowCount(0);
    for (int i = 0; i < tasks.size(); ++i) {
        const KeyTaskDto &task = tasks.at(i);
        const int row = ui->keyTicketTable->rowCount();
        ui->keyTicketTable->insertRow(row);

        const QString taskDecimalId = rawTaskIdToDecimalString(task.taskId);
        bool isOrphan = false;
        for (const SystemTicketDto &st : sysTickets) {
            if (st.taskId == taskDecimalId
                    && st.transferState == QLatin1String("orphan-recovered")) {
                isOrphan = true;
                break;
            }
        }

        const QString taskIdHex = QString(task.taskId.toHex().toUpper());
        QTableWidgetItem *noItem = new QTableWidgetItem(QString::number(i + 1));
        noItem->setTextAlignment(Qt::AlignCenter);
        ui->keyTicketTable->setItem(row, 0, noItem);
        // 未登记任务在任务ID列追加标注
        const QString taskIdDisplay = isOrphan
                ? taskIdHex.left(16) + QStringLiteral(" [未登记]")
                : taskIdHex.left(16);
        ui->keyTicketTable->setItem(row, 1, new QTableWidgetItem(taskIdDisplay));
        QTableWidgetItem *typeItem = new QTableWidgetItem(QStringLiteral("类型%1").arg(task.type));
        typeItem->setTextAlignment(Qt::AlignCenter);
        ui->keyTicketTable->setItem(row, 2, typeItem);
        QTableWidgetItem *sourceItem = new QTableWidgetItem(QStringLiteral("来源%1").arg(task.source));
        sourceItem->setTextAlignment(Qt::AlignCenter);
        ui->keyTicketTable->setItem(row, 3, sourceItem);
        QTableWidgetItem *stateItem = new QTableWidgetItem(keyTaskStatusText(task.status));
        stateItem->setTextAlignment(Qt::AlignCenter);
        ui->keyTicketTable->setItem(row, 4, stateItem);

        // 未登记任务行用淡橙色背景与正常任务区分
        if (isOrphan) {
            const QColor orphanBg(0xFF, 0xF0, 0xE0);
            for (int col = 0; col < ui->keyTicketTable->columnCount(); ++col) {
                QTableWidgetItem *it = ui->keyTicketTable->item(row, col);
                if (it) it->setBackground(orphanBg);
            }
        }
    }
}

void KeyManagePage::refreshSystemTicketViews()
{
    populateSystemTicketTable(m_controller->systemTickets());
    ui->httpClientLogText->setPlainText(m_controller->httpClientLogText());
    ui->httpServerLogText->setPlainText(m_controller->httpServerLogText());
}

void KeyManagePage::scheduleUiFlush()
{
    if (!m_pageVisible) {
        return;
    }

    if (!m_uiFlushTimer->isActive()) {
        m_uiFlushTimer->start();
    }
}

void KeyManagePage::flushPendingUiRefreshes()
{
    if (!m_pageVisible) {
        return;
    }

    if (m_pendingSystemTicketRefresh && ui->contentStack->currentIndex() != 1) {
        populateSystemTicketTable(m_controller->systemTickets());
        if (ui->systemTicketTable->currentRow() >= 0) {
            m_controller->onSystemTicketSelected(ui->systemTicketTable->currentRow());
        }
        m_pendingSystemTicketRefresh = false;
    }

    if (m_pendingKeyTaskRefresh && ui->contentStack->currentIndex() == 0) {
        populateKeyTicketTable(m_latestKeyTasks);
        m_pendingKeyTaskRefresh = false;
    }

    if (m_pendingHttpClientRefresh && isHttpClientTabVisible()) {
        ui->httpClientLogText->setPlainText(m_controller->httpClientLogText());
        m_pendingHttpClientRefresh = false;
    }

    if (m_pendingHttpServerRefresh && isHttpServerTabVisible()) {
        ui->httpServerLogText->setPlainText(m_controller->httpServerLogText());
        m_pendingHttpServerRefresh = false;
    }

    flushPendingSerialLogs();
}

void KeyManagePage::flushPendingSerialLogs()
{
    if (!m_pageVisible || !isSerialTabVisible() || m_pendingSerialLogs.isEmpty()) {
        return;
    }

    QTableWidget *table = ui->serialLogTable;
    table->setUpdatesEnabled(false);
    const QList<LogItem> pending = m_pendingSerialLogs;
    m_pendingSerialLogs.clear();
    for (const LogItem &item : pending) {
        appendSerialLogRow(item);
    }
    if (m_showHex && !table->isColumnHidden(6)) {
        table->resizeRowsToContents();
    }
    table->setUpdatesEnabled(true);
    table->scrollToBottom();
}

bool KeyManagePage::isSerialTabVisible() const
{
    return ui->contentStack->currentIndex() == 1;
}

bool KeyManagePage::isHttpClientTabVisible() const
{
    return ui->contentStack->currentIndex() == 2;
}

bool KeyManagePage::isHttpServerTabVisible() const
{
    return ui->contentStack->currentIndex() == 3;
}

bool KeyManagePage::findKeyTaskStatusForTicket(const QString &taskId, quint8 *status) const
{
    for (const KeyTaskDto &task : m_latestKeyTasks) {
        if (rawTaskIdToDecimalString(task.taskId) == taskId) {
            if (status) {
                *status = task.status;
            }
            return true;
        }
    }

    if (status) {
        *status = 0xFF;
    }
    return false;
}

void KeyManagePage::updateCommIndicators(const KeySessionSnapshot &snapshot)
{
    refreshCommSeatLabel();
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    const bool recentBusinessSuccess = snapshot.lastBusinessSuccessMs > 0
            && (nowMs - snapshot.lastBusinessSuccessMs) <= 5000;
    const bool recentFailureNewer = snapshot.lastProtocolFailureMs > 0
            && (snapshot.lastProtocolFailureMs >= snapshot.lastBusinessSuccessMs);

    if (!snapshot.connected) {
        ui->lblCommStatus->setText(QStringLiteral("通讯: <font color='#C62828'>断开</font>"));
        ui->lblCommParams->setText(QStringLiteral("串口: -- OFF"));
        ui->lblKeyPosition->setText(QStringLiteral("在位: <font color='#EF6C00'>未插</font>"));
        ui->lblBatteryInfo->setText(QStringLiteral("电量: --"));
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
        // A座不在位时，按 B座优先显示电量
        if (snapshot.bKeyPresent) {
            ui->lblBatteryInfo->setText(snapshot.bBatteryPercent >= 0
                ? QStringLiteral("电量(B): %1%").arg(snapshot.bBatteryPercent)
                : QStringLiteral("电量(B): --"));
        } else {
            ui->lblBatteryInfo->setText(QStringLiteral("电量: --"));
        }
        ui->lblTaskInfo->setText(QStringLiteral("任务数: --"));
        ui->lblCommStatus->setToolTip(QStringLiteral("串口已连接，底座在线，但当前未检测到钥匙在位"));
        return;
    }

    // A座在位时，显示 A座电量
    ui->lblBatteryInfo->setText(snapshot.batteryPercent >= 0
                                    ? QStringLiteral("电量(A): %1%").arg(snapshot.batteryPercent)
                                    : QStringLiteral("电量(A): --"));

    if (!snapshot.keyStable) {
        ui->lblCommStatus->setText(QStringLiteral("通讯: <font color='#EF6C00'>稳定中</font>"));
        ui->lblKeyPosition->setText(QStringLiteral("在位: <font color='#EF6C00'>已插（稳定中）</font>"));
        ui->lblCommStatus->setToolTip(QStringLiteral("已检测到钥匙，但接触仍在稳定窗口内，请保持底座接触稳定"));
        return;
    }

    if (!snapshot.sessionReady) {
        if (snapshot.protocolConfirmedOnce || snapshot.recoveryWindowActive) {
            ui->lblCommStatus->setText(QStringLiteral("通讯: <font color='#EF6C00'>恢复中</font>"));
            ui->lblCommStatus->setToolTip(QStringLiteral("此前已确认过业务通讯，当前正在重新握手恢复"));
        } else {
            ui->lblCommStatus->setText(QStringLiteral("通讯: <font color='#1565C0'>待握手</font>"));
            ui->lblCommStatus->setToolTip(QStringLiteral("钥匙已稳定，但尚未完成通讯握手"));
        }
        ui->lblKeyPosition->setText(QStringLiteral("在位: <font color='#2E7D32'>已插（已稳定）</font>"));
        return;
    }

    if (recentBusinessSuccess) {
        ui->lblCommStatus->setText(QStringLiteral("通讯: <font color='#2E7D32'>钥匙已就绪</font>"));
        ui->lblKeyPosition->setText(QStringLiteral("在位: <font color='#2E7D32'>已插（可通讯）</font>"));
        ui->lblCommStatus->setToolTip(QStringLiteral("最近已收到一次真实业务成功响应，可执行传票/回传等操作"));
        return;
    }

    if (!snapshot.protocolHealthy || snapshot.recoveryWindowActive || recentFailureNewer) {
        if (snapshot.protocolConfirmedOnce || recentFailureNewer) {
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
    if (state == QLatin1String("return-handshake"))
        return QStringLiteral("回传前握手中");
    if (state == QLatin1String("return-log-requesting"))
        return QStringLiteral("正在向钥匙请求回传日志");
    if (state == QLatin1String("return-log-receiving"))
        return QStringLiteral("正在接收钥匙回传日志");
    if (state == QLatin1String("return-uploading"))
        return QStringLiteral("日志已读取，正在回传到服务端");
    if (state == QLatin1String("return-upload-success"))
        return QStringLiteral("已上传回服务端");
    if (state == QLatin1String("return-delete-pending"))
        return QStringLiteral("服务端已收，正在清理钥匙任务");
    if (state == QLatin1String("return-delete-verifying"))
        return QStringLiteral("正在对账确认钥匙任务是否已删除");
    if (state == QLatin1String("return-delete-success"))
        return QStringLiteral("已完成回传并清理钥匙任务");
    if (state == QLatin1String("return-delete-failed"))
        return QStringLiteral("钥匙删除验证失败，需人工处理");
    if (state == QLatin1String("return-success"))
        return QStringLiteral("已回传到系统，并准备清理钥匙任务");
    if (state == QLatin1String("return-interrupted-retryable"))
        return QStringLiteral("回传中断，可在钥匙重新就绪后自动恢复");
    if (state == QLatin1String("manual-required"))
        return QStringLiteral("回传需人工确认，请查看 HTTP 客户端报文");
    if (state == QLatin1String("upload_uncertain"))
        return QStringLiteral("回传结果不确定，请人工确认是否已上传");
    if (state == QLatin1String("return-failed"))
        return QStringLiteral("回传失败，请查看HTTP客户端报文");
    if (state == QLatin1String("received"))
        return QStringLiteral("主程序已接收，尚未开始传票");
    if (state == QLatin1String("auto-pending"))
        return QStringLiteral("等待自动传票（钥匙/握手未就绪）");
    if (state == QLatin1String("orphan-recovered"))
        return QStringLiteral("发现钥匙中的未知任务，待人工确认");
    if (state == QLatin1String("sending"))
        return QStringLiteral("正在传票到钥匙");
    if (state == QLatin1String("success"))
        return QStringLiteral("已传票到钥匙，等待设备后续执行");
    if (state == QLatin1String("key-cleared"))
        return QStringLiteral("钥匙任务已删除，可再次传票");
    if (state == QLatin1String("admin-delete-pending"))
        return QStringLiteral("钥匙已清理，正在通知工作台终止");
    if (state == QLatin1String("admin-delete-failed"))
        return QStringLiteral("工作台通知失败，需手动处理");
    if (state == QLatin1String("failed"))
        return QStringLiteral("传票失败，请查看串口报文");
    return state;
}

QString KeyManagePage::ticketStateDescription(const SystemTicketDto &ticket)
{
    if (ticket.adminDeleteStage == SystemTicketDto::AdminDeleteStage::TerminationFailed) {
        const QString err = ticket.adminDeleteError.trimmed();
        return err.isEmpty()
                ? QStringLiteral("工作台终止通知失败，请手动在工作台关闭该任务")
                : QStringLiteral("工作台终止通知失败，请手动在工作台关闭该任务（错误：%1）").arg(err);
    }
    if (ticket.adminDeleteStage == SystemTicketDto::AdminDeleteStage::KeyClearedAwaitingTermination) {
        return QStringLiteral("钥匙端任务已清理，正在通知工作台终止系统票，请稍候");
    }
    if (ticket.returnState == QLatin1String("return-requesting-log")) {
        return QStringLiteral("钥匙任务已完成，主程序正在读取回传日志");
    }
    if (ticket.returnState == QLatin1String("return-handshake")) {
        return QStringLiteral("主程序正在为回传重新建立钥匙通讯握手");
    }
    if (ticket.returnState == QLatin1String("return-log-requesting")) {
        return QStringLiteral("回传握手已完成，主程序正在向钥匙请求任务回传日志");
    }
    if (ticket.returnState == QLatin1String("return-log-receiving")) {
        return QStringLiteral("主程序已开始接收钥匙回传日志，正在等待日志帧全部到齐");
    }
    if (ticket.returnState == QLatin1String("return-uploading")) {
        return QStringLiteral("主程序已读取钥匙日志，正在回传到服务端");
    }
    if (ticket.returnState == QLatin1String("return-upload-success")) {
        return QStringLiteral("主程序已完成服务端上传，准备继续清理钥匙任务");
    }
    if (ticket.returnState == QLatin1String("return-delete-pending")) {
        return QStringLiteral("服务端已确认回传，主程序正在继续清理钥匙中的已完成任务");
    }
    if (ticket.returnState == QLatin1String("return-delete-verifying")) {
        return QStringLiteral("主程序正在通过后续 Q_TASK 对账，确认钥匙中的任务是否已经删除");
    }
    if (ticket.returnState == QLatin1String("return-delete-success")) {
        return QStringLiteral("主程序已完成回传上传，并已清理钥匙中的已完成任务");
    }
    if (ticket.returnState == QLatin1String("return-delete-failed")) {
        if (!ticket.returnError.trimmed().isEmpty()) {
            return QStringLiteral("钥匙删除验证失败：%1").arg(ticket.returnError);
        }
        return QStringLiteral("钥匙删除验证失败，需人工处理或稍后手动重试");
    }
    if (ticket.returnState == QLatin1String("return-success")) {
        return QStringLiteral("主程序已完成回传上传，钥匙任务将进入清理流程");
    }
    if (ticket.returnState == QLatin1String("return-interrupted-retryable")) {
        return QStringLiteral("回传链在握手、日志请求或日志接收阶段被中断；钥匙重新就绪后应可自动继续");
    }
    if (ticket.returnState == QLatin1String("manual-required")) {
        if (!ticket.returnError.trimmed().isEmpty()) {
            return QStringLiteral("当前回传需要人工确认：%1").arg(ticket.returnError);
        }
        return QStringLiteral("当前回传需要人工确认，请先查看 HTTP 客户端报文与钥匙状态");
    }
    if (ticket.returnState == QLatin1String("upload_uncertain")) {
        return QStringLiteral("主程序无法确认服务端是否已收到本次回传，请人工确认后再决定是否重试");
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
    if (ticket.transferState == QLatin1String("orphan-recovered")) {
        return QStringLiteral("该任务来自钥匙侧孤儿恢复，当前不会自动回传或自动删除，请先人工确认来源");
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

void KeyManagePage::updateAdminButtons()
{
    ui->btnTransferTicket->setEnabled(m_isAdmin);
    ui->btnDeleteTicket->setEnabled(m_isAdmin);
    // btnReturnTicket 的可用性由 updateSelectedSystemTicketCard 综合判断（需同时满足 canManualReturn && m_isAdmin）
    // 这里只处理非管理员时强制置灰；管理员时交给票据选中逻辑决定
    if (!m_isAdmin) {
        ui->btnReturnTicket->setEnabled(false);
    }
    updateClearOrphanButton();
    updateForceCleanButton();
}

void KeyManagePage::updateClearOrphanButton()
{
    const bool canClear = m_isAdmin && m_controller->isSelectedKeyTaskOrphan();
    ui->btnClearOrphanKeyTask->setEnabled(canClear);
}

void KeyManagePage::updateForceCleanButton()
{
    const bool canForce = m_isAdmin && m_controller->isSelectedSystemTicketForceClearable();
    ui->btnForceCleanFailedTicket->setEnabled(canForce);
}

void KeyManagePage::updateStatusBar(const QString &message)
{
    ui->lblFeedback->setText(message);
}
