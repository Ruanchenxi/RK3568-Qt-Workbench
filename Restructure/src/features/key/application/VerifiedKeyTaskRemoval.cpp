#include "features/key/application/VerifiedKeyTaskRemoval.h"

KeyTaskRemovalProof VerifiedKeyTaskRemoval::evaluate(
        const KeyTaskRemovalObservation &observation)
{
    if (!observation.sameKeySession) {
        return KeyTaskRemovalProof::SessionChanged;
    }

    if (observation.phase == KeyTaskRemovalPhase::BeforeDelete) {
        return observation.taskPresent
                ? KeyTaskRemovalProof::ReadyToDelete
                : KeyTaskRemovalProof::TaskMissingBeforeDelete;
    }

    return observation.taskPresent
            ? KeyTaskRemovalProof::TaskStillPresent
            : KeyTaskRemovalProof::DeleteConfirmed;
}

bool VerifiedKeyTaskRemoval::containsRawTask(const QList<KeyTaskDto> &tasks,
                                             const QByteArray &taskIdRaw)
{
    for (const KeyTaskDto &task : tasks) {
        if (task.taskId == taskIdRaw) {
            return true;
        }
    }
    return false;
}

QByteArray VerifiedKeyTaskRemoval::findRawTask(const QList<KeyTaskDto> &tasks,
                                               const QString &decimalId,
                                               quint8 *status)
{
    const QString target = decimalId.trimmed();
    for (const KeyTaskDto &task : tasks) {
        if (decimalTaskId(task.taskId) == target) {
            if (status) {
                *status = task.status;
            }
            return task.taskId;
        }
    }
    if (status) {
        *status = 0xFF;
    }
    return QByteArray();
}

QString VerifiedKeyTaskRemoval::decimalTaskId(const QByteArray &taskIdRaw)
{
    if (taskIdRaw.size() < 8) {
        return QString();
    }

    quint64 value = 0;
    for (int i = 0; i < 8; ++i) {
        value |= (static_cast<quint64>(
                      static_cast<unsigned char>(taskIdRaw.at(i))) << (8 * i));
    }
    return QString::number(value);
}
