/**
 * @file mainwindow.cpp
 * @brief 主窗口实现文件 - 数字化停送电安全管理系统
 * @author RK3568项目组
 * @date 2026-01-29
 */

#include "app/mainwindow.h"
#include "app/InputMethodCoordinator.h"
#include "ui_mainwindow.h"
#include "features/auth/ui/loginpage.h"
#include "features/workbench/ui/workbenchpage.h"
#include "features/key/ui/keymanagepage.h"
#include "features/system/ui/systempage.h"
#include "features/log/ui/logpage.h"
#include "core/ConfigManager.h"
#include "platform/logging/LogService.h"
#include <QGuiApplication>
#include <QComboBox>
#include <QScreen>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QTextEdit>

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
                                          m_inputMethodHost(nullptr),
                                          m_mainController(new MainWindowController(nullptr, this)),
                                          m_timeTimer(nullptr)
{
    // 初始化核心服务，服务单例（单例初始化）
    ConfigManager::instance();
    LogService::instance();

    LOG_INFO("MainWindow", "应用程序启动");

    ui->setupUi(this); // ← 加载 .ui 文件，创建所有控件

#ifdef Q_OS_LINUX
    // Linux 板端/VNC 环境的可用画布可能小于 .ui 中固定的 1280x720。
    // 这里覆盖主窗口硬编码最小尺寸，按当前屏幕可用区域自适应启动。
    if (QScreen *screen = QGuiApplication::primaryScreen())
    {
        const QRect available = screen->availableGeometry();
        const int width = qMin(1280, qMax(available.width(), 800));
        const int height = qMin(720, qMax(available.height(), 480));
        setMinimumSize(800, 480);
        resize(width, height);
    }
    else
    {
        setMinimumSize(800, 480);
        resize(1024, 600);
    }
#endif

    // 创建登录页面并添加到QStackedWidget
    setupPages();
    setupInputMethodHost();
    
    // 初始化各模块
    setupConnections(); // 连接信号槽
    setupTimer();       // 设置定时器

    // 初始显示登录页面
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
    connect(m_mainController, &MainWindowController::userStateChanged, this, [this](bool, const QString &, const QString &) {
        updateUserDisplay();
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

void MainWindow::setupInputMethodHost()
{
    m_inputMethodHost = InputMethodCoordinator::createEmbeddedHost(ui->contentStackedWidget);
    if (!m_inputMethodHost)
    {
        return;
    }
}

QWidget *MainWindow::currentPageWidget() const
{
    return ui->contentStackedWidget->currentWidget();
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

    ui->contentStackedWidget->setCurrentIndex(targetIndex);
    updateNavButtonState();
    applyInputMethodPolicy(page);
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
        ui->userNameLabel->setText(m_mainController->currentUsername());
        ui->roleLabel->setText(m_mainController->currentRole());
        ui->logoutBtn->setEnabled(true);
        ui->logoutBtn->setText("退出");
    }
    else
    {
        ui->userNameLabel->setText("未登录");
        ui->roleLabel->setText("游客");
        ui->logoutBtn->setEnabled(true);
        ui->logoutBtn->setText("登录");
    }
}

/**
 * @brief 更新状态栏
 * 更新底部状态栏的设备状态显示
 */
void MainWindow::updateStatusBar()
{
    // 设备状态更新（后续可连接实际硬件检测逻辑）
    // 目前显示为正常状态
    ui->cardStatusLabel->setText("读卡器正常");
    ui->fingerStatusLabel->setText("指纹仪正常");
}

void MainWindow::applyInputMethodPolicy(PageIndex page)
{
    QString pageKey;
    switch (page)
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
    case PAGE_KEYBOARD:
        pageKey = QStringLiteral("keyboard");
        break;
    }

    // 第一阶段采用显式按钮控制：
    // 切页时统一收起键盘，避免页面切换造成残留和布局误判。
    InputMethodCoordinator::hideInputMethod();
    ui->btnKeyboard->setChecked(false);

    if (!InputMethodCoordinator::pageAllowsVirtualKeyboard(pageKey))
    {
        return;
    }
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
    if ((page == PAGE_WORKBENCH || page == PAGE_KEYS) && !loggedIn)
    {
        QMessageBox::information(this, "需要登录",
                                 "请先登录才能访问此功能！");
        switchToPage(PAGE_LOGIN);
        return false;
    }
    return true;
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
    auto isSupportedEditor = [](QWidget *widget) -> bool
    {
        return qobject_cast<QLineEdit *>(widget)
               || qobject_cast<QTextEdit *>(widget)
               || qobject_cast<QPlainTextEdit *>(widget);
    };

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

    qInfo() << "[MainWindow] virtual keyboard button clicked, pageKey=" << pageKey;

    if (!InputMethodCoordinator::pageAllowsVirtualKeyboard(pageKey))
    {
        QMessageBox::information(this, "虚拟键盘",
                                 "当前页面暂不支持软键盘，请在登录页或系统设置页中使用。");
        ui->btnKeyboard->setChecked(false);
        return;
    }

    QWidget *page = currentPageWidget();
    QWidget *focused = page ? page->focusWidget() : nullptr;

    const bool visible = InputMethodCoordinator::isInputMethodVisible();
    qInfo() << "[MainWindow] virtual keyboard visible before toggle =" << visible;
    if (visible)
    {
        InputMethodCoordinator::hideInputMethod();
        ui->btnKeyboard->setChecked(false);
        return;
    }

    if (!isSupportedEditor(focused))
    {
        QMessageBox::information(this, "虚拟键盘",
                                 "请先点击一个可编辑输入框，再打开虚拟键盘。");
        ui->btnKeyboard->setChecked(false);
        return;
    }

    InputMethodCoordinator::showInputMethod();
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
