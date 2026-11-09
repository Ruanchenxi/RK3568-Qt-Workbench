#include "platform/runtime/ApplicationSessionTracker.h"

#include "platform/logging/LogService.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>

ApplicationSessionTracker::ApplicationSessionTracker(QObject *parent)
    : QObject(parent), m_started(false)
{
}

void ApplicationSessionTracker::start()
{
    if (m_started)
        return;
    m_started = true;
    QFile previous(markerPath());
    if (previous.exists()) {
        QString startedAt = QStringLiteral("unknown");
        if (previous.open(QIODevice::ReadOnly))
            startedAt = QJsonDocument::fromJson(previous.readAll()).object()
                    .value(QStringLiteral("startedAt")).toString(startedAt);
        LOG_INCIDENT(QStringLiteral("APP"),
                     QStringLiteral("previous session did not exit cleanly; startedAt=%1").arg(startedAt));
    }

    QDir().mkpath(QFileInfo(markerPath()).absolutePath());
    QFile marker(markerPath());
    if (marker.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QJsonObject value;
        value.insert(QStringLiteral("startedAt"), QDateTime::currentDateTime().toString(Qt::ISODateWithMs));
        value.insert(QStringLiteral("pid"), static_cast<qint64>(QCoreApplication::applicationPid()));
        marker.write(QJsonDocument(value).toJson(QJsonDocument::Compact));
    }
    LOG_INFO(QStringLiteral("APP"), QStringLiteral("application session started"));
}

void ApplicationSessionTracker::markCleanExit()
{
    if (!m_started)
        return;
    LOG_INFO(QStringLiteral("APP"), QStringLiteral("application clean exit requested"));
    QFile::remove(markerPath());
    m_started = false;
}

QString ApplicationSessionTracker::markerPath() const
{
    return QDir(QCoreApplication::applicationDirPath())
            .filePath(QStringLiteral("logs/runtime-session.json"));
}
