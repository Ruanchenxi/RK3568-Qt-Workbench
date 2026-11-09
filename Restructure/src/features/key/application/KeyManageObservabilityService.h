#ifndef KEYMANAGEOBSERVABILITYSERVICE_H
#define KEYMANAGEOBSERVABILITYSERVICE_H

#include <QObject>
#include <QString>
#include <QStringList>

#include "platform/logging/BusinessLogService.h"

// Owns bounded key-page text logs and the fixed-schema key business log.
// It observes workflow results only; it never decides or executes a workflow.
class KeyManageObservabilityService : public QObject
{
public:
    explicit KeyManageObservabilityService(QObject *parent = nullptr,
                                           int textLogCapacity = 2000);

    void appendHttpClientLog(const QString &text);
    void appendHttpServerLog(const QString &text);
    void clearHttpClientLogs();
    void clearHttpServerLogs();
    QString httpClientLogText() const;
    QString httpServerLogText() const;
    // Returns implicitly shared snapshots for background export formatting.
    QStringList httpClientLogLines() const;
    QStringList httpServerLogLines() const;

    // Records only workflow identity and outcome, never ticket bodies or tokens.
    void writeBusinessEvent(
            const QString &flow,
            const QString &phase,
            const QString &result,
            const QString &taskId,
            int keySessionId,
            qint64 opId = -1,
            const QString &reason = QString(),
            BusinessLogSeverity severity = BusinessLogSeverity::Info) const;

private:
    void appendBounded(QStringList *lines, const QString &text);

    int m_textLogCapacity;
    QStringList m_httpClientLogLines;
    QStringList m_httpServerLogLines;
};

#endif // KEYMANAGEOBSERVABILITYSERVICE_H
