#ifndef KEYREQUESTPOLICY_H
#define KEYREQUESTPOLICY_H

#include <QString>
#include <QtGlobal>

#include "KeyRequestType.h"

namespace KeyRequestPolicy {

enum class RetryAction {
    IgnoreStale = 0,
    Resend,
    FailAuxiliarySeatB,
    IgnoreStartupProbe,
    ScheduleQTaskRecovery,
    FinalTimeout
};

struct RetryObservation {
    bool requestActive = false;
    bool timerGenerationMatches = false;
    int retryCountAfterTimeout = 0;
    int maxRetries = 0;
    KeyRequestType requestType = KeyRequestType::None;
    bool auxiliarySeatB = false;
    bool startupQTaskProbe = false;
    bool keyPresent = false;
    int qtaskRecoveryRound = 0;
    int maxQTaskRecoveryRounds = 0;
};

struct CommandDescription {
    KeyRequestType requestType = KeyRequestType::None;
    quint8 expectedAckCommand = 0;
    int frameIndex = -1;
    int frameCount = 0;
};

/**
 * Match an ACK only to the request kind that owns it. Multi-frame transfers
 * use their exact expected command because their final and continuation bytes
 * differ.
 */
bool ackMatches(KeyRequestType requestType,
                quint8 expectedAckCommand,
                quint8 acknowledgedCommand);

/**
 * Classify one retry-timer firing without changing timers, transport, session,
 * or request state.
 */
RetryAction classifyRetry(const RetryObservation &observation);

/** Return the stable human-readable command label used by timeout logs. */
QString commandName(const CommandDescription &description);

/** Return the existing retry-summary wording for one request attempt. */
QString retrySummary(const CommandDescription &description,
                     int retryCount,
                     int maxRetries);

/** Return the existing receive-log wording for an ACK command byte. */
QString ackSummary(quint8 acknowledgedCommand);

} // namespace KeyRequestPolicy

#endif // KEYREQUESTPOLICY_H
