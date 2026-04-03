/**
 * @file systempage.cpp
 * @brief 系统设置页面实现文件
 *
 * UI优化说明：
 * - 基础页静态骨架继续下沉到 .ui，C++ 只做数据回填与保存
 * - 高级页保留 mixed 结构，表格和采集动作继续由 C++ 编排
 * - 标签切换时颜色高亮
 * - 分区标题加粗加大
 * - 下拉框设置最小宽度防止文字截断
 * - 统一按钮样式
 */

#include "systempage.h"
#include "ui_systempage.h"
#include "features/auth/infra/device/CardSerialSource.h"
#include "features/auth/infra/password/authservice.h"
#include <QFrame>
#include <QLineEdit>
#include <QComboBox>
#include <QListView>
#include <QLabel>
#include <QStyle>

namespace {
constexpr int kFixedBaudRate = 115200;
constexpr int kFixedDataBits = 8;
}
#include <QHeaderView>
#include <QScrollArea>
#include <QTableWidget>
#include <QScrollBar>
#include <QMessageBox>

SystemPage::SystemPage(QWidget *parent) : QWidget(parent),
                                          ui(new Ui::SystemPage),
                                          m_controller(new SystemController(this)),
                                          m_cardSource(new CardSerialSource(this)),
                                          m_keyboardTarget(nullptr),
                                          m_keyboardVisible(false),
                                          m_keyboardHeight(0),
                                          m_lastKeyboardScrollValue(0),
                                          m_keyboardAdjustedScroll(false),
                                          m_userListLoaded(false),
                                          m_canManageIdentityMedia(false),
                                          m_canManageSerialPorts(false),
                                          m_identityReadOnlyPromptShown(false),
                                          m_clearingCard(false),
                                          m_collectingCard(false)
{
    ui->setupUi(this);

    if (ui->basicSectionTitle)
    {
        ui->basicSectionTitle->hide();
    }
    if (ui->sectionTitle1)
    {
        ui->sectionTitle1->hide();
    }
    if (ui->advancedSectionTitle)
    {
        ui->advancedSectionTitle->hide();
    }
    if (ui->divider1)
    {
        ui->divider1->hide();
    }
    if (ui->basicProfileCard)
    {
        ui->basicProfileCard->setFrameShape(QFrame::NoFrame);
        ui->basicProfileCard->setStyleSheet(QStringLiteral("QFrame#basicProfileCard { background: transparent; border: none; }"));
    }
    if (ui->basicSerialCard)
    {
        ui->basicSerialCard->setFrameShape(QFrame::NoFrame);
        ui->basicSerialCard->setStyleSheet(QStringLiteral("QFrame#basicSerialCard { background: transparent; border: none; }"));
    }
    if (ui->formScrollArea)
    {
        ui->formScrollArea->setFrameShape(QFrame::NoFrame);
    }
    if (ui->advancedIntroCard)
    {
        ui->advancedIntroCard->hide();
    }

    auto configureComboPopup = [](QComboBox *combo) {
        if (!combo)
        {
            return;
        }

        combo->setMaxVisibleItems(6);
        combo->setCursor(Qt::PointingHandCursor);
        combo->setToolTip(QStringLiteral("点击选择串口"));
        combo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
        combo->setPlaceholderText(QStringLiteral("请选择串口"));
        combo->setMinimumHeight(56);
        QListView *listView = new QListView(combo);
        listView->setUniformItemSizes(true);
        listView->setSpacing(0);
        listView->setMouseTracking(true);
        listView->setStyleSheet(QStringLiteral(
            "QListView {"
            "  background: #FFFFFF;"
            "  color: #1F2937;"
            "  border: 1px solid #DAD7CF;"
            "  border-radius: 12px;"
            "  padding: 4px;"
            "  font-size: 17px;"
            "  outline: 0;"
            "}"
            "QListView::item {"
            "  min-height: 44px;"
            "  padding: 0 14px;"
            "  color: #1F2937;"
            "  background: #FFFFFF;"
            "}"
            "QListView::item:hover {"
            "  background: #FFFFFF;"
            "  color: #1F2937;"
            "}"
            "QListView::item:selected {"
            "  background: #FFFFFF;"
            "  color: #1F2937;"
            "}"
            "QListView::item:selected:active {"
            "  background: #FFFFFF;"
            "  color: #1F2937;"
            "}"));
        combo->setView(listView);
    };
    configureComboPopup(ui->keySerialCombo);
    configureComboPopup(ui->cardSerialCombo);

    if (ui->saveBtn)
    {
        ui->saveBtn->setText(QStringLiteral("保存配置"));
        ui->saveBtn->setCursor(Qt::PointingHandCursor);
        ui->saveBtn->setMinimumHeight(46);
        ui->saveBtn->setMinimumWidth(156);
        ui->saveBtn->setStyleSheet(QStringLiteral(
            "QPushButton#saveBtn {"
            "  background-color: #2E7D32;"
            "  color: #FFFFFF;"
            "  font-size: 16px;"
            "  font-weight: 600;"
            "  border: none;"
            "  border-radius: 8px;"
            "  padding: 10px 20px;"
            "}"
            "QPushButton#saveBtn:hover {"
            "  background-color: #256B29;"
            "}"
            "QPushButton#saveBtn:pressed {"
            "  background-color: #145A14;"
            "}"
            "QPushButton#saveBtn:disabled {"
            "  background-color: #BFD7C1;"
            "  color: #F7FBF7;"
            "}"));
        ui->saveBtn->raise();
    }

    if (ui->refreshBtn)
    {
        ui->refreshBtn->setCursor(Qt::PointingHandCursor);
        ui->refreshBtn->setMinimumHeight(46);
    }
    if (ui->deleteCardBtn)
    {
        ui->deleteCardBtn->setCursor(Qt::PointingHandCursor);
        ui->deleteCardBtn->setMinimumHeight(46);
    }
    if (ui->deleteFingerprintBtn)
    {
        ui->deleteFingerprintBtn->setCursor(Qt::ArrowCursor);
        ui->deleteFingerprintBtn->setMinimumHeight(46);
        ui->deleteFingerprintBtn->setEnabled(false);
    }
    if (ui->collectFingerprintPlaceholderBtn)
    {
        ui->collectFingerprintPlaceholderBtn->setCursor(Qt::ArrowCursor);
        ui->collectFingerprintPlaceholderBtn->setMinimumHeight(46);
        ui->collectFingerprintPlaceholderBtn->setEnabled(false);
    }

    configureUserTable();
    updateIdentityPermissionState();
    updateSerialPermissionState();
    if (ui->serialFixedHintLabel)
    {
        ui->serialFixedHintLabel->setWordWrap(true);
        ui->serialFixedHintLabel->setText(
            QStringLiteral("钥匙串口和读卡串口的配置只会保存到配置文件。表单里显示的新串口只是已保存配置，当前运行中的串口不会立即切换，需在业务空闲后重启程序加载新配置。"));
    }

    // 连接信号槽
    setupConnections();

    // 加载设置
    loadSettings();

    // 初始化标签状态
    updateTabStyles();
}

SystemPage::~SystemPage()
{
    delete ui;
}

/**
 * @brief 设置信号槽连接
 */
void SystemPage::setupConnections()
{
    // 标签页切换
    connect(ui->tabBasic, &QPushButton::clicked, this, &SystemPage::onTabBasicClicked);
    connect(ui->tabAdvanced, &QPushButton::clicked, this, &SystemPage::onTabAdvancedClicked);

    // 刷新和保存按钮
    connect(ui->refreshBtn, &QPushButton::clicked, this, &SystemPage::onRefreshClicked);
    connect(ui->saveBtn, &QPushButton::clicked, this, &SystemPage::onSaveClicked);
    // 用户身份采集相关
    connect(ui->userTable, &QTableWidget::itemSelectionChanged, this, &SystemPage::onUserTableSelectionChanged);
    connect(ui->deleteCardBtn, &QPushButton::clicked, this, &SystemPage::onDeleteCardClicked);
    connect(ui->collectFingerprintBtn, &QPushButton::clicked, this, &SystemPage::onCollectCardClicked);
    connect(m_controller, &SystemController::userListChanged, this, &SystemPage::onUserListChanged);
    connect(m_controller, &SystemController::userListLoadFailed, this, &SystemPage::onUserListLoadFailed);
    connect(m_controller, &SystemController::userListLoadingChanged, this, &SystemPage::onUserListLoadingChanged);
    connect(m_controller, &SystemController::cardNoUpdated, this, &SystemPage::onCardNoUpdated);
    connect(m_controller, &SystemController::cardNoUpdateFailed, this, &SystemPage::onCardNoUpdateFailed);
    connect(m_controller, &SystemController::cardNoUpdateStateChanged, this, &SystemPage::onCardNoUpdateStateChanged);
    connect(m_cardSource, &CardSerialSource::cardCaptured, this, &SystemPage::onCardCaptured);
    connect(m_cardSource, &CardSerialSource::sourceError, this, &SystemPage::onCardSourceError);
    connect(m_cardSource, &CardSerialSource::readerStatusChanged,
            this, &SystemPage::cardReaderStatusChanged);
    connect(AuthService::instance(), &AuthService::loginSuccess, this, [this](const QJsonObject &) {
        resetIdentityViewForSessionChange();
        updateIdentityPermissionState();
        updateSerialPermissionState();
    });
    connect(AuthService::instance(), &AuthService::loggedOut, this, [this]() {
        resetIdentityViewForSessionChange();
        updateIdentityPermissionState();
        updateSerialPermissionState();
    });
}

/**
 * @brief 配置高级页用户表的静态行为
 */
void SystemPage::configureUserTable()
{
    if (!ui || !ui->userTable)
    {
        return;
    }

    ui->userTable->verticalHeader()->setVisible(false);
    ui->userTable->horizontalHeader()->setStretchLastSection(true);
    ui->userTable->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    ui->userTable->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    ui->userTable->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    ui->userTable->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    ui->userTable->setWordWrap(false);
    ui->userTable->setTextElideMode(Qt::ElideRight);
    ui->userTable->setColumnWidth(0, 120);
    ui->userTable->setColumnWidth(1, 150);
    ui->userTable->setColumnWidth(2, 150);

    if (ui->collectFingerprintBtn)
    {
        ui->collectFingerprintBtn->setEnabled(false);
        ui->collectFingerprintBtn->setText(QStringLiteral("采集卡号"));
    }
}

/**
 * @brief 更新标签页样式（选中高亮）
 */
void SystemPage::updateTabStyles()
{
    if (!ui || !ui->contentStack)
    {
        return;
    }

    const bool basicActive = ui->tabBasic->isChecked();
    ui->contentStack->setCurrentIndex(basicActive ? 0 : 1);
    ui->formTitle->setText(basicActive ? QStringLiteral("系统参数设置") : QStringLiteral("用户身份采集"));
    if (ui->refreshBtn)
    {
        ui->refreshBtn->setVisible(!basicActive);
        ui->refreshBtn->setEnabled(!basicActive);
        ui->refreshBtn->setText(QStringLiteral("刷新用户"));
    }
    if (ui->saveBtn)
    {
        ui->saveBtn->setVisible(basicActive);
        ui->saveBtn->setEnabled(basicActive);
        ui->saveBtn->setText(QStringLiteral("保存配置"));
        ui->saveBtn->setToolTip(basicActive
                                    ? QStringLiteral("保存当前配置。钥匙串口和读卡串口会在下次重启程序时加载新配置")
                                    : QString());
        if (basicActive)
        {
            ui->saveBtn->show();
            ui->saveBtn->raise();
            ui->saveBtn->adjustSize();
        }
    }

    if (ui->formHeaderLayout)
    {
        ui->formHeaderLayout->invalidate();
        ui->formHeaderLayout->activate();
    }
    if (ui->formHeader)
    {
        ui->formHeader->updateGeometry();
        ui->formHeader->update();
    }
}

/**
 * @brief 加载用户列表
 */
void SystemPage::loadUserList()
{
    if (!ui)
        return;

    if (m_collectingCard)
    {
        resetCardCollectionState();
    }

    if (ui->advancedActionHintLabel)
    {
        ui->advancedActionHintLabel->setText(QStringLiteral("正在加载用户列表..."));
    }

    m_controller->requestUserList();
}

/**
 * @brief 用户表格选择变化
 */
void SystemPage::onUserTableSelectionChanged()
{
    updateCollectButtonState();
}

/**
 * @brief 采集卡号按钮点击
 */
void SystemPage::onCollectCardClicked()
{
    if (!ui || !ui->userTable)
        return;

    if (!m_canManageIdentityMedia)
    {
        QMessageBox::warning(this, QStringLiteral("权限不足"),
                             QStringLiteral("当前账号仅可查看用户身份信息，卡号维护需管理员账号操作。"));
        return;
    }

    const int row = selectedUserRow();
    if (row < 0 || row >= m_users.size())
    {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请先选择一个用户！"));
        return;
    }

    const SystemIdentityUserDto &user = m_users[row];
    if (!user.canUpdateCardNo())
    {
        QMessageBox::warning(this, QStringLiteral("提示"),
                             QStringLiteral("当前用户列表未返回 userId，暂无法采集卡号。\n请补充用户详情接口或调整返回字段。"));
        return;
    }

    m_collectingCard = true;
    m_clearingCard = false;
    m_collectingUserId = user.userId;
    m_collectingDisplayName = displayNameForUser(user);

    if (ui->advancedActionHintLabel)
    {
        ui->advancedActionHintLabel->setText(
            QStringLiteral("正在为 %1 采集卡号，请将卡贴近读卡器。").arg(m_collectingDisplayName));
    }

    updateCollectButtonState();
    m_cardSource->start();
}

void SystemPage::onDeleteCardClicked()
{
    if (!ui || !ui->userTable)
    {
        return;
    }

    if (!m_canManageIdentityMedia)
    {
        QMessageBox::warning(this, QStringLiteral("权限不足"),
                             QStringLiteral("当前账号仅可查看用户身份信息，卡号维护需管理员账号操作。"));
        return;
    }

    const int row = selectedUserRow();
    if (row < 0 || row >= m_users.size())
    {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请先选择一个用户！"));
        return;
    }

    const SystemIdentityUserDto &user = m_users[row];
    if (!user.canUpdateCardNo())
    {
        QMessageBox::warning(this, QStringLiteral("提示"),
                             QStringLiteral("当前用户列表未返回 userId，暂无法删除卡号。"));
        return;
    }
    if (user.cardNo.trimmed().isEmpty())
    {
        QMessageBox::information(this, QStringLiteral("删除卡号"),
                                 QStringLiteral("%1 当前没有已绑定的卡号。").arg(displayNameForUser(user)));
        return;
    }

    const QString displayName = displayNameForUser(user);
    const auto reply = QMessageBox::question(
        this,
        QStringLiteral("删除卡号"),
        QStringLiteral("确认删除 %1 的卡号吗？").arg(displayName),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    if (reply != QMessageBox::Yes)
    {
        return;
    }

    if (ui->advancedActionHintLabel)
    {
        ui->advancedActionHintLabel->setText(
            QStringLiteral("正在删除 %1 的卡号...").arg(displayName));
    }

    m_collectingDisplayName = displayName;
    m_clearingCard = true;
    m_controller->clearUserCardNo(user.userId);
}

/**
 * @brief 基本设置标签点击
 */
void SystemPage::onTabBasicClicked()
{
    if (m_collectingCard)
    {
        resetCardCollectionState();
    }

    ui->tabBasic->setChecked(true);
    ui->tabAdvanced->setChecked(false);
    updateTabStyles();
}

/**
 * @brief 用户身份采集标签点击
 */
void SystemPage::onTabAdvancedClicked()
{
    ui->tabBasic->setChecked(false);
    ui->tabAdvanced->setChecked(true);
    updateTabStyles();
    updateIdentityPermissionState();

    if (!m_canManageIdentityMedia && !m_identityReadOnlyPromptShown)
    {
        m_identityReadOnlyPromptShown = true;
        QMessageBox::information(
            this,
            QStringLiteral("只读模式"),
            QStringLiteral("当前账号仅可查看用户身份信息。\n\n卡号和指纹维护需管理员账号操作。"));
    }

    if (!m_controller->isUserListLoading() && !m_controller->isCardUpdateInProgress())
    {
        loadUserList();
    }
    else
    {
        updateCollectButtonState();
    }
}

/**
 * @brief 刷新按钮点击
 */
void SystemPage::onRefreshClicked()
{
    // 用户身份采集页面刷新用户列表
    loadUserList();
}

/**
 * @brief 保存按钮点击
 */
void SystemPage::onSaveClicked()
{
    if (ui->tabBasic->isChecked())
    {
        saveSettings();
        loadSettings();
        QMessageBox::information(this, QStringLiteral("保存配置"),
                                 QStringLiteral("配置已保存。\n\n"
                                                "首页地址、接口地址、租户编码会用于后续请求；\n"
                                                "站号会同时影响 DEL 协议地址以及 INIT/RFID 后端取数范围；\n"
                                                "本页表单会立即显示新串口配置，但当前运行中的串口连接保持不变；\n"
                                                "请在当前任务完成后重启程序，再统一加载新的钥匙串口和读卡串口配置。"));
    }
    else
    {
        // 高级页当前没有对应的保存/同步实现，避免误导用户以为已提交成功。
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("高级页当前暂无保存动作。"));
    }
}

/**
 * @brief 加载设置 - 从 ConfigManager 读取
 */
void SystemPage::loadSettings()
{
    const SystemSettingsDto dto = m_controller->loadSettings();
    ui->homeUrlEdit->setText(dto.homeUrl);
    ui->apiUrlEdit->setText(dto.apiUrl);
    ui->stationIdEdit->setText(dto.stationId);
    ui->tenantCodeEdit->setText(dto.tenantCode);

    // 刷新可用串口列表。
    // 当前串口项按“自动识别 + 下拉选择”处理，不再允许自由手输。
    const QStringList availPorts = m_controller->availableSerialPorts();
    const QString savedPorts[2] = { dto.keySerialPort, dto.cardSerialPort };
    QComboBox *combos[2] = { ui->keySerialCombo, ui->cardSerialCombo };
    for (int i = 0; i < 2; ++i) {
        QComboBox *combo = combos[i];
        const QString &saved = savedPorts[i];

        combo->clear();
        for (const QString &portName : availPorts) {
            if (!portName.trimmed().isEmpty())
            {
                combo->addItem(portName);
            }
        }
        // 已保存端口若不在枚举列表中（设备未插 / 预配置），仍追加显示
        if (!saved.isEmpty() && combo->findText(saved) < 0) {
            combo->addItem(saved);
        }
        if (!saved.isEmpty())
        {
            combo->setCurrentText(saved);
        }
        else if (combo->count() > 1)
        {
            // 第一阶段以“自动识别 / 自动回填”优先：
            // 如果已有可枚举端口，则默认选第一个有效项，减少手工配置。
            combo->setCurrentIndex(0);
        }
        else if (combo->count() == 1)
        {
            combo->setCurrentIndex(0);
        }
        else
        {
            combo->setCurrentIndex(-1);
        }

        if (combo->count() == 0)
        {
            combo->setToolTip(QStringLiteral("未检测到可用串口"));
        }
        else
        {
            combo->setToolTip(QStringLiteral("点击选择串口"));
        }
    }

    updateSerialPermissionState();

}

/**
 * @brief 保存设置 - 写入 ConfigManager
 */
void SystemPage::saveSettings()
{
    SystemSettingsDto dto;
    dto.homeUrl = ui->homeUrlEdit->text();
    dto.apiUrl = ui->apiUrlEdit->text();
    dto.stationId = ui->stationIdEdit->text();
    dto.tenantCode = ui->tenantCodeEdit->text();
    dto.keySerialPort = ui->keySerialCombo->currentText();
    dto.cardSerialPort = ui->cardSerialCombo->currentText();
    dto.baudRate = kFixedBaudRate;
    dto.dataBits = kFixedDataBits;

    if (!m_canManageSerialPorts)
    {
        const SystemSettingsDto current = m_controller->loadSettings();
        dto.keySerialPort = current.keySerialPort;
        dto.cardSerialPort = current.cardSerialPort;
    }

    m_controller->saveSettings(dto);
}

void SystemPage::onUserListChanged()
{
    m_users = m_controller->users();
    repopulateUserTable();
    m_userListLoaded = true;

    if (ui->advancedActionHintLabel)
    {
        ui->advancedActionHintLabel->setText(
            m_canManageIdentityMedia
                ? QStringLiteral("请选择一个用户，然后点击“采集卡号”并刷卡。")
                : QStringLiteral("当前账号仅可查看用户身份信息，卡号维护需管理员账号操作。"));
    }
}

void SystemPage::onUserListLoadFailed(const QString &message)
{
    const QString errorMessage = message.trimmed().isEmpty()
        ? QStringLiteral("加载用户列表失败")
        : message.trimmed();

    if (ui->advancedActionHintLabel)
    {
        ui->advancedActionHintLabel->setText(errorMessage);
    }

    QMessageBox::warning(this, QStringLiteral("刷新用户"), errorMessage);
    updateCollectButtonState();
}

void SystemPage::onUserListLoadingChanged(bool inProgress)
{
    Q_UNUSED(inProgress);
    updateCollectButtonState();
}

void SystemPage::onCardCaptured(const CardCredential &credential)
{
    if (!m_collectingCard)
    {
        return;
    }

    const QString normalizedCardNo = credential.cardNo.trimmed().toUpper();
    if (normalizedCardNo.isEmpty())
    {
        return;
    }

    for (const SystemIdentityUserDto &user : m_users)
    {
        if (user.cardNo.trimmed().compare(normalizedCardNo, Qt::CaseInsensitive) != 0)
        {
            continue;
        }

        m_cardSource->stop();
        const QString targetDisplayName = m_collectingDisplayName;
        resetCardCollectionState();

        if (user.userId == m_collectingUserId)
        {
            const QString message = QStringLiteral("%1 已绑定当前卡号，无需重复采集。").arg(targetDisplayName);
            if (ui->advancedActionHintLabel)
            {
                ui->advancedActionHintLabel->setText(message);
            }
            QMessageBox::information(this, QStringLiteral("采集卡号"), message);
            return;
        }

        const QString conflictMessage = QStringLiteral("该卡已绑定给 %1，请先删除原绑定后再采集。")
                                            .arg(displayNameForUser(user));
        if (ui->advancedActionHintLabel)
        {
            ui->advancedActionHintLabel->setText(conflictMessage);
        }
        QMessageBox::warning(this, QStringLiteral("采集卡号"), conflictMessage);
        return;
    }

    m_cardSource->stop();

    if (ui->advancedActionHintLabel)
    {
        ui->advancedActionHintLabel->setText(
            QStringLiteral("已读取卡号 %1，正在保存到 %2。")
                .arg(normalizedCardNo, m_collectingDisplayName));
    }

    m_controller->updateUserCardNo(m_collectingUserId, normalizedCardNo);
}

void SystemPage::onCardSourceError(const QString &message)
{
    if (!m_collectingCard || !ui->advancedActionHintLabel)
    {
        return;
    }

    ui->advancedActionHintLabel->setText(
        QStringLiteral("读卡器尚未就绪：%1")
            .arg(message.trimmed().isEmpty()
                     ? QStringLiteral("请检查读卡串口配置")
                     : message.trimmed()));
}

void SystemPage::onCardNoUpdated(const QString &userId, const QString &cardNo)
{
    const bool clearingCard = m_clearingCard;
    int updatedRow = -1;
    for (int row = 0; row < m_users.size(); ++row)
    {
        if (m_users[row].userId == userId)
        {
            m_users[row].cardNo = cardNo;
            updatedRow = row;
            break;
        }
    }

    repopulateUserTable();
    if (updatedRow >= 0 && ui->userTable)
    {
        ui->userTable->selectRow(updatedRow);
    }

    const QString displayName = m_collectingDisplayName;
    resetCardCollectionState();
    if (ui->advancedActionHintLabel)
    {
        ui->advancedActionHintLabel->setText(clearingCard || cardNo.trimmed().isEmpty()
            ? QStringLiteral("已删除 %1 的卡号").arg(displayName)
            : QStringLiteral("已为 %1 保存卡号：%2").arg(displayName, cardNo.toUpper()));
    }

    QMessageBox::information(this,
                             (clearingCard || cardNo.trimmed().isEmpty()) ? QStringLiteral("删除卡号") : QStringLiteral("采集卡号"),
                             (clearingCard || cardNo.trimmed().isEmpty())
                                 ? QStringLiteral("已删除 %1 的卡号").arg(displayName)
                                 : QStringLiteral("已为 %1 保存卡号：%2")
                                       .arg(displayName, cardNo.toUpper()));
}

void SystemPage::onCardNoUpdateFailed(const QString &message)
{
    const bool clearingCard = m_clearingCard;
    const QString errorMessage = message.trimmed().isEmpty()
        ? (clearingCard ? QStringLiteral("删除卡号失败") : QStringLiteral("保存卡号失败"))
        : message.trimmed();

    resetCardCollectionState();
    if (ui->advancedActionHintLabel)
    {
        ui->advancedActionHintLabel->setText(errorMessage);
    }

    QMessageBox::warning(this,
                         clearingCard ? QStringLiteral("删除卡号") : QStringLiteral("采集卡号"),
                         errorMessage);
}

void SystemPage::onCardNoUpdateStateChanged(bool inProgress)
{
    Q_UNUSED(inProgress);
    updateCollectButtonState();
}

void SystemPage::repopulateUserTable()
{
    if (!ui || !ui->userTable)
    {
        return;
    }

    const int selectedRowBefore = selectedUserRow();
    ui->userTable->clearSelection();
    ui->userTable->clearContents();
    ui->userTable->setRowCount(m_users.size());

    for (int row = 0; row < m_users.size(); ++row)
    {
        const SystemIdentityUserDto &user = m_users[row];
        const QString columnTexts[4] = {
            user.account,
            user.realName,
            user.cardNo,
            user.fingerprint
        };

        for (int column = 0; column < 4; ++column)
        {
            QString displayText = columnTexts[column];
            if (column == 3 && displayText.length() > 20)
            {
                displayText = displayText.left(20) + QStringLiteral("...");
            }

            QTableWidgetItem *item = new QTableWidgetItem(displayText);
            item->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
            item->setToolTip(columnTexts[column]);
            if (column == 0)
            {
                item->setData(Qt::UserRole, user.userId);
            }
            ui->userTable->setItem(row, column, item);
        }
        ui->userTable->setRowHeight(row, 40);
    }

    if (selectedRowBefore >= 0 && selectedRowBefore < m_users.size())
    {
        ui->userTable->selectRow(selectedRowBefore);
    }

    updateCollectButtonState();
}

void SystemPage::resetCardCollectionState()
{
    m_cardSource->stop();
    m_clearingCard = false;
    m_collectingCard = false;
    m_collectingUserId.clear();
    m_collectingDisplayName.clear();
    updateCollectButtonState();
}

void SystemPage::resetIdentityViewForSessionChange()
{
    if (m_collectingCard)
    {
        resetCardCollectionState();
    }

    m_identityReadOnlyPromptShown = false;
    m_userListLoaded = false;
    m_users.clear();
    repopulateUserTable();
}

void SystemPage::updateCollectButtonState()
{
    if (!ui || !ui->collectFingerprintBtn || !ui->deleteCardBtn)
    {
        return;
    }

    const int selectedRow = selectedUserRow();
    const bool hasSelection = selectedRow >= 0 && selectedRow < m_users.size();
    const bool hasCard = hasSelection && !m_users[selectedRow].cardNo.trimmed().isEmpty();
    const bool enableCollect = m_canManageIdentityMedia
        && hasSelection
        && !m_collectingCard
        && !m_controller->isUserListLoading()
        && !m_controller->isCardUpdateInProgress();
    const bool enableDelete = m_canManageIdentityMedia
        && hasSelection
        && hasCard
        && !m_collectingCard
        && !m_controller->isUserListLoading()
        && !m_controller->isCardUpdateInProgress();

    ui->collectFingerprintBtn->setEnabled(enableCollect);
    ui->deleteCardBtn->setEnabled(enableDelete);
    ui->collectFingerprintBtn->setText(m_collectingCard
                                           ? QStringLiteral("等待刷卡...")
                                           : QStringLiteral("采集卡号"));
    ui->deleteCardBtn->setText(QStringLiteral("删除卡号"));

    if (!m_canManageIdentityMedia)
    {
        ui->collectFingerprintBtn->setToolTip(QStringLiteral("仅管理员可维护卡号"));
        ui->deleteCardBtn->setToolTip(QStringLiteral("仅管理员可维护卡号"));
    }
    else if (!hasSelection)
    {
        ui->collectFingerprintBtn->setToolTip(QStringLiteral("请先选择一个用户"));
        ui->deleteCardBtn->setToolTip(QStringLiteral("请先选择一个用户"));
    }
    else
    {
        ui->collectFingerprintBtn->setToolTip(QStringLiteral("点击进入采卡状态，然后刷卡"));
        ui->deleteCardBtn->setToolTip(hasCard
                                          ? QStringLiteral("删除当前用户已绑定的卡号")
                                          : QStringLiteral("当前用户没有已绑定的卡号"));
    }
}

int SystemPage::selectedUserRow() const
{
    if (!ui || !ui->userTable)
    {
        return -1;
    }

    const QList<QTableWidgetItem *> selected = ui->userTable->selectedItems();
    if (selected.isEmpty())
    {
        return -1;
    }

    return selected.first()->row();
}

QString SystemPage::displayNameForUser(const SystemIdentityUserDto &user) const
{
    const QString realName = user.realName.trimmed();
    const QString account = user.account.trimmed();
    if (!realName.isEmpty() && !account.isEmpty())
    {
        return QStringLiteral("%1(%2)").arg(realName, account);
    }
    if (!realName.isEmpty())
    {
        return realName;
    }
    return account;
}

void SystemPage::updateIdentityPermissionState()
{
    m_canManageIdentityMedia = currentUserCanManageIdentityMedia();

    if (ui && ui->advancedHintLabel)
    {
        ui->advancedHintLabel->setText(m_canManageIdentityMedia
            ? QStringLiteral("管理员可为用户采集或删除卡号，指纹维护将在下一阶段接入。")
            : QStringLiteral("当前账号仅可查看用户身份信息，卡号和指纹维护需管理员账号操作。"));
    }

    if (ui && ui->deleteFingerprintBtn)
    {
        ui->deleteFingerprintBtn->setToolTip(m_canManageIdentityMedia
            ? QStringLiteral("指纹维护即将接入")
            : QStringLiteral("仅管理员可维护指纹，且当前功能暂未接入"));
    }
    if (ui && ui->collectFingerprintPlaceholderBtn)
    {
        ui->collectFingerprintPlaceholderBtn->setToolTip(m_canManageIdentityMedia
            ? QStringLiteral("指纹维护即将接入")
            : QStringLiteral("仅管理员可维护指纹，且当前功能暂未接入"));
    }

    updateCollectButtonState();
}

void SystemPage::updateSerialPermissionState()
{
    m_canManageSerialPorts = currentUserCanManageSerialPorts();
    const QString baseHint = QStringLiteral("钥匙串口和读卡串口的配置只会保存到配置文件。表单里显示的新串口只是已保存配置，当前运行中的串口不会立即切换，需在业务空闲后重启程序加载新配置。");

    auto applySerialPermission = [this](QComboBox *combo, const QString &editableTip) {
        if (!combo)
        {
            return;
        }

        combo->setEnabled(m_canManageSerialPorts);
        combo->setCursor(m_canManageSerialPorts ? Qt::PointingHandCursor : Qt::ArrowCursor);
        combo->setToolTip(m_canManageSerialPorts
                              ? editableTip
                              : QStringLiteral("仅管理员可修改串口配置"));
    };

    applySerialPermission(ui ? ui->keySerialCombo : nullptr, QStringLiteral("点击选择钥匙串口"));
    applySerialPermission(ui ? ui->cardSerialCombo : nullptr, QStringLiteral("点击选择读卡串口"));

    if (ui && ui->serialFixedHintLabel)
    {
        ui->serialFixedHintLabel->setText(m_canManageSerialPorts
                                              ? baseHint
                                              : baseHint + QStringLiteral("\n当前账号仅可查看串口配置，只有管理员可修改钥匙串口和读卡串口。"));
    }
}

bool SystemPage::currentUserCanManageIdentityMedia() const
{
    const QJsonObject userInfo = AuthService::instance()->getUserInfo();
    const QString account = userInfo.value(QStringLiteral("account")).toString().trimmed();
    const QString roleName = userInfo.value(QStringLiteral("role_name")).toString().trimmed();

    if (account.compare(QStringLiteral("admin"), Qt::CaseInsensitive) == 0)
    {
        return true;
    }

    if (roleName.compare(QStringLiteral("administrator"), Qt::CaseInsensitive) == 0)
    {
        return true;
    }

    if (roleName.contains(QStringLiteral("admin"), Qt::CaseInsensitive)
        || roleName.contains(QStringLiteral("管理员")))
    {
        return true;
    }

    return false;
}

bool SystemPage::currentUserCanManageSerialPorts() const
{
    return currentUserCanManageIdentityMedia();
}

void SystemPage::onKeyboardVisibilityChanged(bool visible, int height)
{
    m_keyboardVisible = visible;
    m_keyboardHeight = height;

    if (!ui->formScrollArea || !ui->tabBasic->isChecked())
    {
        return;
    }

    if (!visible)
    {
        m_keyboardAdjustedScroll = false;
        return;
    }

    ensureKeyboardTargetVisible();
}

void SystemPage::ensureKeyboardTargetVisible()
{
    if (!ui->formScrollArea || !ui->tabBasic->isChecked() || !m_keyboardTarget)
    {
        return;
    }

    QWidget *scrollContent = ui->formScrollArea->widget();
    if (!scrollContent || !scrollContent->isAncestorOf(m_keyboardTarget))
    {
        return;
    }

    const QPoint posInScroll = m_keyboardTarget->mapTo(scrollContent, QPoint(0, 0));
    QScrollBar *scrollBar = ui->formScrollArea->verticalScrollBar();
    const int currentValue = scrollBar->value();
    const int fieldTop = posInScroll.y();
    const int fieldBottom = fieldTop + m_keyboardTarget->height();
    const int viewportHeight = ui->formScrollArea->viewport()->height();
    const int topPadding = 24;
    const int bottomPadding = 24;
    const int safeTop = currentValue + topPadding;
    const int safeBottom = currentValue + viewportHeight - m_keyboardHeight - bottomPadding;

    int nextValue = currentValue;
    if (fieldBottom > safeBottom)
    {
        nextValue += (fieldBottom - safeBottom);
    }
    else if (fieldTop < safeTop)
    {
        nextValue -= (safeTop - fieldTop);
    }

    nextValue = qMax(scrollBar->minimum(), qMin(nextValue, scrollBar->maximum()));
    if (nextValue != currentValue)
    {
        if (!m_keyboardAdjustedScroll)
        {
            m_lastKeyboardScrollValue = currentValue;
            m_keyboardAdjustedScroll = true;
        }
        scrollBar->setValue(nextValue);
    }
}

void SystemPage::onKeyboardTargetChanged(QWidget *target)
{
    QLineEdit *lineEdit = qobject_cast<QLineEdit *>(target);
    if (lineEdit && this->isAncestorOf(lineEdit))
    {
        m_keyboardTarget = lineEdit;
    }
    else
    {
        m_keyboardTarget.clear();
    }

    if (m_keyboardVisible && m_keyboardHeight > 0)
    {
        ensureKeyboardTargetVisible();
    }
}
