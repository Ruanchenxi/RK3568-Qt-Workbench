#include "features/key/application/KeyManageObservabilityService.h"

KeyManageObservabilityService::KeyManageObservabilityService(
        QObject *parent,
        int textLogCapacity)
    : QObject(parent)
    , m_textLogCapacity(qMax(1, textLogCapacity))
{
}

void KeyManageObservabilityService::appendHttpClientLog(const QString &text)
{
    appendBounded(&m_httpClientLogLines, text);
}

void KeyManageObservabilityService::appendHttpServerLog(const QString &text)
{
    appendBounded(&m_httpServerLogLines, text);
}

void KeyManageObservabilityService::clearHttpClientLogs()
{
    m_httpClientLogLines.clear();
}

void KeyManageObservabilityService::clearHttpServerLogs()
{
    m_httpServerLogLines.clear();
}

QString KeyManageObservabilityService::httpClientLogText() const
{
    return m_httpClientLogLines.join(QLatin1Char('\n'));
}

QString KeyManageObservabilityService::httpServerLogText() const
{
    return m_httpServerLogLines.join(QLatin1Char('\n'));
}

QStringList KeyManageObservabilityService::httpClientLogLines() const
{
    return m_httpClientLogLines;
}

QStringList KeyManageObservabilityService::httpServerLogLines() const
{
    return m_httpServerLogLines;
}

void KeyManageObservabilityService::writeBusinessEvent(
        const QString &flow,
        const QString &phase,
        const QString &result,
        const QString &taskId,
        int keySessionId,
        qint64 opId,
        const QString &reason,
        BusinessLogSeverity severity) const
{
    BusinessLogEvent event;
    event.flow = flow;
    event.phase = phase;
    event.result = result;
    event.taskId = taskId;
    event.slot = QStringLiteral("A");
    event.keySessionId = keySessionId;
    event.opId = opId;
    event.reason = reason;
    BusinessLogService::write(event, severity);
}

void KeyManageObservabilityService::appendBounded(
        QStringList *lines,
        const QString &text)
{
    lines->append(text);
    while (lines->size() > m_textLogCapacity) {
        lines->removeFirst();
    }
}
