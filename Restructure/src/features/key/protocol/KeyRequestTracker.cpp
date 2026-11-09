#include "KeyRequestTracker.h"

void KeyRequestTracker::begin(KeyRequestType type,
                              quint8 expectedAckCommand,
                              const QByteArray &frame)
{
    m_active = true;
    m_type = type;
    m_expectedAckCommand = expectedAckCommand;
    m_lastFrame = frame;
    m_retryCount = 0;
    ++m_generation;
    m_retryTimerGeneration = m_generation;
    m_qTaskFrameHeaderSeen = false;
}

void KeyRequestTracker::armRetryTimer()
{
    m_retryCount = 0;
    m_retryTimerGeneration = m_generation;
}

int KeyRequestTracker::recordTimeout()
{
    return ++m_retryCount;
}

void KeyRequestTracker::clearRetry()
{
    m_retryCount = 0;
    m_expectedAckCommand = 0;
    m_retryTimerGeneration = 0;
    m_lastFrame.clear();
}

void KeyRequestTracker::clearRequest()
{
    m_active = false;
    m_type = KeyRequestType::None;
    m_expectedAckCommand = 0;
    m_qTaskFrameHeaderSeen = false;
}

void KeyRequestTracker::markQTaskFrameHeaderSeen()
{
    if (m_active && m_type == KeyRequestType::QTask) {
        m_qTaskFrameHeaderSeen = true;
    }
}

bool KeyRequestTracker::retryTimerMatchesCurrentRequest() const
{
    return m_active && m_retryTimerGeneration == m_generation;
}
