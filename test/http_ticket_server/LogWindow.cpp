#include "LogWindow.h"
#include "ReplayPanel.h"
#include "GoldenPanel.h"

#include <QPlainTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFont>
#include <QDebug>

// ────────────────────────────────────────────────────────────
LogWindow::LogWindow(QWidget *parent)
    : QWidget(parent)
    , m_requestCount(0)
{
    setWindowTitle(QString::fromUtf8("HTTP 票据服务端"));
    resize(960, 660);

    // ──────────────────────────────────────────────
    //  Tab 1: HTTP 日志页
    // ──────────────────────────────────────────────
    QWidget *logTab = new QWidget(this);

    m_statusLabel = new QLabel(logTab);
    m_statusLabel->setStyleSheet("color: green; font-weight: bold; padding: 4px;");
    m_statusLabel->setText(QString::fromUtf8("正在启动服务…"));

    m_logView = new QPlainTextEdit(logTab);
    m_logView->setReadOnly(true);
    m_logView->setMaximumBlockCount(5000);
    {
        QFont mono("Consolas", 9);
        mono.setStyleHint(QFont::Monospace);
        m_logView->setFont(mono);
    }
    m_logView->setLineWrapMode(QPlainTextEdit::NoWrap);

    m_btnClear = new QPushButton(QString::fromUtf8("清空日志"), logTab);
    QLabel *countLabel = new QLabel(logTab);
    countLabel->setObjectName("countLabel");

    QHBoxLayout *bottomBar = new QHBoxLayout;
    bottomBar->addWidget(countLabel);
    bottomBar->addStretch();
    bottomBar->addWidget(m_btnClear);

    QVBoxLayout *logLayout = new QVBoxLayout(logTab);
    logLayout->setContentsMargins(6, 6, 6, 6);
    logLayout->addWidget(m_statusLabel);
    logLayout->addWidget(m_logView, 1);
    logLayout->addLayout(bottomBar);

    // ──────────────────────────────────────────────
    //  Tab 2: Replay / Diff 页
    // ──────────────────────────────────────────────
    m_replayPanel = new ReplayPanel(this);

    // ──────────────────────────────────────────────
    //  Tab 3: Golden 工具页
    // ──────────────────────────────────────────────
    m_goldenPanel = new GoldenPanel(this);

    // ──────────────────────────────────────────────
    //  QTabWidget
    // ──────────────────────────────────────────────
    m_tabs = new QTabWidget(this);
    m_tabs->addTab(logTab,         QString::fromUtf8("📡  HTTP 日志"));
    m_tabs->addTab(m_replayPanel,  QString::fromUtf8("🔬  Replay / Diff"));
    m_tabs->addTab(m_goldenPanel,  QString::fromUtf8("🛠  Golden 工具"));

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->addWidget(m_tabs);

    connect(m_btnClear, &QPushButton::clicked,
            this, &LogWindow::onClearClicked);
}

// ────────────────────────────────────────────────────────────
void LogWindow::initReplayPanel()
{
    if (m_replayPanel)
        m_replayPanel->autoPopulateDefaults();
}

// ────────────────────────────────────────────────────────────
void LogWindow::appendLog(const QString &text)
{
    ++m_requestCount;

    m_logView->appendPlainText(text);

    QTextCursor cursor = m_logView->textCursor();
    cursor.movePosition(QTextCursor::End);
    m_logView->setTextCursor(cursor);

    QLabel *countLabel = findChild<QLabel *>("countLabel");
    if (countLabel)
        countLabel->setText(
            QString::fromUtf8("已收到 %1 条请求").arg(m_requestCount));

    qInfo("%s", qPrintable(text));
}

// ────────────────────────────────────────────────────────────
void LogWindow::setStatusText(const QString &text)
{
    m_statusLabel->setText(text);
}

// ────────────────────────────────────────────────────────────
void LogWindow::onHttpJsonSaved(const QString &filePath)
{
    if (m_replayPanel)
        m_replayPanel->onLatestHttpJsonSaved(filePath);
}

// ────────────────────────────────────────────────────────────
void LogWindow::onClearClicked()
{
    m_logView->clear();
    m_requestCount = 0;
    QLabel *countLabel = findChild<QLabel *>("countLabel");
    if (countLabel) countLabel->clear();
}
