/**
 * @file mainwindow.cpp
 * @brief 主窗口实现文件 - 数字化停送电安全管理系统
 * @author RK3568项目组
 * @date 2026-01-29
 */

#include "app/mainwindow.h"
#include "ui_mainwindow.h"
#include "features/auth/infra/device/CardSerialSource.h"
#include "features/auth/ui/loginpage.h"
#include "features/keyboard/application/KeyboardController.h"
#include "features/keyboard/application/KeyboardPagePolicy.h"
#include "features/keyboard/application/KeyboardSizingPolicy.h"
#include "features/keyboard/ui/KeyboardContainer.h"
#include "features/workbench/ui/workbenchpage.h"
#include "features/key/ui/keymanagepage.h"
#include "features/system/ui/systempage.h"
#include "features/log/ui/logpage.h"
#include "core/ConfigManager.h"
#include "platform/logging/LogService.h"
#include <QGuiApplication>
#include <QPushButton>
#include <QScreen>
#include <QCloseEvent>
#include <QShowEvent>
#include <QStyle>

namespace
{
void setNeutralStatusDot(QLabel *dot)
{
    if (!dot)
    {
        return;
    }
    dot->setStyleSheet(QStringLiteral("background-color: #B0B0B0; border-radius: 4px;"));
}

void setStatusDot(QLabel *dot, const QString &color)
{
    if (!dot)
    {
        return;
    }
    dot->setStyleSheet(QStringLiteral("background-color: %1; border-radius: 4px;").arg(color));
}
} // namespace

/**
 * @brief 构造函数
 * @param parent 父窗口指针
 *
 * 初始化主窗口，设置UI、信号槽连接和定时器
 */
MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent),
                                          ui(new Ui::MainWindow),
                                          m_loginPage(nullptr),
                                          m_workbenchPage(nullptr),
                                          m_keyManagePage(nullptr),
                                          m_systemPage(nullptr),
                                          m_logPage(nullptr),
                                          m_keyboardContainer(nullptr),
                                          m_keyboardController(nullptr),
                                          m_mainController(new MainWindowController(nullptr, this)),
                                          m_timeTimer(nullptr)
{
    // 初始化核心服务，确保主窗口壳体和页面替换流程进入稳定默认态。
    ConfigManager::instance();
    LogService::instance();

    LOG_INFO("MainWindow", "应用程序启动");

    ui->setupUi(this); // ← 加载 .ui 文件，创建所有控件

    // 创建登录页面并添加到QStackedWidget，保持 .ui 壳体与真实页面 mixed 替换。
    setupPages();
    setupCustomKeyboard();
    applyInitialWindowGeometry();
    
    // 初始化各模块
    setupConnections(); // 连接信号槽
    setupTimer();       // 设置定时器

    // 初始显示登录页面，避免启动后直接落到业务页。
    switchToPage(PAGE_LOGIN);

    // 更新界面显示
    updateNavButtonState();
    updateUserDisplay();
    updateStatusBar();

    LOG_INFO("MainWindow", "主窗口初始化完成");
}

/**
 * @brief 析构函数
 * 释放UI资源和定时器
 */
MainWindow::~MainWindow()
{
    if (m_timeTimer)
    {
        m_timeTimer->stop();
        delete m_timeTimer;
    }
    delete ui;
}

/**
 * @brief 设置页面
 * 创建各个页面实例并添加到QStackedWidget
 */
void MainWindow::setupPages()
{
    // 统一页面替换逻辑：用真实页面替换 .ui 中的占位页，并释放占位页对象。
    auto replacePlaceholderPage = [this](QWidget *placeholder, QWidget *realPage, const QString &pageName) -> bool
    {
        if (!placeholder || !realPage)
        {
            LOG_ERROR("MainWindow", QString("页面替换失败：%1 参数为空").arg(pageName));
            return false;
        }

        int pageIndex = ui->contentStackedWidget->indexOf(placeholder);
        if (pageIndex < 0)
        {
            LOG_ERROR("MainWindow", QString("页面替换失败：未找到占位页 %1").arg(pageName));
            return false;
        }

        ui->contentStackedWidget->removeWidget(placeholder);
        ui->contentStackedWidget->insertWidget(pageIndex, realPage);
        placeholder->deleteLater();
        return true;
    };

    // ========== 创建登录页面 ==========
    m_loginPage = new LoginPage(this);
    if (!replacePlaceholderPage(ui->loginPage, m_loginPage, "loginPage"))
    {
        delete m_loginPage;
        m_loginPage = nullptr;
    }

    // 防御式判空：占位页替换失败时，避免对 nullptr 建立信号连接。
    if (m_loginPage) {
        connect(m_loginPage, &LoginPage::loginSuccess,
                this, &MainWindow::onUserLoggedIn);
        connect(m_loginPage, &LoginPage::cardReaderStatusChanged,
                this, &MainWindow::onCardReaderStatusChanged);
    }

    // ========== 创建工作台页面 ==========
    m_workbenchPage = new WorkbenchPage(this);
    if (!replacePlaceholderPage(ui->workbenchPage, m_workbenchPage, "workbenchPage"))
    {
        delete m_workbenchPage;
        m_workbenchPage = nullptr;
    }

    // ========== 创建钥匙管理页面 ==========
    m_keyManagePage = new KeyManagePage(this);
    if (!replacePlaceholderPage(ui->keyManagePage, m_keyManagePage, "keyManagePage"))
    {
        delete m_keyManagePage;
        m_keyManagePage = nullptr;
    }

    // ========== 创建系统设置页面 ==========
    m_systemPage = new SystemPage(this);
    if (!replacePlaceholderPage(ui->systemPage, m_systemPage, "systemPage"))
    {
        delete m_systemPage;
        m_systemPage = nullptr;
    }
    else
    {
        connect(m_systemPage, &SystemPage::cardReaderStatusChanged,
                this, &MainWindow::onCardReaderStatusChanged);
    }

    // ========== 创建服务日志页面 ==========
    m_logPage = new LogPage(this);
    if (!replacePlaceholderPage(ui->logPage, m_logPage, "logPage"))
    {
        delete m_logPage;
        m_logPage = nullptr;
    }
}

/**
 * @brief 设置信号槽连接
 * 连接导航按钮和其他控件的信号
 */
void MainWindow::setupConnections()
{
    // 导航按钮连接
    connect(ui->btnWorkbench, &QPushButton::clicked,
            this, &MainWindow::onBtnWorkbenchClicked);
    connect(ui->btnKeys, &QPushButton::clicked,
            this, &MainWindow::onBtnKeysClicked);
    connect(ui->btnSystem, &QPushButton::clicked,
            this, &MainWindow::onBtnSystemClicked);
    connect(ui->btnService, &QPushButton::clicked,
            this, &MainWindow::onBtnServiceClicked);
    connect(ui->btnKeyboard, &QPushButton::clicked,
            this, &MainWindow::onBtnKeyboardClicked);

    // 退出按钮连接
    connect(ui->logoutBtn, &QPushButton::clicked,
            this, &MainWindow::onLogoutBtnClicked);

    // 认证状态统一由控制器提供，避免主窗口直接访问具体认证/上下文实现。
    connect(m_mainController, &MainWindowController::userStateChanged, this, [this](bool loggedIn, const QString &, const QString &) {
        updateUserDisplay();

        if (!loggedIn && ui && ui->contentStackedWidget)
        {
            const int currentIndex = ui->contentStackedWidget->currentIndex();
            if (currentIndex >= 0 && currentIndex < ui->contentStackedWidget->count()
                    && pageRequiresLogin(static_cast<PageIndex>(currentIndex)))
            {
                switchToPage(PAGE_LOGIN);
                return;
            }
        }

        updateNavButtonState();
    });
    connect(ui->contentStackedWidget, &QStackedWidget::currentChanged, this, [this](int) {
        resetKeyboardContext();
        updateNavButtonState();
        updateKeyboardButtonState();
    });

    ui->btnKeyboard->setAutoExclusive(false);
}

/**
 * @brief 设置定时器
 * 创建1秒间隔的定时器用于更新时间显示
 */
void MainWindow::setupTimer()
{
    m_timeTimer = new QTimer(this);
    connect(m_timeTimer, &QTimer::timeout, this, &MainWindow::updateTime);
    m_timeTimer->start(1000); // 每秒更新一次

    // 立即更新一次时间
    updateTime();
}

void MainWindow::setupCustomKeyboard()
{
    m_keyboardContainer = new KeyboardContainer(ui->contentStackedWidget);
    m_keyboardContainer->hide();
    m_keyboardController = new KeyboardController(this);
    m_keyboardController->attachContainer(m_keyboardContainer);
    if (m_loginPage)
    {
        m_keyboardController->registerInputScope(m_loginPage);
    }
    if (m_systemPage)
    {
        m_keyboardController->registerInputScope(m_systemPage);
    }
    if (m_loginPage)
    {
        connect(m_keyboardController, &KeyboardController::visibilityChanged,
                m_loginPage, &LoginPage::onKeyboardVisibilityChanged);
    }
    if (m_systemPage)
    {
        connect(m_keyboardController, &KeyboardController::visibilityChanged,
                m_systemPage, &SystemPage::onKeyboardVisibilityChanged);
        connect(m_keyboardController, &KeyboardController::targetChanged,
                m_systemPage, &SystemPage::onKeyboardTargetChanged);
    }
}

void MainWindow::applyInitialWindowGeometry()
{
    constexpr int kDeviceWindowWidth = 1024;
    constexpr int kDeviceWindowHeight = 768;

    // WHY:
    // 板端页面在真实 .ui + 运行时替换后，实际 sizeHint 可能高于历史固定 768。
    // 若仍强行按 1024x768 启动，会把底部 footerWidget 挤出可见区域。
    // 这里保持宽度基线不变，但允许高度按页面 sizeHint 上探，再受屏幕可用区裁剪。
    if (ui && ui->centralWidget && ui->centralWidget->layout()) {
        ui->centralWidget->layout()->activate();
    }

    const int hintedHeight = qMax(kDeviceWindowHeight, sizeHint().height());
    const QSize desiredSize(kDeviceWindowWidth, hintedHeight);

    if (QScreen *screen = QGuiApplication::primaryScreen())
    {
        const QRect available = screen->availableGeometry();
        const int width = qMin(desiredSize.width(), available.width());
        const int height = qMin(desiredSize.height(), available.height());
        setMinimumSize(qMin(960, available.width()),
                       qMin(640, available.height()));
        resize(width, height);
    }
    else
    {
        setMinimumSize(960, 640);
        resize(desiredSize);
    }
}

void MainWindow::fitWindowToAvailableGeometry()
{
    QScreen *screen = QGuiApplication::screenAt(frameGeometry().center());
    if (!screen)
    {
        screen = QGuiApplication::primaryScreen();
    }
    if (!screen)
    {
        return;
    }

    const QRect available = screen->availableGeometry();
    const QRect currentFrame = frameGeometry();
    const QRect currentClient = geometry();
    const int nonClientWidth = qMax(0, currentFrame.width() - currentClient.width());
    const int nonClientHeight = qMax(0, currentFrame.height() - currentClient.height());
    const int maxClientWidth = qMax(320, available.width() - nonClientWidth);
    const int maxClientHeight = qMax(240, available.height() - nonClientHeight);

    const QSize targetSize(qMin(width(), maxClientWidth),
                           qMin(height(), maxClientHeight));

    if (minimumWidth() > maxClientWidth || minimumHeight() > maxClientHeight)
    {
        setMinimumSize(qMin(minimumWidth(), maxClientWidth),
                       qMin(minimumHeight(), maxClientHeight));
    }

    if (size() != targetSize)
    {
        resize(targetSize);
    }

    const QRect finalFrame = frameGeometry();
    const int x = available.x() + qMax(0, (available.width() - finalFrame.width()) / 2);
    const int y = available.y() + qMax(0, (available.height() - finalFrame.height()) / 2);
    move(x, y);
}

void MainWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);

    // WHY:
    // XFCE / X11 下窗口装饰和可用区约束往往在 show 之后才完全稳定；
    // 仅在构造阶段修正一次，容易出现 frame 仍为 768 高、footer 被截掉。
    const QList<int> fitPassDelaysMs = {0, 80, 180, 320};
    for (const int delayMs : fitPassDelaysMs)
    {
        QTimer::singleShot(delayMs, this, [this]() {
            if (isVisible())
            {
                fitWindowToAvailableGeometry();
            }
        });
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    LOG_INFO("MainWindow",
             QString("主窗口收到关闭事件: accepted=%1 visible=%2 spontaneous=%3")
                 .arg(event && event->isAccepted() ? "true" : "false")
                 .arg(isVisible() ? "true" : "false")
                 .arg(event && event->spontaneous() ? "true" : "false"));
    QMainWindow::closeEvent(event);
}

QWidget *MainWindow::currentPageWidget() const
{
    return ui->contentStackedWidget->currentWidget();
}

void MainWindow::resetKeyboardContext()
{
    if (m_keyboardController)
    {
        m_keyboardController->hide();
        m_keyboardController->clearCurrentTarget();
    }

    if (m_keyboardContainer)
    {
        m_keyboardContainer->clearActionHandlers();
    }

    if (ui && ui->btnKeyboard)
    {
        ui->btnKeyboard->setChecked(false);
    }
}

/**
 * @brief 更新时间显示
 * 在底部状态栏显示当前时间
 */
void MainWindow::updateTime()
{
    QString currentTime = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    ui->timeLabel->setText(QString("当前时间: %1").arg(currentTime));
}

/**
 * @brief 切换页面
 * @param page 目标页面索引
 */
void MainWindow::switchToPage(PageIndex page)
{
    const int targetIndex = static_cast<int>(page);
    const int pageCount = ui->contentStackedWidget->count();
    if (targetIndex < 0 || targetIndex >= pageCount)
    {
        LOG_ERROR("MainWindow", QString("页面切换失败：非法索引 %1，pageCount=%2").arg(targetIndex).arg(pageCount));
        return;
    }

    const int currentIndex = ui->contentStackedWidget->currentIndex();
    if (targetIndex != currentIndex)
    {
        resetKeyboardContext();
    }

    ui->contentStackedWidget->setCurrentIndex(targetIndex);
    updateNavButtonState();
    updateKeyboardButtonState();
}

/**
 * @brief 更新导航按钮状态
 * 根据当前页面高亮对应的导航按钮
 */
void MainWindow::updateNavButtonState()
{
    int currentIndex = ui->contentStackedWidget->currentIndex();

    // 设置按钮的checked状态
    ui->btnWorkbench->setChecked(currentIndex == PAGE_WORKBENCH);
    ui->btnKeys->setChecked(currentIndex == PAGE_KEYS);
    ui->btnSystem->setChecked(currentIndex == PAGE_SYSTEM);
    ui->btnService->setChecked(currentIndex == PAGE_LOG);
    updateNavAccessState();
}

void MainWindow::updateNavAccessState()
{
    applyNavAccessState(ui->btnWorkbench, PAGE_WORKBENCH);
    applyNavAccessState(ui->btnKeys, PAGE_KEYS);
    applyNavAccessState(ui->btnSystem, PAGE_SYSTEM);
    applyNavAccessState(ui->btnService, PAGE_LOG);
}

void MainWindow::applyNavAccessState(QPushButton *button, PageIndex page)
{
    if (!button)
    {
        return;
    }

    const bool restricted = pageRequiresLogin(page)
            && !(m_mainController && m_mainController->isLoggedIn());
    button->setProperty("restricted", restricted);

    if (restricted)
    {
        button->setToolTip(QStringLiteral("当前未登录，需登录后访问该页面"));
    }
    else
    {
        button->setToolTip(QString());
    }

    button->style()->unpolish(button);
    button->style()->polish(button);
    button->update();
}

void MainWindow::updateKeyboardButtonState()
{
    if (!ui || !ui->btnKeyboard || !ui->contentStackedWidget)
    {
        return;
    }

    QString pageKey;
    switch (ui->contentStackedWidget->currentIndex())
    {
    case PAGE_LOGIN:
        pageKey = QStringLiteral("login");
        break;
    case PAGE_SYSTEM:
        pageKey = QStringLiteral("system");
        break;
    case PAGE_WORKBENCH:
        pageKey = QStringLiteral("workbench");
        break;
    case PAGE_KEYS:
        pageKey = QStringLiteral("keys");
        break;
    case PAGE_LOG:
        pageKey = QStringLiteral("log");
        break;
    default:
        pageKey = QStringLiteral("unknown");
        break;
    }

    const bool supported = KeyboardPagePolicy::supportsCustomKeyboard(pageKey);
    ui->btnKeyboard->setEnabled(supported);
    if (!supported)
    {
        if (pageKey == QLatin1String("keys"))
        {
            ui->btnKeyboard->setToolTip(QStringLiteral("当前页面暂无可输入字段，暂不需要虚拟键盘"));
        }
        else if (pageKey == QLatin1String("log"))
        {
            ui->btnKeyboard->setToolTip(QStringLiteral("服务日志页为查看页，当前不支持虚拟键盘"));
        }
        else
        {
            ui->btnKeyboard->setToolTip(QStringLiteral("当前页面暂不支持虚拟键盘"));
        }
    }
    else if (pageKey == QLatin1String("workbench"))
    {
        ui->btnKeyboard->setToolTip(QStringLiteral("请先点击工作台中的输入框，再打开虚拟键盘"));
    }
    else
    {
        ui->btnKeyboard->setToolTip(QStringLiteral("点击打开虚拟键盘"));
    }
}

/**
 * @brief 更新用户显示信息
 * 在顶部和底部状态栏显示当前登录用户信息
 */
void MainWindow::updateUserDisplay()
{
    const bool loggedIn = m_mainController && m_mainController->isLoggedIn();

    if (loggedIn)
    {
        const QString username = m_mainController->currentUsername();
        const QString role = m_mainController->currentRole();
        ui->userNameLabel->setText(username);
        ui->userInfoLabel->setText(QStringLiteral("当前用户: %1").arg(username));
        ui->roleLabel->setText(QStringLiteral("角色: %1").arg(role.isEmpty() ? QStringLiteral("已登录") : role));
        ui->logoutBtn->setEnabled(true);
        ui->logoutBtn->setText("退出");
    }
    else
    {
        ui->userNameLabel->setText("未登录");
        ui->userInfoLabel->setText(QStringLiteral("当前用户: 游客"));
        ui->roleLabel->setText(QStringLiteral("角色: 未登录"));
        ui->logoutBtn->setEnabled(true);
        ui->logoutBtn->setText("登录");
    }
}

/**
 * @brief 更新状态栏
 * 更新底部状态栏的设备状态显示，默认保持壳体态。
 */
void MainWindow::updateStatusBar()
{
    applyReaderStatus(static_cast<int>(CardSerialSource::Idle), QStringLiteral("空闲"));
    applyFingerprintStatusDisconnected();
}

void MainWindow::applyReaderStatus(int status, const QString &message)
{
    const QString detail = message.trimmed();
    const CardSerialSource::ReaderStatus readerStatus =
        static_cast<CardSerialSource::ReaderStatus>(status);

    switch (readerStatus)
    {
    case CardSerialSource::Ready:
        setStatusDot(ui->cardStatusDot, QStringLiteral("#2E7D32"));
        ui->cardStatusLabel->setText(QStringLiteral("读卡器状态：%1").arg(detail.isEmpty() ? QStringLiteral("就绪") : detail));
        break;
    case CardSerialSource::Detecting:
        setStatusDot(ui->cardStatusDot, QStringLiteral("#F9A825"));
        ui->cardStatusLabel->setText(QStringLiteral("读卡器状态：%1").arg(detail.isEmpty() ? QStringLiteral("检测中") : detail));
        break;
    case CardSerialSource::Error:
        setStatusDot(ui->cardStatusDot, QStringLiteral("#D84315"));
        ui->cardStatusLabel->setText(QStringLiteral("读卡器状态：%1").arg(detail.isEmpty() ? QStringLiteral("异常") : detail));
        break;
    case CardSerialSource::Unconfigured:
        setNeutralStatusDot(ui->cardStatusDot);
        ui->cardStatusLabel->setText(QStringLiteral("读卡器状态：%1").arg(detail.isEmpty() ? QStringLiteral("未配置串口") : detail));
        break;
    case CardSerialSource::Idle:
    default:
        setNeutralStatusDot(ui->cardStatusDot);
        ui->cardStatusLabel->setText(QStringLiteral("读卡器状态：%1").arg(detail.isEmpty() ? QStringLiteral("空闲") : detail));
        break;
    }
}

void MainWindow::applyFingerprintStatusDisconnected()
{
    setNeutralStatusDot(ui->fingerStatusDot);
    ui->fingerStatusLabel->setText(QStringLiteral("指纹仪状态：未接入"));
}

void MainWindow::onCardReaderStatusChanged(int status, const QString &message)
{
    applyReaderStatus(status, message);
}

/**
 * @brief 检查是否需要登录
 * @param page 目标页面
 * @return true 可以访问，false 需要先登录
 */
bool MainWindow::checkLoginRequired(PageIndex page)
{
    const bool loggedIn = m_mainController && m_mainController->isLoggedIn();

    // 工作台和钥匙管理需要登录
    if (pageRequiresLogin(page) && !loggedIn)
    {
        QMessageBox::information(this, "需要登录",
                                 "请先登录才能访问此功能！");
        switchToPage(PAGE_LOGIN);
        return false;
    }
    return true;
}

bool MainWindow::pageRequiresLogin(PageIndex page) const
{
    return page == PAGE_WORKBENCH || page == PAGE_KEYS;
}

// ==================== 导航按钮槽函数实现 ====================

/**
 * @brief 工作台按钮点击槽函数
 */
void MainWindow::onBtnWorkbenchClicked()
{
    if (checkLoginRequired(PAGE_WORKBENCH))
    {
        switchToPage(PAGE_WORKBENCH);
    }
}

/**
 * @brief 钥匙管理按钮点击槽函数
 */
void MainWindow::onBtnKeysClicked()
{
    if (checkLoginRequired(PAGE_KEYS))
    {
        switchToPage(PAGE_KEYS);
    }
}

/**
 * @brief 系统设置按钮点击槽函数
 * 系统设置不需要登录即可访问
 */
void MainWindow::onBtnSystemClicked()
{
    switchToPage(PAGE_SYSTEM);
}

/**
 * @brief 服务日志按钮点击槽函数
 * 服务日志不需要登录即可访问
 */
void MainWindow::onBtnServiceClicked()
{
    switchToPage(PAGE_LOG);
}

/**
 * @brief 虚拟键盘按钮点击槽函数
 * 虚拟键盘不需要登录即可访问
 */
void MainWindow::onBtnKeyboardClicked()
{
    QString pageKey;
    switch (ui->contentStackedWidget->currentIndex())
    {
    case PAGE_LOGIN:
        pageKey = QStringLiteral("login");
        break;
    case PAGE_SYSTEM:
        pageKey = QStringLiteral("system");
        break;
    case PAGE_WORKBENCH:
        pageKey = QStringLiteral("workbench");
        break;
    case PAGE_KEYS:
        pageKey = QStringLiteral("keys");
        break;
    case PAGE_LOG:
        pageKey = QStringLiteral("log");
        break;
    default:
        pageKey = QStringLiteral("unknown");
        break;
    }

    qInfo() << "[MainWindow] keyboard button clicked, pageKey=" << pageKey;

    if (!KeyboardPagePolicy::supportsCustomKeyboard(pageKey))
    {
        QMessageBox::information(this, "键盘",
                                 "当前页面暂不支持虚拟键盘。");
        ui->btnKeyboard->setChecked(false);
        return;
    }

    QWidget *page = currentPageWidget();
    const bool visible = m_keyboardController && m_keyboardController->isVisible();
    qInfo() << "[MainWindow] keyboard visible before toggle =" << visible;
    if (visible)
    {
        if (m_keyboardController)
        {
            m_keyboardController->hide();
        }
        ui->btnKeyboard->setChecked(false);
        return;
    }

    if (m_keyboardContainer)
    {
        m_keyboardContainer->setKeyboardHeight(KeyboardSizingPolicy::keyboardHeightForPage(pageKey));
    }

    if (pageKey == QLatin1String("workbench"))
    {
        if (!m_workbenchPage || !m_keyboardContainer)
        {
            ui->btnKeyboard->setChecked(false);
            return;
        }

        m_workbenchPage->requestKeyboardActivation(m_keyboardContainer, [this](bool ready) {
            if (ui->contentStackedWidget->currentIndex() != PAGE_WORKBENCH)
            {
                return;
            }

            if (!ready)
            {
                QMessageBox::information(this, "键盘",
                                         "请先点击工作台中的可编辑输入框，再打开键盘。");
                ui->btnKeyboard->setChecked(false);
                return;
            }

            m_keyboardContainer->showStandalone(keyboard::KeyboardMode::Normal, true);
            ui->btnKeyboard->setChecked(true);
        });
        return;
    }

    if (m_keyboardController && m_keyboardController->hasTargetOnPage(page))
    {
        m_keyboardController->toggleOnPage(page);
        ui->btnKeyboard->setChecked(true);
        return;
    }

    if (!m_keyboardContainer)
    {
        ui->btnKeyboard->setChecked(false);
        return;
    }

    m_keyboardContainer->clearActionHandlers();
    m_keyboardContainer->showStandalone(keyboard::KeyboardMode::Normal,
                                        pageKey == QLatin1String("system"));
    ui->btnKeyboard->setChecked(true);
}

/**
 * @brief 退出/登录按钮点击槽函数
 * 根据当前登录状态执行登录或登出操作
 */
void MainWindow::onLogoutBtnClicked()
{
    if (m_mainController && m_mainController->isLoggedIn())
    {
        const QString currentUserName = m_mainController->currentUsername();

        // 已登录 -> 确认登出
        QMessageBox::StandardButton reply = QMessageBox::question(
            this, "退出确认",
            QString("当前用户: %1\n确定要退出登录吗？").arg(currentUserName),
            QMessageBox::Yes | QMessageBox::No);

        if (reply == QMessageBox::Yes)
        {
            onUserLoggedOut();
        }
    }
    else
    {
        // 未登录 -> 跳转到登录页面
        switchToPage(PAGE_LOGIN);
    }
}

// ==================== 登录状态管理槽函数 ====================

/**
 * @brief 用户登录成功槽函数
 * @param username 用户名
 * @param role 用户角色
 *
 * 供登录页面调用，通知主窗口登录成功
 */
void MainWindow::onUserLoggedIn(const QString &username, const QString &role)
{
    // 登录态已由 AuthService 写入，主窗口仅做 UI 与导航刷新。
    Q_UNUSED(role);
    updateUserDisplay();

    // 登录成功后跳转到工作台
    switchToPage(PAGE_WORKBENCH);

    QMessageBox::information(this, "登录成功",
                             QString("欢迎 %1，您已成功登录！").arg(username));
}

/**
 * @brief 用户登出槽函数
 * 清除登录状态，返回登录页面
 */
void MainWindow::onUserLoggedOut()
{
    // 统一由控制器触发登出，避免主窗口直连服务实现。
    if (m_mainController) {
        m_mainController->logout();
    }

    updateUserDisplay();
    switchToPage(PAGE_LOGIN);
}
