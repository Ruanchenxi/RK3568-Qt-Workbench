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
#include <QDialog>
#include <QDir>
#include <QFileDialog>
#include <QGridLayout>
#include <QHideEvent>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPalette>
#include <QSignalBlocker>
#include <QShowEvent>
#include <QStringList>
#include <QTableWidgetItem>
#include <QTimer>
#include <QVBoxLayout>

#include "core/AppContext.h"
#include "core/ConfigManager.h"
#include "features/key/application/KeyManageController.h"
#include "features/key/ui/TicketStatusPresenter.h"
#include "platform/logging/LogService.h"
#include "shared/ui/DesktopUiMetrics.h"

namespace {
// Uses an on-screen numeric keypad so the board can select a station without
// depending on a desktop keyboard or Qt's platform input dialog.
int requestCaptureStationNo(QWidget *parent, int defaultStationNo)
{
    QDialog dialog(parent);
    dialog.setWindowTitle(QStringLiteral("采码票"));
    dialog.setModal(true);
    dialog.setMinimumSize(460, 520);

    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(24, 20, 24, 20);
    layout->setSpacing(14);

    auto *label = new QLabel(QStringLiteral("请选择业务站号（1-255）"), &dialog);
    label->setAlignment(Qt::AlignCenter);
    label->setStyleSheet(QStringLiteral("font-size: 18px; font-weight: 600;"));
    layout->addWidget(label);

    auto *display = new QLineEdit(QString::number(defaultStationNo), &dialog);
    display->setReadOnly(true);
    display->setAlignment(Qt::AlignCenter);
    display->setMaxLength(3);
    display->setMinimumHeight(58);
    display->setStyleSheet(QStringLiteral("font-size: 26px; padding: 8px;"));
    layout->addWidget(display);

    auto *keypad = new QGridLayout;
    keypad->setHorizontalSpacing(10);
    keypad->setVerticalSpacing(10);
    layout->addLayout(keypad, 1);

    QPushButton *confirmButton = nullptr;
    const auto updateConfirmState = [display, &confirmButton]() {
        bool ok = false;
        const int value = display->text().toInt(&ok);
        if (confirmButton) confirmButton->setEnabled(ok && value >= 1 && value <= 255);
    };
    const auto addDigit = [display, updateConfirmState](int digit) {
        QString value = display->text();
        if (value.size() >= 3) return;
        value.append(QChar::fromLatin1(static_cast<char>('0' + digit)));
        display->setText(value);
        updateConfirmState();
    };
    for (int digit = 1; digit <= 9; ++digit) {
        auto *button = new QPushButton(QString::number(digit), &dialog);
        button->setMinimumSize(92, 64);
        button->setStyleSheet(QStringLiteral("font-size: 22px;"));
        keypad->addWidget(button, (digit - 1) / 3, (digit - 1) % 3);
        QObject::connect(button, &QPushButton::clicked, &dialog,
                         [addDigit, digit]() { addDigit(digit); });
    }

    auto *clearButton = new QPushButton(QStringLiteral("清除"), &dialog);
    auto *zeroButton = new QPushButton(QStringLiteral("0"), &dialog);
    auto *backspaceButton = new QPushButton(QStringLiteral("退格"), &dialog);
    for (QPushButton *button : {clearButton, zeroButton, backspaceButton}) {
        button->setMinimumSize(92, 64);
        button->setStyleSheet(QStringLiteral("font-size: 18px;"));
    }
    keypad->addWidget(clearButton, 3, 0);
    keypad->addWidget(zeroButton, 3, 1);
    keypad->addWidget(backspaceButton, 3, 2);
    QObject::connect(zeroButton, &QPushButton::clicked, &dialog,
                     [addDigit]() { addDigit(0); });
    QObject::connect(clearButton, &QPushButton::clicked, &dialog,
                     [display, updateConfirmState]() {
        display->clear();
        updateConfirmState();
    });
    QObject::connect(backspaceButton, &QPushButton::clicked, &dialog,
                     [display, updateConfirmState]() {
        display->backspace();
        updateConfirmState();
    });

    auto *actions = new QHBoxLayout;
    actions->setSpacing(12);
    auto *cancelButton = new QPushButton(QStringLiteral("取消"), &dialog);
    confirmButton = new QPushButton(QStringLiteral("确定"), &dialog);
    cancelButton->setMinimumHeight(58);
    confirmButton->setMinimumHeight(58);
    actions->addWidget(cancelButton);
    actions->addWidget(confirmButton);
    layout->addLayout(actions);
    QObject::connect(cancelButton, &QPushButton::clicked, &dialog, &QDialog::reject);
    QObject::connect(confirmButton, &QPushButton::clicked, &dialog, &QDialog::accept);
    updateConfirmState();

    if (dialog.exec() != QDialog::Accepted) return -1;
    bool ok = false;
    const int stationNo = display->text().toInt(&ok);
    return ok && stationNo >= 1 && stationNo <= 255 ? stationNo : -1;
}

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

}

// ====================================================================
// 构造 / 析构
// ====================================================================

KeyManagePage::KeyManagePage(KeyManageController *controller, QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::KeyManagePage)
    , m_controller(controller)
    , m_expertMode(false)
    , m_showHex(true)
    , m_uiFlushTimer(new QTimer(this))
    , m_pendingSystemTicketRefresh(false)
    , m_pendingKeyTaskRefresh(false)
    , m_pendingHttpClientRefresh(false)
    , m_pendingHttpServerRefresh(false)
    , m_pageVisible(false)
{
    Q_ASSERT(m_controller);
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

void KeyManagePage::applyLayoutProfile(const UiLayoutProfile &profile)
{
    const bool narrow = profile.isNarrowWidth;
    const bool shortHeight = profile.isShortHeight;
    QHeaderView *serialHeader = ui->serialLogTable->horizontalHeader();
    for (int column = 0; column < 5; ++column)
        serialHeader->setSectionResizeMode(column, QHeaderView::Fixed);

    if (narrow) {
        serialHeader->setSectionResizeMode(5, QHeaderView::Fixed);
        serialHeader->setSectionResizeMode(6, QHeaderView::Fixed);
        ui->serialLogTable->setColumnWidth(5, 280);
        ui->serialLogTable->setColumnWidth(6, 560);
    } else {
        serialHeader->setSectionResizeMode(5, QHeaderView::Stretch);
        serialHeader->setSectionResizeMode(
                    6, profile.mode == UiLayoutMode::Wide
                       ? QHeaderView::Stretch : QHeaderView::Interactive);
        if (profile.mode != UiLayoutMode::Wide)
            ui->serialLogTable->setColumnWidth(6, 480);
    }
    ui->serialLogTable->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);

#ifdef Q_OS_WIN
    ui->leftSidebarLayout->setContentsMargins(
                narrow ? 10 : 12, 12, narrow ? 10 : 12, 12);
    ui->leftSidebarLayout->setSpacing(shortHeight ? 6 : 8);
    ui->ticketInfoCardLayout->setContentsMargins(
                narrow ? 8 : 10, 10, narrow ? 8 : 10, 10);
    ui->ticketInfoCardLayout->setSpacing(narrow ? 5 : 6);

    const QList<QPushButton *> buttons = findChildren<QPushButton *>();
    for (QPushButton *button : buttons)
        DesktopUiMetrics::applyButton(button, profile.buttonFontPointSize,
                                      profile.buttonMinimumHeight);
    const QList<QPushButton *> tabs = {
        ui->tabKey, ui->tabSerial, ui->tabHttpClient, ui->tabHttpServer
    };
    for (QPushButton *tab : tabs)
        DesktopUiMetrics::applyButton(tab, profile.navigationFontPointSize,
                                      profile.navigationButtonMinimumHeight);

    ui->tabKey->setMinimumWidth(narrow ? 100 : 120);
    ui->tabSerial->setMinimumWidth(narrow ? 110 : 132);
    ui->tabHttpClient->setMinimumWidth(narrow ? 140 : 164);
    ui->tabHttpServer->setMinimumWidth(narrow ? 140 : 164);
    ui->tabBar->setFixedHeight(profile.navigationButtonMinimumHeight);
    ui->serialToolbar->setFixedHeight(profile.buttonMinimumHeight + 8);

    const int ticketHeaderHeight = profile.buttonMinimumHeight + 8;
    ui->systemTicketHeader->setFixedHeight(ticketHeaderHeight);
    ui->keyTicketHeader->setFixedHeight(ticketHeaderHeight);
    ui->systemTicketSectionLayout->setSpacing(6);
    ui->keyTicketSectionLayout->setSpacing(6);
    ui->verticalSpacer->changeSize(0, shortHeight ? 12 : 16,
                                   QSizePolicy::Minimum, QSizePolicy::Fixed);

    const int tableRowHeight = qMax(34, profile.buttonMinimumHeight - 4);
    ui->systemTicketTable->verticalHeader()->setDefaultSectionSize(tableRowHeight);
    ui->keyTicketTable->verticalHeader()->setDefaultSectionSize(tableRowHeight);
    ui->serialLogTable->verticalHeader()->setDefaultSectionSize(tableRowHeight);
#endif
    refreshTicketReturnStatusHeight();
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
    ui->btnQueryTaskCount->setText(QStringLiteral("采码票"));
    ui->lblCommSeatNo->setText(QStringLiteral("当前配置座号: --"));
    ui->btnQueryTaskCount->setToolTip(QStringLiteral("选择业务站号，领取并下发采码票"));
    ui->btnDeleteTicket->setToolTip(QStringLiteral("删除钥匙中已存在的票，请先读取钥匙票列表并选中。"));
    ui->btnClearUnregisteredKeyTask->setToolTip(QStringLiteral("清除钥匙中的未登记任务；钥匙确认删除后将同时通知后台终止任务"));
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
    ui->lblTicketReturnStatus->setWordWrap(true);
    ui->lblTicketReturnStatus->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    ui->lblTicketReturnStatus->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    ui->ticketInfoCard->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    refreshTicketReturnStatusHeight();

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
    connect(ui->btnReadKeyTicketList,   &QPushButton::clicked, this, &KeyManagePage::onReadKeyTicketList);
    connect(ui->btnClearUnregisteredKeyTask,    &QPushButton::clicked, this, &KeyManagePage::onClearUnregisteredKeyTask);
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
    updateAdminButtons();
}

// Connects the injected application controller without owning or starting it.
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
    connect(m_controller, &KeyManageController::taskQueryTimedOut,
            this, &KeyManagePage::onTaskQueryTimedOut);
    connect(m_controller, &KeyManageController::statusMessage,
            this, &KeyManagePage::onControllerStatusMessage);
    connect(m_controller, &KeyManageController::logRowAppended,
            this, &KeyManagePage::onLogRowAppended);
    connect(m_controller, &KeyManageController::logTableRefreshRequested,
            this, &KeyManagePage::onLogTableRefreshRequested);
    connect(m_controller, &KeyManageController::logsCleared,
            this, &KeyManagePage::onLogsCleared);
    connect(m_controller, &KeyManageController::logExportFinished,
            this, &KeyManagePage::onLogExportFinished);
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
    // Capture-ticket claiming is an administrator-only maintenance entry.
    if (!m_isAdmin) return;

    bool stationOk = false;
    int stationNo = ConfigManager::instance()->stationId().trimmed().toInt(&stationOk);
    if (!stationOk || stationNo < 1 || stationNo > 255) stationNo = 1;
    stationNo = requestCaptureStationNo(this, stationNo);
    if (stationNo > 0) m_controller->onCaptureTicketClicked(stationNo);
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
    if (m_controller->isSelectedKeyTaskCapture()) {
        const QMessageBox::StandardButton answer = QMessageBox::warning(
                this,
                QStringLiteral("删除采码票"),
                QStringLiteral("确定删除选中的采码票吗？\n\n"
                               "该操作会删除钥匙任务，且不会回传当前采码结果。"),
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::No);
        if (answer != QMessageBox::Yes) return;
    }
    m_controller->onDeleteClicked();
}

void KeyManagePage::onReturnTicket()
{
    m_controller->onReturnSelectedTicket();
}


void KeyManagePage::onReadKeyTicketList()
{
    m_controller->onQueryTasksClicked();
    updateStatusBar(QStringLiteral("读取钥匙票列表 (Q_TASK)"));
}


void KeyManagePage::onClearUnregisteredKeyTask()
{
    const QMessageBox::StandardButton reply = QMessageBox::warning(
        this,
        QStringLiteral("清除未登记任务"),
        QStringLiteral("确认清除该未登记任务？\n\n程序将先删除钥匙任务，确认删除后通知后台终止对应任务，无法撤销。"),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    if (reply != QMessageBox::Yes) {
        return;
    }
    m_controller->onClearUnregisteredKeyTaskClicked();
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
#ifdef Q_OS_LINUX
    const QString defaultPath = QCoreApplication::applicationDirPath()
            + QStringLiteral("/logs/export");
    QDir().mkpath(defaultPath);
#else
    const QString defaultPath = QDir::homePath();
#endif
    const QString path = QFileDialog::getExistingDirectory(
                this, QStringLiteral("选择日志导出目录"), defaultPath);
    if (path.isEmpty()) {
        return;
    }

    if (!m_controller->exportLogsAsync(path)) {
        updateStatusBar(QStringLiteral("日志正在导出，请稍候"));
        return;
    }

    ui->btnExportLog->setEnabled(false);
    updateStatusBar(QStringLiteral("正在后台导出串口、HTTP和本次运行日志..."));
}

void KeyManagePage::onLogExportFinished(bool success, const QString &result)
{
    ui->btnExportLog->setEnabled(true);
    updateStatusBar(success
            ? QStringLiteral("串口、HTTP和本次运行日志已导出到：%1").arg(result)
            : QStringLiteral("导出失败：%1").arg(result));
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
    updateClearUnregisteredButton();
}

void KeyManagePage::onAckReceived(quint8 ackedCmd)
{
    updateStatusBar(QStringLiteral("收到 ACK: %1")
                        .arg(m_controller->serialCommandText(ackedCmd)));
}

void KeyManagePage::onNakReceived(quint8 origCmd, quint8 errCode)
{
    updateStatusBar(QStringLiteral("收到 NAK: %1 err=0x%2")
                        .arg(m_controller->serialCommandText(origCmd))
                        .arg(errCode, 2, 16, QLatin1Char('0')));
}

void KeyManagePage::onControllerTimeout(const QString &what)
{
    updateStatusBar(what);
}

void KeyManagePage::onTaskQueryTimedOut()
{
    ui->lblTaskInfo->setText(QStringLiteral("浠诲姟鏁? 鏌ヨ瓒呮椂"));
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
    updateClearUnregisteredButton();
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
    updateClearUnregisteredButton();

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
        // 选中的是未登记任务，系统票侧无对应行，主动清空系统票选区避免左侧显示残留
        if (m_controller->isSelectedKeyTaskUnregistered()) {
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

    QTableWidgetItem *cmdItem = new QTableWidgetItem(
                m_controller->serialCommandText(item.cmd));
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
        QTableWidgetItem *stateItem =
                new QTableWidgetItem(TicketStatusPresenter::tableText(ticket));
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

void KeyManagePage::refreshTicketReturnStatusHeight()
{
    QTimer::singleShot(0, this, [this]() {
        const int availableWidth = qMax(80, ui->lblTicketReturnStatus->contentsRect().width());
        const int contentHeight = ui->lblTicketReturnStatus->heightForWidth(availableWidth);
        const int twoLines = ui->lblTicketReturnStatus->fontMetrics().lineSpacing() * 2 + 8;
        ui->lblTicketReturnStatus->setMinimumHeight(qMax(twoLines, contentHeight + 4));
        ui->lblTicketReturnStatus->updateGeometry();
        ui->ticketInfoCard->updateGeometry();
    });
}
void KeyManagePage::updateSelectedSystemTicketCard(const SystemTicketDto &ticket)
{
    if (!ticket.valid) {
        ui->lblTicketNo->setText(QStringLiteral("票号: <font color='#2E7D32'>未选择</font>"));
        ui->lblTicketType->setText(QStringLiteral("类型: --"));
        ui->lblTicketPosition->setText(QStringLiteral("位置: --"));
        ui->lblTicketReturnStatus->setText(QStringLiteral("传票状态: <font color='#C62828'>未选择系统票</font>"));
        refreshTicketReturnStatusHeight();
        ui->lblTicketPosition->setToolTip(QString());
        ui->btnReturnTicket->setEnabled(false);
        ui->lblReturnHint->setText(QStringLiteral("回传按任务独立触发；当前任务完成后可自动或手动回传；接口未配置时不会上传"));
        return;
    }

    const QString stateColor = TicketStatusPresenter::statusColor(ticket);

    ui->lblTicketNo->setText(QStringLiteral("票号: <font color='#2E7D32'>%1</font>").arg(ticket.ticketNo));
    ui->lblTicketType->setText(QStringLiteral("类型: %1  |  步骤: %2")
                               .arg(ticketTypeText(ticket.taskType))
                               .arg(ticket.stepNum));
    const QString positionText = ticketPositionText(ticket.ticketNo, ticket.taskName);
    ui->lblTicketPosition->setText(QStringLiteral("位置: %1").arg(positionText));
    const QString statusText = TicketStatusPresenter::detailText(ticket);
    ui->lblTicketReturnStatus->setText(
        QStringLiteral("传票状态: <font color='%1'>%2</font>").arg(stateColor, statusText));
    refreshTicketReturnStatusHeight();

    quint8 keyTaskStatus = 0xFF;
    findKeyTaskStatusForTicket(ticket.taskId, &keyTaskStatus);
    QString manualReturnBlockedReason;
    const bool canManualReturn =
            m_controller->canStartManualReturn(ticket.taskId, &manualReturnBlockedReason);
    ui->btnReturnTicket->setEnabled(canManualReturn && m_isAdmin);
    ui->lblReturnHint->setText(TicketStatusPresenter::returnHint(
                                   ticket,
                                   keyTaskStatus,
                                   canManualReturn,
                                   manualReturnBlockedReason));
}

void KeyManagePage::populateKeyTicketTable(const QList<KeyTaskDto> &tasks)
{
    const QList<KeyTaskDisplayDto> rows = m_controller->keyTaskDisplayRows(tasks);
    ui->keyTicketTable->setRowCount(0);
    for (int i = 0; i < rows.size(); ++i) {
        const KeyTaskDisplayDto &display = rows.at(i);
        const int row = ui->keyTicketTable->rowCount();
        ui->keyTicketTable->insertRow(row);

        QTableWidgetItem *noItem = new QTableWidgetItem(QString::number(i + 1));
        noItem->setTextAlignment(Qt::AlignCenter);
        ui->keyTicketTable->setItem(row, 0, noItem);

        QTableWidgetItem *taskItem = new QTableWidgetItem(display.taskIdText);
        ui->keyTicketTable->setItem(row, 1, taskItem);
        QTableWidgetItem *typeItem = new QTableWidgetItem(display.typeText);
        typeItem->setTextAlignment(Qt::AlignCenter);
        ui->keyTicketTable->setItem(row, 2, typeItem);
        QTableWidgetItem *sourceItem = new QTableWidgetItem(display.sourceText);
        sourceItem->setTextAlignment(Qt::AlignCenter);
        ui->keyTicketTable->setItem(row, 3, sourceItem);
        QTableWidgetItem *stateItem = new QTableWidgetItem(display.statusText);
        stateItem->setTextAlignment(Qt::AlignCenter);
        ui->keyTicketTable->setItem(row, 4, stateItem);

        for (int col = 1; col < ui->keyTicketTable->columnCount(); ++col) {
            QTableWidgetItem *item = ui->keyTicketTable->item(row, col);
            if (item) item->setToolTip(display.diagnosticText);
        }

        // Only truly unknown key tasks receive the orphan-task warning color.
        if (display.unregistered) {
            const QColor unregisteredBg(0xFF, 0xF0, 0xE0);
            for (int col = 0; col < ui->keyTicketTable->columnCount(); ++col) {
                QTableWidgetItem *it = ui->keyTicketTable->item(row, col);
                if (it) it->setBackground(unregisteredBg);
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

QString KeyManagePage::ticketPositionText(const QString &ticketNo, const QString &taskName)
{
    return extractPositionFromTaskName(ticketNo, taskName);
}

void KeyManagePage::updateAdminButtons()
{
    ui->btnTransferTicket->setEnabled(m_isAdmin);
    ui->btnDeleteTicket->setEnabled(m_isAdmin);
    // The capture entry replaces the former Q_TASK diagnostic button and is
    // not part of the normal operator surface.
    ui->btnQueryTaskCount->setVisible(m_isAdmin);
    ui->btnQueryTaskCount->setEnabled(m_isAdmin);
    // btnReturnTicket 的可用性由 updateSelectedSystemTicketCard 综合判断（需同时满足 canManualReturn && m_isAdmin）
    // 这里只处理非管理员时强制置灰；管理员时交给票据选中逻辑决定
    if (!m_isAdmin) {
        ui->btnReturnTicket->setEnabled(false);
    }
    updateClearUnregisteredButton();
    updateForceCleanButton();
}

void KeyManagePage::updateClearUnregisteredButton()
{
    const bool canClear = m_isAdmin && m_controller->isSelectedKeyTaskUnregistered();
    ui->btnClearUnregisteredKeyTask->setEnabled(canClear);
}

void KeyManagePage::updateForceCleanButton()
{
}

void KeyManagePage::updateStatusBar(const QString &message)
{
    ui->lblFeedback->setText(message);
}
