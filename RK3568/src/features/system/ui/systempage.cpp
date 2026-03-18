/**
 * @file systempage.cpp
 * @brief 系统设置页面实现文件
 *
 * UI优化说明：
 * - 标签切换时颜色高亮
 * - 分区标题加粗加大
 * - 下拉框设置最小宽度防止文字截断
 * - 统一按钮样式
 */

#include "systempage.h"
#include "ui_systempage.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QGroupBox>
#include <QScrollArea>
#include <QFrame>
#include <QTableWidget>
#include <QHeaderView>
#include <QScrollBar>

// 全局样式常量
namespace SystemPageStyle
{
    // 分区标题样式（加粗、加大）
    const QString SECTION_TITLE = "color: #2C3E50; font-size: 14px; font-weight: bold; padding: 8px 0; background: transparent; border: none;";
    // 字段标签样式
    const QString FIELD_LABEL = "color: #5D6B7A; font-size: 13px; background: transparent; border: none; padding: 0;";
    // 输入框样式
    const QString INPUT_STYLE = "background-color: #F5F3EF; border: 1px solid #E8E4DE; border-radius: 8px; padding: 12px 16px; font-size: 14px; min-height: 20px;";
    // 下拉框样式
    const QString COMBO_STYLE = "QComboBox { background-color: #F5F3EF; border: 1px solid #E8E4DE; border-radius: 8px; padding: 10px 16px; font-size: 14px; min-width: 120px; min-height: 20px; }"
                                "QComboBox::drop-down { border: none; width: 30px; }"
                                "QComboBox QAbstractItemView { background-color: #FFFFFF; border: 1px solid #E8E4DE; selection-background-color: #E8F5E9; }";
    // 表格样式
    const QString TABLE_STYLE = "QTableWidget { background-color: #FFFFFF; border: 1px solid #E8E4DE; border-radius: 8px; gridline-color: #E8E4DE; }"
                                "QTableWidget::item { padding: 8px; border-bottom: 1px solid #E8E4DE; }"
                                "QTableWidget::item:selected { background-color: #E8F5E9; color: #2C3E50; }"
                                "QHeaderView::section { background-color: #F5F3EF; color: #2C3E50; font-weight: bold; padding: 10px; border: none; border-bottom: 1px solid #E8E4DE; }";
    // 采集按钮样式
    const QString COLLECT_BTN_STYLE = "QPushButton { background-color: #FFFFFF; color: #2E7D32; font-size: 13px; border: 1px solid #2E7D32; border-radius: 6px; padding: 10px 20px; }"
                                      "QPushButton:hover { background-color: #E8F5E9; }"
                                      "QPushButton:pressed { background-color: #C8E6C9; }";
    // 提示文字样式
    const QString HINT_STYLE = "color: #FF6B35; font-size: 13px;";
}

SystemPage::SystemPage(QWidget *parent) : QWidget(parent),
                                          ui(new Ui::SystemPage),
                                          m_controller(new SystemController(this)),
                                          m_userIdentityWidget(nullptr),
                                          m_keyboardTarget(nullptr),
                                          m_keyboardVisible(false),
                                          m_keyboardHeight(0),
                                          m_lastKeyboardScrollValue(0),
                                          m_keyboardAdjustedScroll(false),
                                          m_userTable(nullptr),
                                          m_collectCardBtn(nullptr),
                                          m_collectFingerprintBtn(nullptr)
{
    ui->setupUi(this);

    // 应用全局样式优化
    applyGlobalStyles();

    // 创建用户身份采集页面
    createUserIdentityContent();

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
 * @brief 应用全局样式优化
 */
void SystemPage::applyGlobalStyles()
{
    // 优化基本设置中的分区标题 (sectionTitle1 是 "串口配置")
    if (ui->sectionTitle1)
    {
        ui->sectionTitle1->setStyleSheet(SystemPageStyle::SECTION_TITLE);
    }

    // 优化字段标签样式（移除边框，透明背景）
    QList<QLabel *> fieldLabels = {
        ui->field1Label, ui->field2Label, ui->field3Label, ui->field4Label,
        ui->field5Label, ui->field6Label, ui->field7Label, ui->field8Label};
    for (QLabel *label : fieldLabels)
    {
        if (label)
        {
            label->setStyleSheet(SystemPageStyle::FIELD_LABEL);
        }
    }

    // 优化下拉框样式（防止文字截断）
    QList<QComboBox *> combos = {
        ui->keySerialCombo, ui->cardSerialCombo,
        ui->baudRateCombo, ui->dataBitsCombo};
    for (QComboBox *combo : combos)
    {
        if (combo)
        {
            combo->setStyleSheet(SystemPageStyle::COMBO_STYLE);
            combo->setMinimumWidth(140);
        }
    }

    // 串口参数收口为工业设备配置风格：
    // 串口仅允许自动识别 + 下拉选择，不允许任意手输；
    // 波特率/数据位作为固定协议参数展示，不再让用户编辑。
    if (ui->keySerialCombo)
    {
        ui->keySerialCombo->setEditable(false);
    }
    if (ui->cardSerialCombo)
    {
        ui->cardSerialCombo->setEditable(false);
    }
    if (ui->field7Label)
    {
        ui->field7Label->setText(QStringLiteral("固定波特率"));
    }
    if (ui->field8Label)
    {
        ui->field8Label->setText(QStringLiteral("固定数据位"));
    }
    if (ui->baudRateCombo)
    {
        ui->baudRateCombo->clear();
        ui->baudRateCombo->addItem(QStringLiteral("115200（固定）"));
        ui->baudRateCombo->setCurrentIndex(0);
        ui->baudRateCombo->setEnabled(false);
    }
    if (ui->dataBitsCombo)
    {
        ui->dataBitsCombo->clear();
        ui->dataBitsCombo->addItem(QStringLiteral("8（固定）"));
        ui->dataBitsCombo->setCurrentIndex(0);
        ui->dataBitsCombo->setEnabled(false);
    }
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
    if (m_userTable)
    {
        connect(m_userTable, &QTableWidget::itemSelectionChanged, this, &SystemPage::onUserTableSelectionChanged);
    }
    if (m_collectFingerprintBtn)
    {
        connect(m_collectFingerprintBtn, &QPushButton::clicked, this, &SystemPage::onCollectCardClicked);
    }
}

/**
 * @brief 更新标签页样式（选中高亮）
 */
void SystemPage::updateTabStyles()
{
    // 选中状态样式
    QString selectedStyle = "background-color: #2E7D32; color: #FFFFFF; font-size: 14px; font-weight: bold; border: none; border-radius: 6px; padding: 8px 20px;";
    // 未选中状态样式
    QString normalStyle = "background-color: transparent; color: #5D6B7A; font-size: 14px; font-weight: normal; border: none; border-radius: 6px; padding: 8px 20px;";

    if (ui->tabBasic->isChecked())
    {
        ui->tabBasic->setStyleSheet(selectedStyle);
        ui->tabAdvanced->setStyleSheet(normalStyle);
    }
    else
    {
        ui->tabBasic->setStyleSheet(normalStyle);
        ui->tabAdvanced->setStyleSheet(selectedStyle);
    }
}

/**
 * @brief 创建用户身份采集页面
 */
void SystemPage::createUserIdentityContent()
{
    // 创建用户身份采集的Widget
    m_userIdentityWidget = new QWidget(this);
    m_userIdentityWidget->setStyleSheet("background: transparent;");

    QVBoxLayout *mainLayout = new QVBoxLayout(m_userIdentityWidget);
    mainLayout->setSpacing(16);
    mainLayout->setContentsMargins(24, 24, 24, 24);

    // === 顶部提示和操作区域 ===
    QHBoxLayout *topRow = new QHBoxLayout();
    topRow->setSpacing(16);

    // 提示文字
    QLabel *hintLabel = new QLabel("选择一个用户，然后刷卡，即可采集卡号", m_userIdentityWidget);
    hintLabel->setStyleSheet(SystemPageStyle::HINT_STYLE);
    topRow->addWidget(hintLabel);

    topRow->addStretch();

    // 采指纹按钮
    m_collectFingerprintBtn = new QPushButton("采指纹", m_userIdentityWidget);
    m_collectFingerprintBtn->setStyleSheet(SystemPageStyle::COLLECT_BTN_STYLE);
    m_collectFingerprintBtn->setMinimumWidth(100);
    m_collectFingerprintBtn->setEnabled(false); // 默认禁用，选中用户后启用
    topRow->addWidget(m_collectFingerprintBtn);

    mainLayout->addLayout(topRow);

    // === 用户表格 ===
    m_userTable = new QTableWidget(m_userIdentityWidget);
    m_userTable->setColumnCount(4);
    m_userTable->setHorizontalHeaderLabels({"账号", "姓名", "卡号", "指纹"});
    m_userTable->setStyleSheet(SystemPageStyle::TABLE_STYLE);
    m_userTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_userTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_userTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_userTable->setAlternatingRowColors(false);
    m_userTable->verticalHeader()->setVisible(false);
    m_userTable->setShowGrid(true);

    // 设置列宽
    m_userTable->horizontalHeader()->setStretchLastSection(true);
    m_userTable->setColumnWidth(0, 120); // 账号
    m_userTable->setColumnWidth(1, 150); // 姓名
    m_userTable->setColumnWidth(2, 150); // 卡号
    // 指纹列自动拉伸

    // 加载用户数据
    loadUserList();

    mainLayout->addWidget(m_userTable, 1); // 表格占据剩余空间

    // 初始隐藏用户身份采集页面
    m_userIdentityWidget->hide();
}

/**
 * @brief 加载用户列表
 */
void SystemPage::loadUserList()
{
    if (!m_userTable)
        return;

    const QList<QStringList> users = m_controller->loadUserList();

    m_userTable->setRowCount(users.size());

    for (int row = 0; row < users.size(); ++row)
    {
        const QStringList &user = users[row];
        for (int col = 0; col < user.size() && col < 4; ++col)
        {
            QTableWidgetItem *item = new QTableWidgetItem(user[col]);
            item->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
            m_userTable->setItem(row, col, item);
        }
        m_userTable->setRowHeight(row, 40);
    }
}

/**
 * @brief 用户表格选择变化
 */
void SystemPage::onUserTableSelectionChanged()
{
    bool hasSelection = m_userTable && m_userTable->selectedItems().count() > 0;
    if (m_collectFingerprintBtn)
    {
        m_collectFingerprintBtn->setEnabled(hasSelection);
    }
}

/**
 * @brief 采集指纹按钮点击
 */
void SystemPage::onCollectCardClicked()
{
    if (!m_userTable)
        return;

    QList<QTableWidgetItem *> selected = m_userTable->selectedItems();
    if (selected.isEmpty())
    {
        QMessageBox::warning(this, "提示", "请先选择一个用户！");
        return;
    }

    int row = selected.first()->row();
    QString username = m_userTable->item(row, 0)->text();
    QString realname = m_userTable->item(row, 1)->text();

    QMessageBox::information(this, "采集指纹",
                             QString("准备为用户 %1(%2) 采集指纹...\n\n请将手指放在指纹仪上。").arg(realname).arg(username));
}

/**
 * @brief 基本设置标签点击
 */
void SystemPage::onTabBasicClicked()
{
    ui->tabBasic->setChecked(true);
    ui->tabAdvanced->setChecked(false);

    // 更新标签样式
    updateTabStyles();

    // 显示基本设置，隐藏用户身份采集
    ui->formScrollArea->show();
    if (m_userIdentityWidget)
    {
        // 从布局中移除用户身份采集页面
        ui->sysFormPanelLayout->removeWidget(m_userIdentityWidget);
        m_userIdentityWidget->hide();
    }

    // 更新标题
    ui->formTitle->setText("系统参数设置");
}

/**
 * @brief 用户身份采集标签点击
 */
void SystemPage::onTabAdvancedClicked()
{
    ui->tabBasic->setChecked(false);
    ui->tabAdvanced->setChecked(true);

    // 更新标签样式
    updateTabStyles();

    // 隐藏基本设置，显示用户身份采集
    ui->formScrollArea->hide();
    if (m_userIdentityWidget)
    {
        // 将用户身份采集页面添加到布局
        ui->sysFormPanelLayout->addWidget(m_userIdentityWidget);
        m_userIdentityWidget->show();
    }

    // 更新标题
    ui->formTitle->setText("用户身份采集");
}

/**
 * @brief 刷新按钮点击
 */
void SystemPage::onRefreshClicked()
{
    if (ui->tabBasic->isChecked())
    {
        loadSettings();
        QMessageBox::information(this, "刷新", "配置已从存储中重新加载！");
    }
    else
    {
        // 用户身份采集页面刷新用户列表
        loadUserList();
        QMessageBox::information(this, "刷新", "用户列表已刷新！");
    }
}

/**
 * @brief 保存按钮点击
 */
void SystemPage::onSaveClicked()
{
    if (ui->tabBasic->isChecked())
    {
        saveSettings();
        QMessageBox::information(this, "保存成功", "所有配置已保存！\n\n配置文件位置：系统注册表\n(HKEY_CURRENT_USER\\Software\\RK3568)");
    }
    else
    {
        // 用户身份采集页面暂无保存操作
        QMessageBox::information(this, "提示", "用户身份信息已同步到服务器。");
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

        combo->setEditable(false);
        combo->clear();
        combo->addItem("");                 // 空选项 = 未配置
        for (const QString &portName : availPorts) {
            combo->addItem(portName);
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
            combo->setCurrentIndex(1);
        }
    }
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
    dto.baudRate = ui->baudRateCombo->currentText().toInt();
    dto.dataBits = ui->dataBitsCombo->currentText().toInt();

    m_controller->saveSettings(dto);
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
