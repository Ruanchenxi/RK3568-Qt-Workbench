#ifndef PROGRESSIVERETURNOPERATIONCONTEXT_H
#define PROGRESSIVERETURNOPERATIONCONTEXT_H

#include <QByteArray>
#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QSet>
#include <QString>

// Holds only the short-lived data for progressive step return. Uploaded-step
// observations remain in memory for the process lifetime and are never persisted.
struct ProgressiveReturnOperationContext
{
    bool active = false;
    int sweepSessionId = 0;
    int totalSteps = 0;
    int stationNo = 0;
    QSet<int> observedCompletedSteps;
    QList<QJsonObject> pendingItems;
    QList<QByteArray> candidateTaskIds;
    QHash<QString, QSet<int>> uploadedSteps;
    QString httpTaskId;
    int httpSerialNumber = -1;

    bool hasHttpUpload() const
    {
        return !httpTaskId.isEmpty();
    }

    // Starts one task after the sweep has selected and validated it.
    void beginTask(int newTotalSteps)
    {
        active = true;
        totalSteps = newTotalSteps;
        stationNo = 0;
        observedCompletedSteps.clear();
        pendingItems.clear();
        clearHttpUpload();
    }

    // Ends correlation for one HTTP step without clearing other queued steps.
    void clearHttpUpload()
    {
        httpTaskId.clear();
        httpSerialNumber = -1;
    }

    // Stops candidate scanning when its physical-key session is no longer valid.
    void clearSweep()
    {
        candidateTaskIds.clear();
        sweepSessionId = 0;
    }

    // Clears one active progressive task. Uploaded-step evidence is retained
    // until the owning task is removed after verified key cleanup.
    void clearTask(bool clearCandidates)
    {
        active = false;
        totalSteps = 0;
        stationNo = 0;
        observedCompletedSteps.clear();
        pendingItems.clear();
        clearHttpUpload();
        if (clearCandidates) {
            clearSweep();
        }
    }
};

#endif // PROGRESSIVERETURNOPERATIONCONTEXT_H
