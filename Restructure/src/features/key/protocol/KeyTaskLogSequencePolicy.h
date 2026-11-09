#ifndef KEYTASKLOGSEQUENCEPOLICY_H
#define KEYTASKLOGSEQUENCEPOLICY_H

#include <QtGlobal>

namespace KeyTaskLogSequencePolicy {

enum class StartAction {
    Accept = 0,
    RejectInvalidResponse,
    IgnoreDuplicate,
    IgnoreWithoutRequest
};

enum class FrameAction {
    IgnoreWithoutContext = 0,
    RejectShortPayload,
    RejectUnexpectedSequence,
    BufferMore,
    BufferComplete
};

struct FrameObservation {
    bool contextActive = false;
    int dataLength = 0;
    quint16 frameSequence = 0;
    quint16 expectedSequence = 0;
    quint16 totalFrames = 0;
};

/**
 * Classify an I_TASK_LOG response without changing timers or receive state.
 */
StartAction classifyStart(bool requestActive,
                          bool uploadAlreadyPending,
                          int dataLength,
                          quint16 totalFrames);

/**
 * Classify one UP_TASK_LOG frame from its current receive context.
 */
FrameAction classifyFrame(const FrameObservation &observation);

} // namespace KeyTaskLogSequencePolicy

#endif // KEYTASKLOGSEQUENCEPOLICY_H
