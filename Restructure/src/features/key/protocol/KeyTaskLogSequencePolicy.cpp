#include "KeyTaskLogSequencePolicy.h"

namespace KeyTaskLogSequencePolicy {

StartAction classifyStart(bool requestActive,
                          bool uploadAlreadyPending,
                          int dataLength,
                          quint16 totalFrames)
{
    if (!requestActive) {
        return uploadAlreadyPending
                ? StartAction::IgnoreDuplicate
                : StartAction::IgnoreWithoutRequest;
    }
    if (dataLength < 2 || totalFrames == 0) {
        return StartAction::RejectInvalidResponse;
    }
    return StartAction::Accept;
}

FrameAction classifyFrame(const FrameObservation &observation)
{
    if (!observation.contextActive) {
        return FrameAction::IgnoreWithoutContext;
    }
    if (observation.dataLength < 2) {
        return FrameAction::RejectShortPayload;
    }
    if (observation.frameSequence != observation.expectedSequence) {
        return FrameAction::RejectUnexpectedSequence;
    }
    return observation.expectedSequence + 1 < observation.totalFrames
            ? FrameAction::BufferMore
            : FrameAction::BufferComplete;
}

} // namespace KeyTaskLogSequencePolicy
