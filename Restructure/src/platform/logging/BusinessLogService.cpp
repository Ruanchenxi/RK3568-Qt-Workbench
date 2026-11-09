#include "platform/logging/BusinessLogService.h"

#include <QJsonDocument>
#include <QJsonObject>

#include "platform/logging/LogService.h"

QString BusinessLogService::format(const BusinessLogEvent &event)
{
    QJsonObject fields;
    fields.insert(QStringLiteral("flow"), event.flow.trimmed());
    fields.insert(QStringLiteral("phase"), event.phase.trimmed());
    fields.insert(QStringLiteral("result"), event.result.trimmed());

    if (!event.taskId.trimmed().isEmpty()) {
        fields.insert(QStringLiteral("taskId"), event.taskId.trimmed());
    }
    if (!event.slot.trimmed().isEmpty()) {
        fields.insert(QStringLiteral("slot"), event.slot.trimmed());
    }
    if (event.keySessionId >= 0) {
        fields.insert(QStringLiteral("keySessionId"), QString::number(event.keySessionId));
    }
    if (event.opId >= 0) {
        fields.insert(QStringLiteral("opId"), QString::number(event.opId));
    }
    if (event.durationMs >= 0) {
        fields.insert(QStringLiteral("durationMs"), QString::number(event.durationMs));
    }
    if (!event.reason.trimmed().isEmpty()) {
        fields.insert(QStringLiteral("reason"), event.reason.trimmed());
    }

    return QString::fromUtf8(QJsonDocument(fields).toJson(QJsonDocument::Compact));
}

void BusinessLogService::write(const BusinessLogEvent &event,
                               BusinessLogSeverity severity)
{
    const QString message = format(event);
    switch (severity) {
    case BusinessLogSeverity::Info:
        LogService::instance()->info(QStringLiteral("BUSINESS"), message);
        break;
    case BusinessLogSeverity::Warning:
        LogService::instance()->warning(QStringLiteral("BUSINESS"), message);
        break;
    case BusinessLogSeverity::Error:
        LogService::instance()->error(QStringLiteral("BUSINESS"), message);
        break;
    }
}
