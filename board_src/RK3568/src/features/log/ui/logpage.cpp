/**
 * @file logpage.cpp
 * @brief 服务日志页面（静态骨架在 .ui，动态逻辑在 .cpp）
 */

#include "logpage.h"
#include "ui_logpage.h"

#include "features/log/application/LogController.h"
#include "core/ConfigManager.h"

#include <QShowEvent>
#include <QPlainTextEdit>
#include <QTextCursor>
#include <QTextOption>

namespace
{
void scrollToEnd(QPlainTextEdit *logDisplay)
{
    if (!logDisplay) {
        return;
    }

    QTextCursor cursor = logDisplay->textCursor();
    cursor.movePosition(QTextCursor::End);
    logDisplay->setTextCursor(cursor);
}
}

LogPage::LogPage(QWidget *parent)
    : QWidget(parent),
      ui(new Ui::LogPage),
      m_controller(new LogController(nullptr, this)),
      m_started(false)
{
    ui->setupUi(this);
    ui->logDisplay->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    ui->logDisplay->setWordWrapMode(QTextOption::WrapAnywhere);

    connect(m_controller, &LogController::logGenerated,
            this, &LogPage::onServiceLogGenerated);

    if (shouldStartControllerOnConstruction()) {
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
    ensureStarted();
    flushBufferedLogs();
}

bool LogPage::shouldStartControllerOnConstruction() const
{
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
    return startImmediately;
}

void LogPage::ensureStarted()
{
    if (m_started || !m_controller) {
        return;
    }

    m_controller->start();
    m_started = true;
}

void LogPage::flushBufferedLogs()
{
    if (!ui || !ui->logDisplay || !isVisible() || m_pendingLogs.isEmpty()) {
        return;
    }

    ui->logDisplay->setUpdatesEnabled(false);
    for (const QString &text : qAsConst(m_pendingLogs)) {
        ui->logDisplay->appendPlainText(text);
    }
    ui->logDisplay->setUpdatesEnabled(true);
    m_pendingLogs.clear();

    scrollToEnd(ui->logDisplay);
}

void LogPage::onServiceLogGenerated(const QString &text)
{
    appendOrBufferLog(text);
}

void LogPage::appendOrBufferLog(const QString &text)
{
    if (!ui || !ui->logDisplay)
    {
        return;
    }

    if (!isVisible()) {
        m_pendingLogs.append(text);
        return;
    }

    ui->logDisplay->appendPlainText(text);
    scrollToEnd(ui->logDisplay);
}
