/**
 * @file FingerprintSource.h
 * @brief 指纹采集源占位实现
 */
#ifndef FINGERPRINT_SOURCE_H
#define FINGERPRINT_SOURCE_H

#include "features/auth/domain/ports/ICredentialSource.h"

class FingerprintSource : public ICredentialSource
{
    Q_OBJECT
public:
    explicit FingerprintSource(QObject *parent = nullptr);
    ~FingerprintSource() override = default;

    AuthMode mode() const override;
    void start() override;
    void stop() override;
};

#endif // FINGERPRINT_SOURCE_H
