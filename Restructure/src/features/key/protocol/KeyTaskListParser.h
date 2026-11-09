#ifndef KEYTASKLISTPARSER_H
#define KEYTASKLISTPARSER_H

#include <QByteArray>
#include <QList>
#include <QtGlobal>

#include "KeyTaskInfo.h"

namespace KeyTaskListParser {

struct ParseResult {
    quint8 declaredCount = 0;
    QList<KeyTaskInfo> tasks;
};

/**
 * Parse the Q_TASK data field without applying session state, logging, or
 * deferred-command effects. Truncated trailing records remain ignored to
 * preserve the verified device-adapter behavior.
 */
ParseResult parse(const QByteArray &payload);

} // namespace KeyTaskListParser

#endif // KEYTASKLISTPARSER_H
