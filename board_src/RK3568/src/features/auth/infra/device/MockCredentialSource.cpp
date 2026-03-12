/**
 * @file MockCredentialSource.cpp
 * @brief 凭据采集 Mock 实现
 */
#include "features/auth/infra/device/MockCredentialSource.h"

MockCredentialSource::MockCredentialSource(AuthMode mode, QObject *parent)
    : ICredentialSource(parent)
    , m_mode(mode)
    , m_started(false)
{
}

AuthMode MockCredentialSource::mode() const
{
    return m_mode;
}

void MockCredentialSource::start()
{
    m_started = true;
}

void MockCredentialSource::stop()
{
    m_started = false;
}

void MockCredentialSource::emitCard(const QString &tenantId, const QString &cardNo)
{
    if (!m_started || m_mode != AuthMode::Card) {
        return;
    }
    CardCredential credential;
    credential.tenantId = tenantId;
    credential.cardNo = cardNo;
    emit cardCaptured(credential);
}

void MockCredentialSource::emitFingerprint(const QString &tenantId, const QString &fingerId, const QByteArray &templateData)
{
    if (!m_started || m_mode != AuthMode::Fingerprint) {
        return;
    }
    FingerprintCredential credential;
    credential.tenantId = tenantId;
    credential.fingerId = fingerId;
    credential.templateData = templateData;
    emit fingerprintCaptured(credential);
}
