#ifndef KEYREQUESTTRACKER_H
#define KEYREQUESTTRACKER_H

#include <QByteArray>
#include <QtGlobal>

#include "KeyRequestType.h"

// Owns the process-local identity of one serial request. Timers and transport
// remain in KeySerialClient; this value object only preserves correlation data.
class KeyRequestTracker
{
public:
    void begin(KeyRequestType type,
               quint8 expectedAckCommand,
               const QByteArray &frame);
    void armRetryTimer();
    int recordTimeout();
    void clearRetry();
    void clearRequest();
    void markQTaskFrameHeaderSeen();

    bool active() const { return m_active; }
    KeyRequestType type() const { return m_type; }
    quint8 expectedAckCommand() const { return m_expectedAckCommand; }
    const QByteArray &lastFrame() const { return m_lastFrame; }
    int retryCount() const { return m_retryCount; }
    quint32 generation() const { return m_generation; }
    quint32 retryTimerGeneration() const { return m_retryTimerGeneration; }
    bool retryTimerMatchesCurrentRequest() const;
    bool qTaskFrameHeaderSeen() const { return m_qTaskFrameHeaderSeen; }

private:
    bool m_active = false;
    KeyRequestType m_type = KeyRequestType::None;
    quint8 m_expectedAckCommand = 0;
    QByteArray m_lastFrame;
    int m_retryCount = 0;
    quint32 m_generation = 0;
    quint32 m_retryTimerGeneration = 0;
    bool m_qTaskFrameHeaderSeen = false;
};

#endif // KEYREQUESTTRACKER_H
