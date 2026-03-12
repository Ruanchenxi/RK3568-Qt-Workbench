/**
 * @file logpage.cpp
 * @brief 服务日志页面（仅负责日志渲染）
 */

#include "logpage.h"
#include "ui_logpage.h"

#include "features/log/application/LogController.h"

#include <QPlainTextEdit>
#include <QTextCursor>

LogPage::LogPage(QWidget *parent)
    : QWidget(parent),
      ui(new Ui::LogPage),
      m_logDisplay(nullptr),
      m_controller(new LogController(nullptr, this))
{
    ui->setupUi(this);
    initServiceLog();

    connect(m_controller, &LogController::logGenerated,
            this, &LogPage::onServiceLogGenerated);

    m_controller->start();
}

LogPage::~LogPage()
{
    if (m_controller)
    {
        m_controller->stop();
    }
    delete ui;
}

void LogPage::initServiceLog()
{
    m_logDisplay = new QPlainTextEdit(this);
    m_logDisplay->setReadOnly(true);
    m_logDisplay->setMaximumBlockCount(5000);

    m_logDisplay->setStyleSheet(
        "QPlainTextEdit {"
        "  background-color: #1E1E1E;"
        "  color: #00FF00;"
        "  border: 1px solid #333333;"
        "  border-radius: 6px;"
        "  padding: 10px;"
        "  font-family: 'Consolas', 'Courier New', monospace;"
        "  font-size: 12px;"
        "  line-height: 1.4;"
        "}"
        "QPlainTextEdit QScrollBar:vertical {"
        "  background-color: #2A2A2A;"
        "  width: 10px;"
        "}"
        "QPlainTextEdit QScrollBar::handle:vertical {"
        "  background-color: #555555;"
        "  border-radius: 5px;"
        "}"
        "QPlainTextEdit QScrollBar::handle:vertical:hover {"
        "  background-color: #777777;"
        "}");

    if (ui->layoutLog)
    {
        ui->layoutLog->addWidget(m_logDisplay);
    }
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

    m_logDisplay->appendPlainText(text);
    QTextCursor cursor = m_logDisplay->textCursor();
    cursor.movePosition(QTextCursor::End);
    m_logDisplay->setTextCursor(cursor);
}
