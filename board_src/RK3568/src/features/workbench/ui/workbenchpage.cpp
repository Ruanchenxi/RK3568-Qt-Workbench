/**
 * @file workbenchpage.cpp
 * @brief 工作台页面实现文件 - 内嵌网页显示
 */

#include "workbenchpage.h"
#include "ui_workbenchpage.h"
#include <QWebEnginePage>
#include <QVBoxLayout>
#include <QDebug>
#include <QUrl>
#include <QUrlQuery>

namespace
{
bool workbenchWebDebugEnabled()
{
    return qEnvironmentVariableIntValue("RK3568_WEB_DEBUG") == 1;
}

class DebugWorkbenchPage : public QWebEnginePage
{
public:
    explicit DebugWorkbenchPage(QObject *parent = nullptr)
        : QWebEnginePage(parent)
    {
    }

protected:
    bool acceptNavigationRequest(const QUrl &url, NavigationType type, bool isMainFrame) override
    {
        if (workbenchWebDebugEnabled()) {
            qDebug() << "[WorkbenchPage] 导航请求:"
                     << "url=" << url
                     << "type=" << type
                     << "mainFrame=" << isMainFrame;
        }
        return QWebEnginePage::acceptNavigationRequest(url, type, isMainFrame);
    }

    void javaScriptConsoleMessage(JavaScriptConsoleMessageLevel level,
                                  const QString &message,
                                  int lineNumber,
                                  const QString &sourceId) override
    {
        if (workbenchWebDebugEnabled()) {
            qDebug() << "[WorkbenchPage][Console]"
                     << "level=" << level
                     << "line=" << lineNumber
                     << "source=" << sourceId
                     << "message=" << message;
        }
        QWebEnginePage::javaScriptConsoleMessage(level, message, lineNumber, sourceId);
    }
};

QString maskedUrl(const QUrl &url)
{
    QUrl safeUrl(url);
    QUrlQuery query(safeUrl);
    const QStringList sensitiveKeys = {
        QStringLiteral("accessToken"),
        QStringLiteral("refreshToken"),
        QStringLiteral("token")
    };

    for (const QString &key : sensitiveKeys) {
        if (query.hasQueryItem(key)) {
            query.removeAllQueryItems(key);
            query.addQueryItem(key, QStringLiteral("***"));
        }
    }

    safeUrl.setQuery(query);
    return safeUrl.toString();
}
}

/**
 * @brief 构造函数
 * @param parent 父窗口指针
 */
WorkbenchPage::WorkbenchPage(QWidget *parent)
    : QWidget(parent),
      ui(new Ui::WorkbenchPage),
      m_webView(nullptr),
      m_controller(new WorkbenchController(nullptr, this)),
      m_webViewInitialized(false)
{
    ui->setupUi(this);
}

/**
 * @brief 析构函数
 */
WorkbenchPage::~WorkbenchPage()
{
    delete ui;
}

/**
 * @brief 初始化 WebEngine 视图
 */
void WorkbenchPage::ensureWebViewInitialized()
{
    if (m_webViewInitialized) {
        return;
    }

    initWebView();
    m_webViewInitialized = true;
}

void WorkbenchPage::initWebView()
{
    // 创建 QWebEngineView
    m_webView = new QWebEngineView(this);
    m_webView->setPage(new DebugWorkbenchPage(m_webView));

    // 将 WebView 添加到容器中
    QVBoxLayout *layout = new QVBoxLayout(ui->webViewContainer);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_webView);

    connect(m_webView, &QWebEngineView::loadStarted, this, [this]()
            {
        if (workbenchWebDebugEnabled()) {
            qDebug() << "[WorkbenchPage] loadStarted, currentUrl=" << maskedUrl(m_webView->url());
        } });

    connect(m_webView, &QWebEngineView::loadProgress, this, [](int progress)
            {
        if (workbenchWebDebugEnabled()) {
            qDebug() << "[WorkbenchPage] loadProgress:" << progress;
        } });

    connect(m_webView, &QWebEngineView::urlChanged, this, [](const QUrl &url)
            {
        if (!workbenchWebDebugEnabled()) {
            return;
        }

        const QString safeUrl = maskedUrl(url);
        qDebug() << "[WorkbenchPage] urlChanged:" << safeUrl;

        const QString path = url.path();
        if (path.contains(QStringLiteral("/login"))) {
            qWarning() << "[WorkbenchPage] 检测到页面跳转到登录页:" << safeUrl;
        } });

    connect(m_webView, &QWebEngineView::titleChanged, this, [](const QString &title)
            {
        if (workbenchWebDebugEnabled()) {
            qDebug() << "[WorkbenchPage] titleChanged:" << title;
        } });

    connect(m_webView, &QWebEngineView::loadFinished, this, [=](bool ok)
            {
        if (workbenchWebDebugEnabled()) {
            if (ok) {
                qDebug() << "[WorkbenchPage] 网页加载成功, finalUrl=" << maskedUrl(m_webView->url());
            } else {
                qWarning() << "[WorkbenchPage] 网页加载失败, finalUrl=" << maskedUrl(m_webView->url());
            }
        } });
}

/**
 * @brief 加载工作台网页（带 token）
 */
void WorkbenchPage::loadWorkbench()
{
    ensureWebViewInitialized();
    if (!m_webView) {
        return;
    }

    if (workbenchWebDebugEnabled()) {
        qDebug() << "[WorkbenchPage] ========== 开始加载工作台 ==========";
    }
    QUrl targetUrl;
    QString safeSummary;
    QString error;
    if (!m_controller->buildTargetUrl(&targetUrl, &safeSummary, &error))
    {
        qWarning() << "[WorkbenchPage]" << error;
        return;
    }
    if (workbenchWebDebugEnabled()) {
        qDebug() << "[WorkbenchPage] 使用中转页面 URL（已脱敏）:" << safeSummary;
        qDebug() << "[WorkbenchPage] 实际加载 URL（敏感参数已脱敏）:" << maskedUrl(targetUrl);
    }

    // 加载网页
    if (workbenchWebDebugEnabled()) {
        qDebug() << "[WorkbenchPage] 开始加载网页...";
    }
    m_webView->load(targetUrl);
    if (workbenchWebDebugEnabled()) {
        qDebug() << "[WorkbenchPage] ========== 加载工作台结束 ==========";
    }
}

/**
 * @brief 页面显示事件 - 首次显示或 token 变化时才重新加载
 */
void WorkbenchPage::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    ensureWebViewInitialized();
    if (m_controller->shouldLoadNow()) {
        loadWorkbench();
    }
}
