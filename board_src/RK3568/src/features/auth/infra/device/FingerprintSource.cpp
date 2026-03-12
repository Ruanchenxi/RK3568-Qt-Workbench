/**
 * @file FingerprintSource.cpp
 * @brief 指纹采集源占位实现
 */
#include "features/auth/infra/device/FingerprintSource.h"

FingerprintSource::FingerprintSource(QObject *parent)
    : ICredentialSource(parent)
{
}

AuthMode FingerprintSource::mode() const
{
    return AuthMode::Fingerprint;
}

void FingerprintSource::start()
{
    emit sourceError(QStringLiteral("FingerprintSource 尚未接入设备驱动"));
}

void FingerprintSource::stop()
{
}
