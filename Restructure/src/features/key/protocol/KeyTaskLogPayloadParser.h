#ifndef KEYTASKLOGPAYLOADPARSER_H
#define KEYTASKLOGPAYLOADPARSER_H

#include <QByteArray>
#include <QList>
#include <QString>

namespace KeyTaskLogPayloadParser {

enum class ParseError {
    None = 0,
    PayloadTooShort,
    InvalidFileSize,
    TaskIdMismatch,
    InvalidItemBytes
};

struct TaskLogItem {
    int serialNumber = 0;
    int opStatus = 0;
    QString rfid;
    QString opTime;
    int opType = 0;
};

struct TaskLogPayload {
    QString taskId;
    QByteArray taskIdRaw;
    int stationNo = 0;
    int taskAttr = 0;
    int steps = 0;
    QString version;
    QList<TaskLogItem> items;
};

struct ParseResult {
    ParseError error = ParseError::None;
    quint32 fileSize = 0;
    int itemBytes = 0;
    TaskLogPayload payload;

    bool isValid() const { return error == ParseError::None; }
};

/**
 * Decode the assembled UP_TASK_LOG file without logging, emitting events, or
 * changing request state. An expected task id, when supplied, is an exact
 * 16-byte correlation guard.
 */
ParseResult parse(const QByteArray &bytes, const QByteArray &expectedTaskId);

} // namespace KeyTaskLogPayloadParser

#endif // KEYTASKLOGPAYLOADPARSER_H
