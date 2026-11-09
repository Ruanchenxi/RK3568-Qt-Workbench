#ifndef BUSINESSLOGSERVICE_H
#define BUSINESSLOGSERVICE_H

#include <QString>

enum class BusinessLogSeverity
{
    Info,
    Warning,
    Error
};

struct BusinessLogEvent
{
    QString flow;
    QString phase;
    QString result;
    QString taskId;
    QString slot;
    QString reason;
    qint64 keySessionId = -1;
    qint64 opId = -1;
    qint64 durationMs = -1;
};

class BusinessLogService
{
public:
    // Writes one compact event to the existing rotated application log. The
    // fixed schema keeps production incidents searchable by taskId and phase.
    static void write(const BusinessLogEvent &event,
                      BusinessLogSeverity severity = BusinessLogSeverity::Info);

    static QString format(const BusinessLogEvent &event);
};

#endif // BUSINESSLOGSERVICE_H
