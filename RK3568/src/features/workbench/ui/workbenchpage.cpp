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
      , m_pendingBusinessReload(false)
      , m_reloadAttemptCount(0)
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
    // 创建 QWebEngineView，父对象直接给容器，让 Chromium 进程从初始化起就拿到正确的视口大小
    m_webView = new QWebEngineView(ui->webViewContainer);
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
            m_reloadAttemptCount = 0;
        }
        if (workbenchWebDebugEnabled()) {
            if (ok) {
                qDebug() << "[WorkbenchPage] 网页加载成功, finalUrl=" << maskedUrl(m_webView->url());
            } else {
                qWarning() << "[WorkbenchPage] 网页加载失败, finalUrl=" << maskedUrl(m_webView->url());
            }
        }

        // 触摸拖拽滚动 shim：补全 Qt 触摸合成器在 QT_XCB_NO_XI2=1 下丢失的滚动能力
        // 根因：XI2 关闭后拖拽只发 buttons=1 的 pointermove，不发 pointerdown，
        //       uni-app scroll-view 等不到 pointerdown 无法识别滚动手势。
        //       shim 直接监听 buttons=1 的 move，找最近可滚动祖先，操作其 scrollTop。
        // 安全：buttons=0（hover/接近）时重置状态；真实拖拽后压制 click 防误触；tap 有 pointerdown 负责重置，不干预单击。
        if (ok && m_webView && m_webView->page()) {
            static const QString shim = QStringLiteral(R"JS(
(function(){
    if (window.__rk_scroll_shim_installed) return 'already';
    window.__rk_scroll_shim_installed = true;

    var IDLE_MS  = 350; // ms，超过此间隔视为新手势
    var DRAG_PX  = 10;  // px，累计位移超过此值才算真实拖拽，用于压制手指抬起时的误触 click

    var last      = null;
    var scroller  = null;
    var dragStartX = 0, dragStartY = 0;
    var dragMoved  = false; // 本次手势是否发生了真实位移

    function findScrollable(el) {
        while (el && el !== document.documentElement) {
            var oy = getComputedStyle(el).overflowY;
            if ((oy === 'auto' || oy === 'scroll') && el.scrollHeight > el.clientHeight) {
                return el;
            }
            el = el.parentElement;
        }
        return null;
    }

    document.addEventListener('pointermove', function(e) {
        if (e.buttons === 0) {
            // 手指接近但未按压，重置手势状态
            last = null; scroller = null;
            return;
        }

        // buttons=1：手指按压移动，Qt 合成器不发 pointerdown，直接靠 buttons=1 识别拖拽
        var now = e.timeStamp;

        if (!last || (now - last.t) > IDLE_MS) {
            last = { x: e.clientX, y: e.clientY, t: now };
            dragStartX = e.clientX;
            dragStartY = e.clientY;
            scroller = findScrollable(e.target);
            return;
        }

        var dy = e.clientY - last.y;
        var dx = e.clientX - last.x;

        // 累计位移超过阈值：标记为真实拖拽，抬手时压制 click 防止误入详情页
        if (!dragMoved &&
            (Math.abs(e.clientY - dragStartY) > DRAG_PX ||
             Math.abs(e.clientX - dragStartX) > DRAG_PX)) {
            dragMoved = true;
        }

        if (scroller) {
            scroller.scrollTop -= dy;
            scroller.scrollLeft -= dx;
        }

        last.x = e.clientX;
        last.y = e.clientY;
        last.t = now;
    }, { capture: true, passive: true });

    // 拖拽结束后抬手会触发 click，压制它防止误触列表项进入详情页
    document.addEventListener('click', function(e) {
        if (dragMoved) {
            e.preventDefault();
            e.stopPropagation();
            dragMoved = false;
        }
    }, { capture: true }); // 不能 passive，需要 preventDefault

    // tap 有 pointerdown，重置手势状态和拖拽标记
    document.addEventListener('pointerdown', function() {
        last = null; scroller = null;
        dragMoved = false;
    }, { capture: true, passive: true });

    return 'installed';
})();
)JS");
            m_webView->page()->runJavaScript(shim, [](const QVariant &r){
                qDebug() << "[WorkbenchPage] 触摸滚动shim注入结果:" << r.toString();
            });
        }

        // API 操作反馈 shim：H5 某些 POST 接口返回 {success:true} 但无 msg，导致页面无任何提示。
        // 拦截 fetch，检测到 success=true 且无 msg 时主动弹 toast。
        if (ok && m_webView && m_webView->page()) {
            static const QString feedbackShim = QStringLiteral(R"JS(
(function(){
    if (window.__rk_api_feedback_installed) return 'already';
    window.__rk_api_feedback_installed = true;

    function showToast(msg) {
        if (typeof uni !== 'undefined' && uni.showToast) {
            uni.showToast({ title: msg, icon: 'success', duration: 2000 });
            return;
        }
        var div = document.createElement('div');
        div.style.cssText = 'position:fixed;top:50%;left:50%;transform:translate(-50%,-50%);'
            + 'background:rgba(0,0,0,0.72);color:#fff;padding:12px 28px;border-radius:8px;'
            + 'z-index:99999;font-size:16px;pointer-events:none;';
        div.textContent = msg;
        document.body.appendChild(div);
        setTimeout(function(){ div.parentNode && div.parentNode.removeChild(div); }, 2000);
    }

    var _fetch = window.fetch;
    window.fetch = function(input, init) {
        var method = (init && init.method) ? init.method.toUpperCase() : 'GET';
        return _fetch.apply(this, arguments).then(function(resp) {
            if (method !== 'POST') return resp;
            return resp.clone().text().then(function(text) {
                try {
                    var json = JSON.parse(text);
                    if (json.success === true && !json.msg) {
                        showToast('\u64cd\u4f5c\u6210\u529f');
                    }
                } catch(e) {}
                return resp;
            }).catch(function() { return resp; });
        });
    };

    return 'installed';
})();
)JS");
            m_webView->page()->runJavaScript(feedbackShim, [](const QVariant &r){
                qDebug() << "[WorkbenchPage] API反馈shim注入结果:" << r.toString();
            });
        }

        // 触摸拖拽诊断：仅在 RK3568_WEB_DEBUG=1 时注入，定位 Chromium 是否收到 touch 事件
        if (ok && workbenchWebDebugEnabled() && m_webView && m_webView->page()) {
            static const QString diag = QStringLiteral(R"JS(
(function(){
    if (window.__rk_touch_diag_installed) return 'already-installed';
    window.__rk_touch_diag_installed = true;

    var counters = { touchstart:0, touchmove:0, touchend:0,
                     mousedown:0, mousemove:0, mouseup:0,
                     pointerdown:0, pointermove:0, pointerup:0 };
    var dragDumped = false;

    function log(tag, e) {
        var t = (e.touches && e.touches[0]) || e;
        var path = (e.composedPath && e.composedPath()[0]) || e.target;
        var tag2 = path && path.tagName ? path.tagName : '?';
        console.log('[RK_TOUCH]', tag,
                    'x=' + (t.clientX|0), 'y=' + (t.clientY|0),
                    'buttons=' + e.buttons,
                    'target=' + tag2,
                    'cancelable=' + e.cancelable,
                    'defaultPrevented=' + e.defaultPrevented);
    }

    // 找出第一个可滚动的祖先
    window.__rk_dump_scrollable = function(x, y){
        var el = document.elementFromPoint(x, y);
        var depth = 0;
        while (el && depth < 20) {
            var cs = getComputedStyle(el);
            var oy = cs.overflowY, ox = cs.overflowX;
            var canScroll = (oy === 'auto' || oy === 'scroll' || ox === 'auto' || ox === 'scroll');
            console.log('[RK_SCROLL] ancestor#' + depth,
                        el.tagName + (el.id ? '#' + el.id : '') +
                        (el.className ? '.' + String(el.className).replace(/\s+/g,'.').slice(0,40) : ''),
                        'overflowY=' + oy, 'scrollH=' + el.scrollHeight, 'clientH=' + el.clientHeight,
                        canScroll && el.scrollHeight > el.clientHeight ? '<<< 可滚动' : '');
            el = el.parentElement;
            depth++;
        }
    };

    ['touchstart','touchmove','touchend',
     'mousedown','mousemove','mouseup',
     'pointerdown','pointermove','pointerup'].forEach(function(name){
        document.addEventListener(name, function(e){
            counters[name]++;
            if (counters[name] <= 3 || counters[name] % 30 === 0) {
                log(name + '#' + counters[name], e);
            }
            // [测试A] pointermove 第 10 次时自动扫描祖先链（此时大概率正在拖拽）
            if (name === 'pointermove' && counters[name] === 10 && !dragDumped) {
                dragDumped = true;
                console.log('[RK_SCROLL] === 拖拽自动触发祖先扫描 x=' + (e.clientX|0) + ' y=' + (e.clientY|0) + ' ===');
                window.__rk_dump_scrollable(e.clientX, e.clientY);
            }
        }, { capture: true, passive: true });
    });

    // [测试B] 页面加载 1 秒后，测试 scrollTop 赋值是否对各容器有效
    setTimeout(function(){
        var cx = window.innerWidth / 2, cy = window.innerHeight / 2;
        // 找屏幕中心点最近的可滚动祖先
        var el = document.elementFromPoint(cx, cy);
        var scrollTarget = null;
        while (el) {
            var oy = getComputedStyle(el).overflowY;
            if ((oy === 'auto' || oy === 'scroll') && el.scrollHeight > el.clientHeight) {
                scrollTarget = el;
                break;
            }
            el = el.parentElement;
        }

        var targets = [
            { name: 'document.scrollingElement', el: document.scrollingElement },
            { name: 'document.body',             el: document.body },
            { name: 'center最近可滚动div',        el: scrollTarget }
        ];

        targets.forEach(function(t){
            if (!t.el) {
                console.log('[RK_SCROLL] ' + t.name + ' = null，跳过');
                return;
            }
            var before = t.el.scrollTop;
            t.el.scrollTop = before + 80;
            var after = t.el.scrollTop;
            console.log('[RK_SCROLL] scrollTop测试 ' + t.name +
                        ' before=' + before + ' after=' + after +
                        (after !== before ? '  ✓ 有效，shim 可用此节点' : '  ✗ 无效'));
            setTimeout(function(){ t.el.scrollTop = before; }, 400);
        });
    }, 1000);

    console.log('[RK_TOUCH] 诊断v2已安装。拖拽触发祖先扫描(测试A)，1秒后自动scrollTop测试(测试B)。');
    return 'installed';
})();
)JS");
            m_webView->page()->runJavaScript(diag, [](const QVariant &r){
                qDebug() << "[WorkbenchPage] 触摸诊断注入结果:" << r.toString();
            });
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
    // 每次切换到工作台时，强制同步 Chromium 视口与容器实际尺寸，防止视口高度算错导致底部按钮被遮挡
    if (m_webView && ui->webViewContainer) {
        m_webView->resize(ui->webViewContainer->size());
    }
    // 新登录会话：token 变化时重置熔断器，允许重新加载
    if (m_controller->shouldLoadNow()) {
        m_reloadAttemptCount = 0;
        m_renderProcessTerminated = false;
        loadWorkbench();
        return;
    }
    if (m_renderProcessTerminated) {
        qWarning() << "[WorkbenchPage] 页面重新显示时检测到渲染异常，准备重新加载工作台";
        scheduleWorkbenchReload();
        return;
    }
    if (m_pendingBusinessReload) {
        m_pendingBusinessReload = false;
        loadWorkbench();
        return;
    }

    if (m_webView) {
        m_webView->show();
        m_webView->update();
    }
}

void WorkbenchPage::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    // 窗口几何修正（fitWindowToAvailableGeometry 多次 pass）期间同步 Chromium 视口
    if (m_webView && ui->webViewContainer) {
        m_webView->resize(ui->webViewContainer->size());
    }
}

void WorkbenchPage::reloadWorkbench()
{
    // 业务刷新（回传完成/登录切换等）：重置熔断器，允许全新加载
    m_reloadAttemptCount = 0;
    m_renderProcessTerminated = false;
    if (isVisible()) {
        loadWorkbench();
    } else {
        m_pendingBusinessReload = true;
    }
}

void WorkbenchPage::scheduleWorkbenchReload()
{
    if (!m_webView || m_reloadScheduled) {
        return;
    }

    constexpr int kMaxReloadAttempts = 5;
    if (m_reloadAttemptCount >= kMaxReloadAttempts) {
        qWarning() << "[WorkbenchPage] 工作台渲染恢复已达最大重试次数"
                   << kMaxReloadAttempts << "次，停止自动恢复，需人工重启设备";
        return;
    }

    if (m_reloadCooldownTimer.isValid() && m_reloadCooldownTimer.elapsed() < 1200) {
        qWarning() << "[WorkbenchPage] 工作台恢复重载已在冷却中，跳过本次重复触发";
        return;
    }

    m_reloadAttemptCount++;

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
        qWarning() << "[WorkbenchPage] 尝试重新加载工作台，以恢复白屏/渲染异常"
                   << "attempt=" << m_reloadAttemptCount;
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
