/**
 * @file ICredentialSource.h
 * @brief 凭据采集源抽象（刷卡/指纹）
 */
#ifndef I_CREDENTIAL_SOURCE_H
#define I_CREDENTIAL_SOURCE_H

#include <QObject>

#include "features/auth/domain/AuthTypes.h"

class ICredentialSource : public QObject
{
    Q_OBJECT
public:
    explicit ICredentialSource(QObject *parent = nullptr)
        : QObject(parent)
    {
    }
    ~ICredentialSource() override = default;

    virtual AuthMode mode() const = 0;
    virtual void start() = 0;
    virtual void stop() = 0;

signals:
    void cardCaptured(const CardCredential &credential);
    void fingerprintCaptured(const FingerprintCredential &credential);
    void sourceError(const QString &message);
};

#endif // I_CREDENTIAL_SOURCE_H
