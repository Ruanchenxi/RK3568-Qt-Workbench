/**
 * @file logpage.cpp
 * @brief 服务日志页面（仅负责日志渲染）
 */

#include "logpage.h"
#include "ui_logpage.h"

#include "features/log/application/LogController.h"
#include "core/ConfigManager.h"

#include <QPlainTextEdit>
#include <QShowEvent>
#include <QTextCursor>

LogPage::LogPage(QWidget *parent)
    : QWidget(parent),
      ui(new Ui::LogPage),
      m_logDisplay(nullptr),
      m_controller(new LogController(nullptr, this)),
      m_started(false),
      m_visibleOnce(false)
{
    ui->setupUi(this);
    initServiceLog();

    connect(m_controller, &LogController::logGenerated,
            this, &LogPage::onServiceLogGenerated);

    bool startImmediately = true;
#ifdef Q_OS_LINUX
    const QString startMode = ConfigManager::instance()
            ->value("service/startMode", QStringLiteral("remote"))
            .toString()
            .trimmed()
            .toLower();
    if (startMode == QLatin1String("remote")) {
        startImmediately = false;
    }
#endif
    if (startImmediately) {
        ensureStarted();
    }
}

LogPage::~LogPage()
{
    if (m_controller && m_started)
    {
        m_controller->stop();
    }
    delete ui;
}

void LogPage::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    m_visibleOnce = true;
    ensureStarted();
    flushPendingLogs();
}

void LogPage::ensureStarted()
{
    if (m_started || !m_controller) {
        return;
    }

    m_controller->start();
    m_started = true;
}

void LogPage::flushPendingLogs()
{
    if (!m_logDisplay || !isVisible() || m_pendingLogs.isEmpty()) {
        return;
    }

    m_logDisplay->setUpdatesEnabled(false);
    for (const QString &text : qAsConst(m_pendingLogs)) {
        m_logDisplay->appendPlainText(text);
    }
    m_logDisplay->setUpdatesEnabled(true);
    m_pendingLogs.clear();

    QTextCursor cursor = m_logDisplay->textCursor();
    cursor.movePosition(QTextCursor::End);
    m_logDisplay->setTextCursor(cursor);
}

void LogPage::initServiceLog()
{
    m_logDisplay = ui->logDisplay;
    if (!m_logDisplay) {
        return;
    }

    m_logDisplay->setMaximumBlockCount(5000);
}

void LogPage::onServiceLogGenerated(const QString &text)
{
    appendLog(text);
}

void LogPage::appendLog(const QString &text)
{
    if (!m_logDisplay)
    {
        return;
    }

    if (!isVisible()) {
        m_pendingLogs.append(text);
        return;
    }

    m_logDisplay->appendPlainText(text);
    QTextCursor cursor = m_logDisplay->textCursor();
    cursor.movePosition(QTextCursor::End);
    m_logDisplay->setTextCursor(cursor);
}
