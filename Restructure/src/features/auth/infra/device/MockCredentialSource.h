/**
 * @file MockCredentialSource.h
 * @brief 凭据采集 Mock（仅手动触发，用于开发联调）
 */
#ifndef MOCK_CREDENTIAL_SOURCE_H
#define MOCK_CREDENTIAL_SOURCE_H

#include "features/auth/domain/ports/ICredentialSource.h"

class MockCredentialSource : public ICredentialSource
{
    Q_OBJECT
public:
    explicit MockCredentialSource(AuthMode mode, QObject *parent = nullptr);
    ~MockCredentialSource() override = default;

    AuthMode mode() const override;
    void start() override;
    void stop() override;

    void emitCard(const QString &tenantId, const QString &cardNo);
    void emitFingerprint(const QString &tenantId, const QString &fingerId, const QByteArray &templateData = QByteArray());

private:
    AuthMode m_mode;
    bool m_started;
};

#endif // MOCK_CREDENTIAL_SOURCE_H
