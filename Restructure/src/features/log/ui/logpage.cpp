/**
 * @file logpage.cpp
 * @brief 服务日志页面（静态骨架在 .ui，动态逻辑在 .cpp）
 */

#include "logpage.h"
#include "ui_logpage.h"

#include "features/log/application/LogController.h"
#include "shared/contracts/IProcessService.h"

#include <QShowEvent>
#include <QPlainTextEdit>
#include <QTextCursor>
#include <QTextOption>
#include <QTimer>

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

LogPage::LogPage(IProcessService *processService, QWidget *parent)
    : QWidget(parent),
      ui(new Ui::LogPage),
      m_controller(new LogController(processService, this)),
      m_flushTimer(new QTimer(this))
{
    ui->setupUi(this);
    ui->logDisplay->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    ui->logDisplay->setWordWrapMode(QTextOption::WrapAnywhere);
    ui->logDisplay->setUndoRedoEnabled(false);
    ui->logDisplay->setMaximumBlockCount(1500);

    m_flushTimer->setInterval(150);
    m_flushTimer->setSingleShot(true);
    connect(m_flushTimer, &QTimer::timeout,
            this, &LogPage::flushBufferedLogs);

    connect(m_controller, &LogController::logGenerated,
            this, &LogPage::onServiceLogGenerated);
}

LogPage::~LogPage()
{
    delete ui;
}

void LogPage::applyLayoutProfile(const UiLayoutProfile &profile)
{
    const int margin = profile.mode == UiLayoutMode::Compact
            ? 8 : profile.contentHorizontalMargin;
    ui->contentLayout->setContentsMargins(margin, margin, margin, margin);
    ui->contentLayout->setSpacing(profile.mode == UiLayoutMode::Compact ? 6 : 10);
}

void LogPage::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    flushBufferedLogs();
}

void LogPage::flushBufferedLogs()
{
    if (!ui || !ui->logDisplay || !isVisible())
    {
        m_flushTimer->stop();
        return;
    }
    if (m_pendingLogs.isEmpty())
    {
        return;
    }

    // One document update per interval keeps log traffic off the hot UI path.
    const QString batch = m_pendingLogs.join(QLatin1Char('\n'));
    m_pendingLogs.clear();
    ui->logDisplay->appendPlainText(batch);
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

    m_pendingLogs.append(text);
    constexpr int kMaxPending = 300;
    while (m_pendingLogs.size() > kMaxPending)
    {
        m_pendingLogs.removeFirst();
    }

    if (isVisible() && !m_flushTimer->isActive())
    {
        m_flushTimer->start();
    }
}
