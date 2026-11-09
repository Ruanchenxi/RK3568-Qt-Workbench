/** Structured serial diagnostic entry shared across the key feature. */
#ifndef LOGITEM_H
#define LOGITEM_H
#include <QString>
#include <QByteArray>
#include <QDateTime>
#include <QtGlobal>
enum class LogDir { TX, RX, RAW, EVENT, WARN, ERROR };
static constexpr quint8 LogCmdNone = 0xFF;
struct LogItem {
    QDateTime timestamp;
    LogDir dir;
    quint8 cmd;
    quint16 length;
    QString summary;
    QByteArray hex;
    bool crcOk;
    bool expertOnly;
    int opId;
    LogItem() : dir(LogDir::TX), cmd(LogCmdNone), length(0), crcOk(true), expertOnly(false), opId(-1) {}
};
#endif
