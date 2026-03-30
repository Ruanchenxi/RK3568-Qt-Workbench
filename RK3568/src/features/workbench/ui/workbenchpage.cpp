/**
 * @file workbenchpage.cpp
 * @brief 工作台页面实现文件 - 内嵌网页显示
 */

#include "workbenchpage.h"
#include "ui_workbenchpage.h"
#include "features/keyboard/ui/KeyboardContainer.h"
#include <QWebEnginePage>
#include <QVBoxLayout>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
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
      m_webViewInitialized(false),
      m_renderProcessTerminated(false),
      m_reloadScheduled(false)
      , m_recreatePageOnReload(false)
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

void WorkbenchPage::requestKeyboardActivation(KeyboardContainer *container, std::function<void(bool)> callback)
{
    ensureWebViewInitialized();
    if (!container || !m_webView || !m_webView->page())
    {
        if (callback)
        {
            callback(false);
        }
        return;
    }

    static const QString script = QStringLiteral(R"JS(
(function(){
    const el = document.activeElement;
    if (!el) return false;
    const tag = (el.tagName || '').toLowerCase();
    const type = (el.type || '').toLowerCase();
    const blocked = ['button', 'checkbox', 'radio', 'submit', 'file', 'hidden', 'image', 'reset'];
    return ((tag === 'input' && !blocked.includes(type)) || tag === 'textarea' || el.isContentEditable) && !el.disabled && !el.readOnly;
})();
)JS");

    m_webView->page()->runJavaScript(script, [this, container, callback](const QVariant &result) {
        const bool ready = result.toBool();
        if (ready)
        {
            container->setActionHandlers(
                [this](const QString &text) { insertTextFromKeyboard(text); },
                [this]() { backspaceFromKeyboard(); },
                [this]() { commitFromKeyboard(); },
                [this]() { clearFromKeyboard(); });
        }
        else
        {
            container->clearActionHandlers();
        }

        if (callback)
        {
            callback(ready);
        }
    });
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
    attachDebugPage(false);

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
        if (ok) {
            m_renderProcessTerminated = false;
            m_reloadScheduled = false;
        }
        if (workbenchWebDebugEnabled()) {
            if (ok) {
                qDebug() << "[WorkbenchPage] 网页加载成功, finalUrl=" << maskedUrl(m_webView->url());
            } else {
                qWarning() << "[WorkbenchPage] 网页加载失败, finalUrl=" << maskedUrl(m_webView->url());
            }
        } });

}

void WorkbenchPage::attachDebugPage(bool recreate)
{
    if (!m_webView) {
        return;
    }

    if (recreate && m_webView->page()) {
        qWarning() << "[WorkbenchPage] 重建工作台 WebEngine page";
        m_webView->page()->deleteLater();
    }

    auto *page = new DebugWorkbenchPage(m_webView);
    m_webView->setPage(page);
    connect(page, &QWebEnginePage::renderProcessTerminated, this,
            [this](QWebEnginePage::RenderProcessTerminationStatus terminationStatus, int exitCode)
            {
        m_renderProcessTerminated = true;
        m_recreatePageOnReload = true;
        qWarning() << "[WorkbenchPage] 工作台渲染进程终止:"
                   << "status=" << terminationStatus
                   << "exitCode=" << exitCode
                   << "currentUrl=" << maskedUrl(m_webView ? m_webView->url() : QUrl());
        scheduleWorkbenchReload(); });
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
    if (m_renderProcessTerminated) {
        qWarning() << "[WorkbenchPage] 页面重新显示时检测到渲染异常，准备重新加载工作台";
        scheduleWorkbenchReload();
        return;
    }
    if (m_controller->shouldLoadNow()) {
        loadWorkbench();
        return;
    }

    if (m_webView) {
        m_webView->show();
        m_webView->update();
    }
}

void WorkbenchPage::scheduleWorkbenchReload()
{
    if (!m_webView || m_reloadScheduled) {
        return;
    }

    if (m_reloadCooldownTimer.isValid() && m_reloadCooldownTimer.elapsed() < 1200) {
        qWarning() << "[WorkbenchPage] 工作台恢复重载已在冷却中，跳过本次重复触发";
        return;
    }

    m_reloadScheduled = true;
    QTimer::singleShot(300, this, [this]() {
        m_reloadScheduled = false;
        if (!m_webView) {
            return;
        }

        if (!isVisible()) {
            qWarning() << "[WorkbenchPage] 工作台当前不可见，延迟到下次显示时再恢复";
            return;
        }

        m_reloadCooldownTimer.restart();
        if (m_recreatePageOnReload) {
            attachDebugPage(true);
            m_recreatePageOnReload = false;
        }
        qWarning() << "[WorkbenchPage] 尝试重新加载工作台，以恢复白屏/渲染异常";
        loadWorkbench();
    });
}

void WorkbenchPage::insertTextFromKeyboard(const QString &text)
{
    if (!m_webView || !m_webView->page())
    {
        return;
    }

    QJsonObject payloadObject;
    payloadObject.insert(QStringLiteral("text"), text);
    const QString payload = QString::fromUtf8(
        QJsonDocument(payloadObject).toJson(QJsonDocument::Compact));
    const QString script = QStringLiteral(R"JS(
(function(payload){
    const el = document.activeElement;
    if (!el) return false;
    const tag = (el.tagName || '').toLowerCase();
    const type = (el.type || '').toLowerCase();
    const blocked = ['button', 'checkbox', 'radio', 'submit', 'file', 'hidden', 'image', 'reset'];
    const isEditable = ((tag === 'input' && !blocked.includes(type)) || tag === 'textarea' || el.isContentEditable) && !el.disabled && !el.readOnly;
    if (!isEditable) return false;

    const text = payload.text || '';
    el.focus();

    if (el.isContentEditable) {
        if (document.queryCommandSupported && document.queryCommandSupported('insertText')) {
            document.execCommand('insertText', false, text);
        } else {
            const selection = window.getSelection();
            if (selection && selection.rangeCount > 0) {
                selection.deleteFromDocument();
                selection.getRangeAt(0).insertNode(document.createTextNode(text));
            } else {
                el.textContent = (el.textContent || '') + text;
            }
        }
        el.dispatchEvent(new Event('input', {bubbles:true}));
        return true;
    }

    const value = el.value || '';
    const start = typeof el.selectionStart === 'number' ? el.selectionStart : value.length;
    const end = typeof el.selectionEnd === 'number' ? el.selectionEnd : start;
    el.value = value.slice(0, start) + text + value.slice(end);
    const pos = start + text.length;
    if (typeof el.setSelectionRange === 'function') {
        el.setSelectionRange(pos, pos);
    }
    el.dispatchEvent(new Event('input', {bubbles:true}));
    el.dispatchEvent(new Event('change', {bubbles:true}));
    return true;
})(%1);
)JS").arg(payload);

    m_webView->page()->runJavaScript(script);
}

void WorkbenchPage::backspaceFromKeyboard()
{
    if (!m_webView || !m_webView->page())
    {
        return;
    }

    static const QString script = QStringLiteral(R"JS(
(function(){
    const el = document.activeElement;
    if (!el) return false;
    const tag = (el.tagName || '').toLowerCase();
    const type = (el.type || '').toLowerCase();
    const blocked = ['button', 'checkbox', 'radio', 'submit', 'file', 'hidden', 'image', 'reset'];
    const isEditable = ((tag === 'input' && !blocked.includes(type)) || tag === 'textarea' || el.isContentEditable) && !el.disabled && !el.readOnly;
    if (!isEditable) return false;

    el.focus();
    if (el.isContentEditable) {
        document.execCommand('delete', false, null);
        el.dispatchEvent(new Event('input', {bubbles:true}));
        return true;
    }

    const value = el.value || '';
    let start = typeof el.selectionStart === 'number' ? el.selectionStart : value.length;
    let end = typeof el.selectionEnd === 'number' ? el.selectionEnd : start;
    if (start === end && start > 0) {
        start -= 1;
    }
    el.value = value.slice(0, start) + value.slice(end);
    if (typeof el.setSelectionRange === 'function') {
        el.setSelectionRange(start, start);
    }
    el.dispatchEvent(new Event('input', {bubbles:true}));
    el.dispatchEvent(new Event('change', {bubbles:true}));
    return true;
})();
)JS");

    m_webView->page()->runJavaScript(script);
}

void WorkbenchPage::commitFromKeyboard()
{
    if (!m_webView || !m_webView->page())
    {
        return;
    }

    static const QString script = QStringLiteral(R"JS(
(function(){
    const el = document.activeElement;
    if (!el) return false;
    const tag = (el.tagName || '').toLowerCase();
    if (tag === 'textarea' || el.isContentEditable) {
        if (document.queryCommandSupported && document.queryCommandSupported('insertLineBreak')) {
            document.execCommand('insertLineBreak', false, null);
        } else if (document.queryCommandSupported && document.queryCommandSupported('insertText')) {
            document.execCommand('insertText', false, '\n');
        }
        el.dispatchEvent(new Event('input', {bubbles:true}));
        return true;
    }
    if (typeof el.blur === 'function') {
        el.blur();
    }
    return true;
})();
)JS");

    m_webView->page()->runJavaScript(script);
}

void WorkbenchPage::clearFromKeyboard()
{
    if (!m_webView || !m_webView->page())
    {
        return;
    }

    static const QString script = QStringLiteral(R"JS(
(function(){
    const el = document.activeElement;
    if (!el) return false;
    const tag = (el.tagName || '').toLowerCase();
    const type = (el.type || '').toLowerCase();
    const blocked = ['button', 'checkbox', 'radio', 'submit', 'file', 'hidden', 'image', 'reset'];
    const isEditable = ((tag === 'input' && !blocked.includes(type)) || tag === 'textarea' || el.isContentEditable) && !el.disabled && !el.readOnly;
    if (!isEditable) return false;

    el.focus();
    if (el.isContentEditable) {
        el.textContent = '';
    } else {
        el.value = '';
        if (typeof el.setSelectionRange === 'function') {
            el.setSelectionRange(0, 0);
        }
    }
    el.dispatchEvent(new Event('input', {bubbles:true}));
    el.dispatchEvent(new Event('change', {bubbles:true}));
    return true;
})();
)JS");

    m_webView->page()->runJavaScript(script);
}
