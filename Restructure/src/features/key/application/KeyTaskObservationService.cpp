#include "features/key/application/KeyTaskObservationService.h"

namespace
{
constexpr qint64 kObservationClockToleranceMs = 500;
}

KeyTaskObservationService::KeyTaskObservationService(QObject *parent)
    : QObject(parent)
{
}

void KeyTaskObservationService::initializeReadyState(bool ready)
{
    m_ready = ready;
}

bool KeyTaskObservationService::wasReady() const
{
    return m_ready;
}

void KeyTaskObservationService::beginSession()
{
    m_observedAtMs = 0;
    m_observedSessionId = 0;
    m_automaticQuerySessionId = 0;
}

void KeyTaskObservationService::invalidateSession()
{
    beginSession();
    m_tasks.clear();
}

void KeyTaskObservationService::beginQuery()
{
    m_observedAtMs = 0;
    m_observedSessionId = 0;
}

void KeyTaskObservationService::acceptObservation(
        const QList<KeyTaskDto> &tasks,
        int keySessionId,
        bool sessionReady,
        qint64 observedAtMs)
{
    m_tasks = tasks;
    if (sessionReady && keySessionId > 0 && observedAtMs > 0) {
        m_observedAtMs = observedAtMs;
        m_observedSessionId = keySessionId;
    } else {
        m_observedAtMs = 0;
        m_observedSessionId = 0;
    }
    releaseAutomaticQuery(keySessionId);
}

bool KeyTaskObservationService::beginReadyEdgeQuery(
        bool ready,
        bool transferBusy,
        int keySessionId)
{
    const bool becameReady = !m_ready && ready;
    m_ready = ready;
    if (!becameReady || transferBusy || keySessionId <= 0
            || automaticQueryActive(keySessionId)) {
        return false;
    }
    m_automaticQuerySessionId = keySessionId;
    return true;
}

bool KeyTaskObservationService::beginAutomaticQuery(int keySessionId)
{
    if (keySessionId <= 0 || automaticQueryActive(keySessionId)) {
        return false;
    }
    m_automaticQuerySessionId = keySessionId;
    return true;
}

void KeyTaskObservationService::releaseAutomaticQuery(int keySessionId)
{
    if (keySessionId > 0 && m_automaticQuerySessionId == keySessionId) {
        m_automaticQuerySessionId = 0;
    }
}

bool KeyTaskObservationService::automaticQueryActive(int keySessionId) const
{
    return keySessionId > 0 && m_automaticQuerySessionId == keySessionId;
}

QList<KeyTaskDto> KeyTaskObservationService::tasks() const
{
    return m_tasks;
}

qint64 KeyTaskObservationService::observedAtMs() const
{
    return m_observedAtMs;
}

int KeyTaskObservationService::observedSessionId() const
{
    return m_observedSessionId;
}

bool KeyTaskObservationService::hasTasks() const
{
    return !m_tasks.isEmpty();
}

bool KeyTaskObservationService::containsTaskId(const QString &taskId) const
{
    return !findTaskIdRaw(taskId).isEmpty();
}

bool KeyTaskObservationService::containsRawTask(
        const QByteArray &taskIdRaw) const
{
    if (taskIdRaw.isEmpty()) {
        return false;
    }
    for (const KeyTaskDto &task : m_tasks) {
        if (task.taskId == taskIdRaw) {
            return true;
        }
    }
    return false;
}

QByteArray KeyTaskObservationService::findTaskIdRaw(
        const QString &taskId,
        quint8 *status) const
{
    for (const KeyTaskDto &task : m_tasks) {
        if (taskIdFromRaw(task.taskId) == taskId) {
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

bool KeyTaskObservationService::hasFreshSelectedTask(
        const QByteArray &taskIdRaw,
        int keySessionId) const
{
    return keySessionId > 0
            && m_observedSessionId == keySessionId
            && m_observedAtMs > 0
            && containsRawTask(taskIdRaw);
}

bool KeyTaskObservationService::isPresenceProofFresh(
        qint64 keyConfirmedAtMs,
        int keySessionId) const
{
    return keySessionId > 0
            && m_observedSessionId == keySessionId
            && keyConfirmedAtMs > 0
            && m_observedAtMs > 0
            && keyConfirmedAtMs + kObservationClockToleranceMs
                    >= m_observedAtMs;
}

QString KeyTaskObservationService::taskIdFromRaw(
        const QByteArray &taskIdRaw)
{
    if (taskIdRaw.size() < 8) {
        return QString();
    }

    qulonglong value = 0;
    for (int i = 0; i < 8; ++i) {
        value |= static_cast<qulonglong>(
                    static_cast<quint8>(taskIdRaw.at(i))) << (i * 8);
    }
    return QString::number(value);
}
