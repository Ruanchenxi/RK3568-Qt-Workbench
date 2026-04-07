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
      m_controller(new LogController(processService, this))
{
    ui->setupUi(this);
    ui->logDisplay->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    ui->logDisplay->setWordWrapMode(QTextOption::WrapAnywhere);

    connect(m_controller, &LogController::logGenerated,
            this, &LogPage::onServiceLogGenerated);
}

LogPage::~LogPage()
{
    delete ui;
}

void LogPage::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    flushBufferedLogs();
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
