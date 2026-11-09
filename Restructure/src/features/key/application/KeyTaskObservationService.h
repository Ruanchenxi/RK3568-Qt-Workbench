#ifndef KEYTASKOBSERVATIONSERVICE_H
#define KEYTASKOBSERVATIONSERVICE_H

#include <QObject>
#include <QList>

#include "shared/contracts/KeyTaskDto.h"

// Owns the process-local Q_TASK snapshot and its physical-key-session proof.
// It never starts a ticket workflow or sends a device/network command.
class KeyTaskObservationService : public QObject
{
public:
    /// Creates an empty process-local observation catalog.
    explicit KeyTaskObservationService(QObject *parent = nullptr);

    /// Seeds ready-edge detection without treating application startup as insertion.
    void initializeReadyState(bool ready);
    bool wasReady() const;

    /// Invalidates all proof when a new physical-key session begins.
    void beginSession();
    /// Removes both the task snapshot and proof when the physical key is lost.
    void invalidateSession();

    /// Invalidates the previous proof before issuing a new Q_TASK command.
    void beginQuery();
    /// Records a Q_TASK result; only a ready session creates admissible proof.
    void acceptObservation(const QList<KeyTaskDto> &tasks,
                           int keySessionId,
                           bool sessionReady,
                           qint64 observedAtMs);

    /// Acquires the one-shot automatic-query window on a ready edge.
    bool beginReadyEdgeQuery(bool ready,
                             bool transferBusy,
                             int keySessionId);
    /// Acquires an automatic-query window for explicit reconciliation work.
    bool beginAutomaticQuery(int keySessionId);
    /// Releases the automatic-query window after response, timeout, or NAK.
    void releaseAutomaticQuery(int keySessionId);
    bool automaticQueryActive(int keySessionId) const;

    /// Returns the latest diagnostic snapshot, whether authoritative or not.
    QList<KeyTaskDto> tasks() const;
    /// Returns the timestamp of the latest admissible same-session Q_TASK.
    qint64 observedAtMs() const;
    /// Returns the physical-key session that owns the admissible proof.
    int observedSessionId() const;
    /// Reports whether the latest diagnostic snapshot contains any task.
    bool hasTasks() const;
    /// Finds a task using the workbench decimal identifier.
    bool containsTaskId(const QString &taskId) const;
    /// Matches the protocol identifier without string conversion.
    bool containsRawTask(const QByteArray &taskIdRaw) const;
    /// Returns the current raw identity and optional hardware task status.
    QByteArray findTaskIdRaw(const QString &taskId,
                             quint8 *status = nullptr) const;

    /// A selected row is actionable only when it came from this key session.
    bool hasFreshSelectedTask(const QByteArray &taskIdRaw,
                              int keySessionId) const;
    /// Presence proof is fresh only when confirmation is not older than Q_TASK.
    bool isPresenceProofFresh(qint64 keyConfirmedAtMs,
                              int keySessionId) const;

    /// Converts the protocol little-endian task id to its backend decimal form.
    static QString taskIdFromRaw(const QByteArray &taskIdRaw);

private:
    QList<KeyTaskDto> m_tasks;
    qint64 m_observedAtMs = 0;
    int m_observedSessionId = 0;
    int m_automaticQuerySessionId = 0;
    bool m_ready = false;
};

#endif // KEYTASKOBSERVATIONSERVICE_H
