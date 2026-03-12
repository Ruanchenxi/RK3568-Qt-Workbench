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
#include <QMessageBox>
#include <QDebug>

/**
 * @brief 构造函数
 * @param parent 父窗口指针
 */
LoginPage::LoginPage(QWidget *parent) : QWidget(parent),
                                        ui(new Ui::LoginPage),
                                        m_loginInProgress(false),
                                        m_controller(new LoginController(nullptr, this))
{
    ui->setupUi(this); // 加载 loginpage.ui 界面

    // 确保初始显示设备登录页面（页面索引0）
    ui->loginStackedWidget->setCurrentIndex(0);

    // ========== 连接信号与槽 ==========

    // "账号密码登录" 按钮 → 切换到账号密码页面
    connect(ui->accountLoginBtn, &QPushButton::clicked,
            this, &LoginPage::onShowAccountLogin);

    // "返回" 按钮 → 返回设备登录页面
    connect(ui->backBtn, &QPushButton::clicked,
            this, &LoginPage::onShowDeviceLogin);

    // "登录" 按钮 → 验证账号密码
    connect(ui->loginBtn, &QPushButton::clicked,
            this, &LoginPage::onLoginButtonClicked);

    // 连接 Controller 信号
    connect(m_controller, &LoginController::loginSucceeded,
            this, &LoginPage::onLoginSucceeded);
    connect(m_controller, &LoginController::loginFailed,
            this, &LoginPage::onLoginFailed);
    connect(m_controller, &LoginController::loginStateChanged,
            this, &LoginPage::onLoginStateChanged);

    setLoginInProgress(false);

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
 * @brief 显示账号密码登录页面
 */
void LoginPage::onShowAccountLogin()
{
    ui->loginStackedWidget->setCurrentIndex(1); // 切换到账号密码页面
    ui->usernameEdit->setFocus();               // 自动聚焦到用户名输入框
}

/**
 * @brief 返回设备登录页面（刷卡/指纹）
 */
void LoginPage::onShowDeviceLogin()
{
    ui->loginStackedWidget->setCurrentIndex(0); // 切换回设备登录页面
    ui->usernameEdit->clear();                  // 清空输入
    ui->passwordEdit->clear();
}

/**
 * @brief 登录按钮点击槽函数
 * 使用 AuthService 进行验证
 */
void LoginPage::onLoginButtonClicked()
{
    if (m_loginInProgress)
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

void LoginPage::setLoginInProgress(bool inProgress)
{
    m_loginInProgress = inProgress;
    ui->loginBtn->setEnabled(!inProgress);
    ui->accountLoginBtn->setEnabled(!inProgress);
    ui->backBtn->setEnabled(!inProgress);
    if (inProgress)
    {
        ui->loginBtn->setText("登录中...");
    }
    else
    {
        ui->loginBtn->setText("登录");
    }
}
