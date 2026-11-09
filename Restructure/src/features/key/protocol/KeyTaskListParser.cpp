#include "KeyTaskListParser.h"

namespace {

constexpr int kTaskIdLength = 16;
constexpr int kTaskRecordLength = 20;

} // namespace

namespace KeyTaskListParser {

ParseResult parse(const QByteArray &payload)
{
    ParseResult result;
    if (payload.isEmpty()) {
        return result;
    }

    result.declaredCount = static_cast<quint8>(payload[0]);
    int offset = 1;
    for (int i = 0;
         i < result.declaredCount && offset + kTaskRecordLength <= payload.size();
         ++i) {
        KeyTaskInfo task;
        task.taskId = payload.mid(offset, kTaskIdLength);
        task.status = static_cast<quint8>(payload[offset + 16]);
        task.type = static_cast<quint8>(payload[offset + 17]);
        task.source = static_cast<quint8>(payload[offset + 18]);
        task.reserved = static_cast<quint8>(payload[offset + 19]);
        result.tasks.append(task);
        offset += kTaskRecordLength;
    }
    return result;
}

} // namespace KeyTaskListParser
