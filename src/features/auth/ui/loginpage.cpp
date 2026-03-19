/**
 * @file loginpage.cpp
 * @brief 登录页面实现文件 - 方案C 智能检测式
 *
 * 支持三种登录方式：
 * 1. 刷卡登录 - 系统自动检测
 * 2. 指纹登录 - 系统自动检测
 * 3. 账号密码登录 - 手动输入
 */

#include "loginpage.h"
#include "ui_loginpage.h"
#include <QAction>
#include <QDebug>
#include <QMenu>
#include <QMessageBox>

/**
 * @brief 构造函数
 * @param parent 父窗口指针
 */
LoginPage::LoginPage(QWidget *parent) : QWidget(parent),
                                        ui(new Ui::LoginPage),
                                        m_loginInProgress(false),
                                        m_accountListLoading(false),
                                        m_pendingAccountPopup(false),
                                        m_controller(new LoginController(nullptr, this))
{
    ui->setupUi(this); // 加载 loginpage.ui 界面

    // ========== 连接信号与槽 ==========

    // "登录" 按钮 → 验证账号密码
    connect(ui->loginBtn, &QPushButton::clicked,
            this, &LoginPage::onLoginButtonClicked);
    connect(ui->selectAccountBtn, &QPushButton::clicked,
            this, &LoginPage::onSelectAccountClicked);

    // 连接 Controller 信号
    connect(m_controller, &LoginController::loginSucceeded,
            this, &LoginPage::onLoginSucceeded);
    connect(m_controller, &LoginController::loginFailed,
            this, &LoginPage::onLoginFailed);
    connect(m_controller, &LoginController::loginStateChanged,
            this, &LoginPage::onLoginStateChanged);
    connect(m_controller, &LoginController::accountListReady,
            this, &LoginPage::onAccountListReady);
    connect(m_controller, &LoginController::accountListFailed,
            this, &LoginPage::onAccountListFailed);
    connect(m_controller, &LoginController::accountListStateChanged,
            this, &LoginPage::onAccountListStateChanged);

    setLoginInProgress(false);
    setAccountListLoading(false);

    qDebug() << "[LoginPage] 登录页面初始化完成";
}

/**
 * @brief 析构函数
 */
LoginPage::~LoginPage()
{
    delete ui;
}

/**
 * @brief 登录按钮点击槽函数
 * 使用 AuthService 进行验证
 */
void LoginPage::onLoginButtonClicked()
{
    if (m_loginInProgress || m_accountListLoading)
    {
        return;
    }

    // 获取用户输入
    QString username = ui->usernameEdit->text().trimmed();
    QString password = ui->passwordEdit->text();

    // 基本验证
    if (username.isEmpty() || password.isEmpty())
    {
        QMessageBox::warning(this, "提示", "请输入用户名和密码");
        return;
    }

    qDebug() << "[LoginPage] 开始登录，用户名:" << username;

    // 调用控制器
    m_controller->login(username, password);
}

void LoginPage::onSelectAccountClicked()
{
    if (m_loginInProgress || m_accountListLoading)
    {
        return;
    }

    if (!m_cachedAccounts.isEmpty())
    {
        showAccountDropdown();
        return;
    }

    m_pendingAccountPopup = true;
    m_controller->requestAccountList();
}

/**
 * @brief 登录成功回调
 */
void LoginPage::onLoginSucceeded(const QString &username, const QString &roleName)
{
    qDebug() << "[LoginPage] 登录成功，用户:" << username << "角色:" << roleName;

    // 清空密码输入（安全考虑）
    ui->passwordEdit->clear();

    // 发出登录成功信号给 MainWindow
    emit loginSuccess(username, roleName);
}

/**
 * @brief 登录失败回调
 */
void LoginPage::onLoginFailed(const QString &errorMessage)
{
    qDebug() << "[LoginPage] 登录失败:" << errorMessage;

    QMessageBox::warning(this, "登录失败", errorMessage);
    ui->passwordEdit->clear();
    ui->passwordEdit->setFocus();
}

void LoginPage::onLoginStateChanged(bool inProgress)
{
    setLoginInProgress(inProgress);
}

void LoginPage::onAccountListReady(const QStringList &accounts)
{
    m_cachedAccounts = accounts;
    if (m_pendingAccountPopup)
    {
        m_pendingAccountPopup = false;
        showAccountDropdown();
    }
}

void LoginPage::onAccountListFailed(const QString &errorMessage)
{
    m_pendingAccountPopup = false;
    qDebug() << "[LoginPage] 获取账号列表失败:" << errorMessage;
    QMessageBox::warning(this, "账号列表", errorMessage);
}

void LoginPage::onAccountListStateChanged(bool inProgress)
{
    setAccountListLoading(inProgress);
}

void LoginPage::setLoginInProgress(bool inProgress)
{
    m_loginInProgress = inProgress;
    const bool controlsEnabled = !m_loginInProgress && !m_accountListLoading;
    ui->loginBtn->setEnabled(controlsEnabled);
    ui->selectAccountBtn->setEnabled(controlsEnabled);
    if (inProgress)
    {
        ui->loginBtn->setText("登录中...");
    }
    else
    {
        ui->loginBtn->setText("登录");
    }
}

void LoginPage::setAccountListLoading(bool inProgress)
{
    m_accountListLoading = inProgress;
    const bool controlsEnabled = !m_loginInProgress && !m_accountListLoading;
    ui->loginBtn->setEnabled(controlsEnabled);
    ui->selectAccountBtn->setEnabled(controlsEnabled);
    ui->selectAccountBtn->setText(QStringLiteral("⌄"));
}

void LoginPage::onKeyboardVisibilityChanged(bool visible, int height)
{
    Q_UNUSED(visible);
    Q_UNUSED(height);
}

void LoginPage::showAccountDropdown()
{
    if (m_cachedAccounts.isEmpty())
    {
        return;
    }

    QMenu menu(this);
    menu.setStyleSheet(QStringLiteral(
        "QMenu { background-color: #FFFFFF; border: 1px solid #D0D7DE; border-radius: 8px; padding: 6px; }"
        "QMenu::item { padding: 8px 14px; color: #333333; border-radius: 6px; }"
        "QMenu::item:selected { background-color: #E8F5E9; color: #1B5E20; }"));

    for (const QString &account : m_cachedAccounts)
    {
        QAction *action = menu.addAction(account);
        connect(action, &QAction::triggered, this, [this, account]() {
            ui->usernameEdit->setText(account);
            ui->passwordEdit->setFocus();
        });
    }

    const QPoint popupPoint = ui->selectAccountBtn->mapToGlobal(QPoint(ui->selectAccountBtn->width() - 4, ui->selectAccountBtn->height()));
    menu.exec(popupPoint);
}
