#ifndef VERIFIEDKEYTASKREMOVAL_H
#define VERIFIEDKEYTASKREMOVAL_H

#include <QByteArray>
#include <QList>
#include <QString>

#include "shared/contracts/KeyTaskDto.h"

enum class KeyTaskRemovalPhase
{
    BeforeDelete,
    AfterDelete
};

enum class KeyTaskRemovalProof
{
    SessionChanged,
    TaskMissingBeforeDelete,
    ReadyToDelete,
    DeleteConfirmed,
    TaskStillPresent
};

struct KeyTaskRemovalObservation
{
    KeyTaskRemovalPhase phase = KeyTaskRemovalPhase::BeforeDelete;
    bool sameKeySession = false;
    bool taskPresent = false;
};

class VerifiedKeyTaskRemoval
{
public:
    // Converts one fresh Q_TASK observation into a small proof result. Business
    // callers decide what to do after success; this class never updates tickets.
    static KeyTaskRemovalProof evaluate(const KeyTaskRemovalObservation &observation);

    static bool containsRawTask(const QList<KeyTaskDto> &tasks,
                                const QByteArray &taskIdRaw);

    // Finds the protocol task identifier in the Q_TASK response without making
    // cached controller state part of the deletion proof.
    static QByteArray findRawTask(const QList<KeyTaskDto> &tasks,
                                  const QString &decimalTaskId,
                                  quint8 *status = nullptr);

    // Converts the little-endian protocol task identifier into the backend's
    // decimal task id so every Q_TASK workflow uses one identity rule.
    static QString decimalTaskId(const QByteArray &taskIdRaw);
};

#endif // VERIFIEDKEYTASKREMOVAL_H
