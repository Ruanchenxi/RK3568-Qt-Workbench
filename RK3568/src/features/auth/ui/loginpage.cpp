/**
 * @file loginpage.cpp
 * @brief 登录页面实现文件 - 账号密码登录主线
 */

#include "loginpage.h"
#include "ui_loginpage.h"
#include <QApplication>
#include "core/ConfigManager.h"
#include "features/auth/infra/device/CardSerialSource.h"
#include <QAbstractButton>
#include <QDebug>
#include <QDialog>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPixmap>
#include <QScreen>
#include <QStyle>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

/**
 * @brief 构造函数
 * @param parent 父窗口指针
 */
LoginPage::LoginPage(QWidget *parent) : QWidget(parent),
                                        ui(new Ui::LoginPage),
                                        m_loginInProgress(false),
                                        m_accountListLoading(false),
                                        m_pendingAccountPopup(false),
                                        m_controller(new LoginController(nullptr, this)),
                                        m_cardSource(new CardSerialSource(this)),
                                        m_probeManager(new QNetworkAccessManager(this)),
                                        m_probeReply(nullptr),
                                        m_probeInFlight(false),
                                        m_serviceReadyTimer(new QTimer(this)),
                                        m_serviceReady(false)
{
    ui->setupUi(this); // 加载 loginpage.ui 界面

    QPixmap logoPix(QStringLiteral(":/branding/logo.png"));
    if (!logoPix.isNull() && ui->logoLabel)
    {
        ui->logoLabel->setPixmap(
            logoPix.scaled(ui->logoLabel->size(),
                           Qt::KeepAspectRatio,
                           Qt::SmoothTransformation));
    }

    // ========== 连接信号与槽 ==========

    // "登录" 按钮 → 验证账号密码
    connect(ui->loginBtn, &QPushButton::clicked,
            this, &LoginPage::onLoginButtonClicked);
    connect(ui->selectAccountBtn, &QAbstractButton::clicked,
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
    connect(m_cardSource, &CardSerialSource::cardCaptured,
            this, &LoginPage::onCardCaptured);
    connect(m_cardSource, &CardSerialSource::sourceError,
            this, &LoginPage::onCardSourceError);
    connect(m_cardSource, &CardSerialSource::readerStatusChanged,
            this, &LoginPage::cardReaderStatusChanged);
    connect(ConfigManager::instance(), &ConfigManager::configChanged,
            this, &LoginPage::onConfigChanged);
    connect(qApp, &QApplication::focusChanged, this, [this](QWidget *, QWidget *) {
        updateAccountRowHighlight();
    });
    connect(m_serviceReadyTimer, &QTimer::timeout,
            this, &LoginPage::refreshServiceReadyState);
    m_serviceReadyTimer->setInterval(1500);
    m_serviceReadyTimer->setSingleShot(false);

    setLoginInProgress(false);
    setAccountListLoading(false);
    setServiceReady(false, QStringLiteral("后台服务启动中，请稍候..."));
    updateAccountRowHighlight();

    if (ui->accountLoginDescLabel)
    {
        ui->accountLoginDescLabel->setText(QStringLiteral("后台服务启动中，请稍候..."));
    }
    if (ui->loginFootHint)
    {
        ui->loginFootHint->setText(QStringLiteral("服务就绪后支持账号密码登录和刷卡直接登录。"));
    }

    qDebug() << "[LoginPage] 登录页面初始化完成";
}

/**
 * @brief 析构函数
 */
LoginPage::~LoginPage()
{
    cancelServiceReadyProbe();
    delete ui;
}

/**
 * @brief 登录按钮点击槽函数
 * 使用 AuthService 进行验证
 */
void LoginPage::onLoginButtonClicked()
{
    if (!m_serviceReady)
    {
        QMessageBox::information(this, "服务启动中",
                                 m_serviceReadyMessage.isEmpty()
                                     ? QStringLiteral("服务尚未就绪，请稍候后再试。")
                                     : m_serviceReadyMessage);
        return;
    }

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
    if (!m_serviceReady)
    {
        QMessageBox::information(this, "服务启动中",
                                 m_serviceReadyMessage.isEmpty()
                                     ? QStringLiteral("服务尚未就绪，请稍候后再试。")
                                     : m_serviceReadyMessage);
        return;
    }

    if (m_loginInProgress || m_accountListLoading)
    {
        return;
    }

    if (!m_cachedAccounts.isEmpty() && m_cachedAccountListKey == currentAccountListCacheKey())
    {
        showAccountDropdown();
        return;
    }

    m_pendingAccountPopup = true;
    m_pendingAccountListKey = currentAccountListCacheKey();
    m_controller->requestAccountList();
}

void LoginPage::onCardCaptured(const CardCredential &credential)
{
    if (!isVisible() || !m_serviceReady || m_loginInProgress || m_accountListLoading)
    {
        return;
    }

    const QString cardNo = credential.cardNo.trimmed();
    if (cardNo.isEmpty())
    {
        return;
    }

    qDebug() << "[LoginPage] 检测到刷卡登录，cardNo=" << cardNo;
    m_controller->loginByCard(cardNo);
}

void LoginPage::onCardSourceError(const QString &message)
{
    qDebug() << "[LoginPage] 读卡器状态:" << message;
}

void LoginPage::onConfigChanged(const QString &key, const QVariant &value)
{
    Q_UNUSED(value);

    if (key == QStringLiteral("system/apiUrl") || key == QStringLiteral("system/tenantCode"))
    {
        invalidateAccountListCache();
        refreshServiceReadyState();
    }
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
    if (m_pendingAccountListKey != currentAccountListCacheKey())
    {
        m_pendingAccountPopup = false;
        m_pendingAccountListKey.clear();
        return;
    }

    m_cachedAccounts = accounts;
    m_cachedAccountListKey = m_pendingAccountListKey;
    m_pendingAccountListKey.clear();
    if (m_pendingAccountPopup)
    {
        m_pendingAccountPopup = false;
        showAccountDropdown();
    }
}

void LoginPage::onAccountListFailed(const QString &errorMessage)
{
    if (m_pendingAccountListKey != currentAccountListCacheKey())
    {
        m_pendingAccountPopup = false;
        m_pendingAccountListKey.clear();
        return;
    }

    m_pendingAccountPopup = false;
    m_pendingAccountListKey.clear();
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
    updateInteractionState();
    if (inProgress)
    {
        ui->loginBtn->setText(QStringLiteral("授权中..."));
    }
    else
    {
        ui->loginBtn->setText(QStringLiteral("授权登录"));
    }
}

void LoginPage::setAccountListLoading(bool inProgress)
{
    m_accountListLoading = inProgress;
    updateInteractionState();
}

QString LoginPage::currentAccountListCacheKey() const
{
    QString apiUrl = ConfigManager::instance()->apiUrl().trimmed();
    QString accountListUrl = ConfigManager::instance()->value("auth/accountListUrl", "").toString().trimmed();
    QString tenantCode = ConfigManager::instance()->tenantCode().trimmed();

    if (tenantCode.isEmpty())
    {
        tenantCode = QStringLiteral("000000");
    }

    return apiUrl + QLatin1Char('|') + accountListUrl + QLatin1Char('|') + tenantCode;
}

void LoginPage::invalidateAccountListCache()
{
    m_cachedAccounts.clear();
    m_cachedAccountListKey.clear();
    m_pendingAccountPopup = false;
    m_pendingAccountListKey.clear();
}

void LoginPage::updateInteractionState()
{
    const bool controlsEnabled = m_serviceReady && !m_loginInProgress && !m_accountListLoading;
    ui->loginBtn->setEnabled(controlsEnabled);
    ui->selectAccountBtn->setEnabled(controlsEnabled);
}

void LoginPage::updateAccountRowHighlight()
{
    if (!ui || !ui->selectAccountBtn || !ui->usernameEdit)
    {
        return;
    }

    QWidget *focusWidget = QApplication::focusWidget();
    const bool accountRowActive = (focusWidget == ui->usernameEdit || focusWidget == ui->selectAccountBtn);
    ui->selectAccountBtn->setProperty("linkedFocus", accountRowActive);
    ui->selectAccountBtn->style()->unpolish(ui->selectAccountBtn);
    ui->selectAccountBtn->style()->polish(ui->selectAccountBtn);
    ui->selectAccountBtn->update();
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

    // 面向 4:3 工业触摸屏的卡片式账号选择弹层：
    // 1) 宽度与账号输入行一致，保证触控对齐
    // 2) 最大显示 5 项，避免在 1024x768 上铺得过高
    // 3) 超出屏幕时自动改为向上展开，减少边缘裁切
    QDialog *popup = new QDialog(this, Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
    popup->setAttribute(Qt::WA_DeleteOnClose);
    popup->setAttribute(Qt::WA_TranslucentBackground);

    QVBoxLayout *layout = new QVBoxLayout(popup);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->setSpacing(0);

    QFrame *card = new QFrame(popup);
    card->setObjectName(QStringLiteral("accountPopupCard"));
    card->setStyleSheet(
        "QFrame#accountPopupCard {"
        "  background: #FFFFFF;"
        "  border: 1px solid #D7E0D4;"
        "  border-radius: 16px;"
        "}"
        "QLabel#accountPopupTitle {"
        "  color: #1E2A21;"
        "  font-size: 13px;"
        "  font-weight: 700;"
        "  padding: 14px 18px 10px 18px;"
        "}"
        "QListWidget#accountPopupList {"
        "  background: transparent;"
        "  border: none;"
        "  outline: none;"
        "}"
        "QListWidget#accountPopupList::item {"
        "  min-height: 58px;"
        "  padding: 0 18px;"
        "  font-size: 15px;"
        "  color: #26332A;"
        "  border-top: 1px solid #EEF3EC;"
        "}"
        "QListWidget#accountPopupList::item:selected {"
        "  background: #EAF4EA;"
        "  color: #1D6A27;"
        "}"
        "QListWidget#accountPopupList::item:hover {"
        "  background: #F4F7F1;"
        "}"
        "QScrollBar:vertical {"
        "  background: transparent;"
        "  width: 12px;"
        "  margin: 8px 4px 8px 0;"
        "}"
        "QScrollBar::handle:vertical {"
        "  background: #C7D3C4;"
        "  border-radius: 6px;"
        "  min-height: 28px;"
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
        "  height: 0;"
        "}"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {"
        "  background: transparent;"
        "}");
    layout->addWidget(card);

    auto *shadow = new QGraphicsDropShadowEffect(card);
    shadow->setBlurRadius(28.0);
    shadow->setOffset(0.0, 8.0);
    shadow->setColor(QColor(19, 32, 24, 56));
    card->setGraphicsEffect(shadow);

    QVBoxLayout *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(0, 0, 0, 0);
    cardLayout->setSpacing(0);

    QLabel *title = new QLabel(QStringLiteral("选择操作员账号"), card);
    title->setObjectName(QStringLiteral("accountPopupTitle"));
    cardLayout->addWidget(title);

    QListWidget *list = new QListWidget(card);
    list->setObjectName(QStringLiteral("accountPopupList"));
    list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    list->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    list->setSelectionMode(QAbstractItemView::SingleSelection);
    list->setFocusPolicy(Qt::NoFocus);
    cardLayout->addWidget(list);

    for (const QString &account : m_cachedAccounts)
    {
        QListWidgetItem *item = new QListWidgetItem(account, list);
        item->setSizeHint(QSize(0, 58));
        if (account == ui->usernameEdit->text().trimmed())
        {
            list->setCurrentItem(item);
        }
    }

    const int popupWidth = ui->usernameEdit->width() + ui->selectAccountBtn->width();
    const int maxVisible = qMin(m_cachedAccounts.size(), 5);
    const int listHeight = maxVisible * 58;
    card->setFixedWidth(qMax(popupWidth, 320));
    list->setFixedHeight(listHeight);
    popup->adjustSize();

    const QPoint anchorBelow = ui->usernameEdit->mapToGlobal(
        QPoint(0, ui->usernameEdit->height() + 8));

    QRect availableGeometry;
    if (QScreen *screen = QGuiApplication::screenAt(anchorBelow))
    {
        availableGeometry = screen->availableGeometry();
    }
    else if (QScreen *screen = QGuiApplication::primaryScreen())
    {
        availableGeometry = screen->availableGeometry();
    }

    int x = anchorBelow.x();
    int y = anchorBelow.y();

    if (!availableGeometry.isNull())
    {
        if (x + popup->width() > availableGeometry.right())
        {
            x = availableGeometry.right() - popup->width() - 12;
        }
        if (x < availableGeometry.left() + 12)
        {
            x = availableGeometry.left() + 12;
        }

        if (y + popup->height() > availableGeometry.bottom())
        {
            const QPoint anchorAbove = ui->usernameEdit->mapToGlobal(
                QPoint(0, -popup->height() - 8));
            y = qMax(anchorAbove.y(), availableGeometry.top() + 12);
        }
    }

    popup->move(x, y);

    connect(list, &QListWidget::itemClicked, this, [this, popup](QListWidgetItem *item) {
        ui->usernameEdit->setText(item->text());
        ui->passwordEdit->setFocus();
        popup->close();
    });

    popup->show();
}

void LoginPage::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);

    refreshServiceReadyState();
    if (m_serviceReadyTimer)
    {
        m_serviceReadyTimer->start();
    }
}

void LoginPage::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);

    if (m_serviceReadyTimer)
    {
        m_serviceReadyTimer->stop();
    }

    cancelServiceReadyProbe();

    if (m_cardSource)
    {
        m_cardSource->stop();
    }
}

void LoginPage::refreshServiceReadyState()
{
    startServiceReadyProbe();
}

void LoginPage::setServiceReady(bool ready, const QString &message)
{
    m_serviceReady = ready;
    m_serviceReadyMessage = ready
        ? QStringLiteral("服务已就绪，可进行账号密码登录或刷卡登录。")
        : (message.trimmed().isEmpty() ? QStringLiteral("服务启动中，请稍候...") : message.trimmed());

    if (ui->accountLoginDescLabel)
    {
        ui->accountLoginDescLabel->setText(m_serviceReady
            ? QStringLiteral("请输入操作员账号和登录密码，也可直接刷卡登录。")
            : m_serviceReadyMessage);
    }

    if (ui->loginFootHint)
    {
        ui->loginFootHint->setText(m_serviceReady
            ? QStringLiteral("支持刷卡直接登录；若无响应，请重新贴卡后再试。")
            : QStringLiteral("后台服务启动完成前，暂不开放账号密码登录和刷卡登录。"));
    }

    if (m_cardSource)
    {
        if (m_serviceReady && isVisible())
        {
            m_cardSource->start();
        }
        else
        {
            m_cardSource->stop();
        }
    }

    updateInteractionState();
}

void LoginPage::startServiceReadyProbe()
{
    QString accountListUrl = ConfigManager::instance()->value(QStringLiteral("auth/accountListUrl"), QString()).toString().trimmed();
    QString apiUrlText = ConfigManager::instance()->apiUrl().trimmed();
    QString tenantCode = ConfigManager::instance()->tenantCode().trimmed();

    if (m_probeInFlight || !m_probeManager)
    {
        return;
    }

    if (tenantCode.isEmpty())
    {
        tenantCode = QStringLiteral("000000");
    }

    if (accountListUrl.isEmpty())
    {
        if (apiUrlText.isEmpty())
        {
            setServiceReady(false, QStringLiteral("服务地址未配置，请先检查接口地址。"));
            return;
        }

        while (apiUrlText.endsWith('/'))
        {
            apiUrlText.chop(1);
        }
        accountListUrl = apiUrlText + QStringLiteral("/list-account");
    }

    const QUrl requestUrl = QUrl::fromUserInput(accountListUrl);
    if (!requestUrl.isValid())
    {
        setServiceReady(false, QStringLiteral("账号列表地址无效，请检查接口配置。"));
        return;
    }

    QNetworkRequest request(requestUrl);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("kids-requested-with", "KidsHttpRequest");
    request.setRawHeader("Authorization", "Basic cmlkZXI6cmlkZXJfc2VjcmV0");

    const QByteArray body = QJsonDocument(QJsonObject{
        {QStringLiteral("tenantId"), tenantCode}
    }).toJson(QJsonDocument::Compact);

    m_probeReply = m_probeManager->post(request, body);
    m_probeInFlight = true;

    QNetworkReply *reply = m_probeReply;
    QTimer::singleShot(1200, this, [this, reply]() {
        if (reply && reply == m_probeReply && reply->isRunning())
        {
            reply->setProperty("probeTimedOut", true);
            reply->abort();
        }
    });

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (!reply)
        {
            return;
        }

        if (reply != m_probeReply)
        {
            reply->deleteLater();
            return;
        }

        m_probeReply = nullptr;
        m_probeInFlight = false;

        const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray responseData = reply->readAll();

        if (reply->error() != QNetworkReply::NoError)
        {
            const bool timedOut = reply->property("probeTimedOut").toBool();
            setServiceReady(false,
                            timedOut
                                ? QStringLiteral("后台服务启动中，请稍候...")
                                : QStringLiteral("后台服务启动中，请稍候... 当前账号列表探测未就绪：%1")
                                      .arg(reply->errorString()));
            reply->deleteLater();
            return;
        }

        if (httpStatus < 200 || httpStatus >= 300)
        {
            const QString bodyText = QString::fromUtf8(responseData).trimmed();
            setServiceReady(false,
                            bodyText.isEmpty()
                                ? QStringLiteral("后台服务启动中，请稍候... 当前探测状态 HTTP %1").arg(httpStatus)
                                : bodyText);
            reply->deleteLater();
            return;
        }

        setServiceReady(true, QStringLiteral("服务已就绪"));
        reply->deleteLater();
    });
}

void LoginPage::cancelServiceReadyProbe()
{
    if (!m_probeReply)
    {
        m_probeInFlight = false;
        return;
    }

    QNetworkReply *reply = m_probeReply;
    m_probeReply = nullptr;
    m_probeInFlight = false;
    reply->abort();
    reply->deleteLater();
}
