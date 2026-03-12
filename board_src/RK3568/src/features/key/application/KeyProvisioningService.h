/**
 * @file KeyProvisioningService.h
 * @brief 初始化/RFID 后端取数与 payload 组装服务
 */
#ifndef KEYPROVISIONINGSERVICE_H
#define KEYPROVISIONINGSERVICE_H

#include <QObject>
#include <QString>

#include "shared/contracts/IAuthService.h"

class QNetworkAccessManager;
class QNetworkReply;

class KeyProvisioningService : public QObject
{
    Q_OBJECT
public:
    explicit KeyProvisioningService(IAuthService *auth = nullptr, QObject *parent = nullptr);
    ~KeyProvisioningService() override = default;

    bool isBusy() const { return m_requestInFlight; }
    void requestInitPayload(int stationNo);
    void requestRfidPayload(int stationNo);

signals:
    void logAppended(const QString &text);
    void initPayloadReady(const QByteArray &payload);
    void rfidPayloadReady(const QByteArray &payload);
    void requestFailed(const QString &reason);

private:
    enum class RequestKind {
        Init,
        Rfid
    };

    QString endpointUrl(RequestKind kind) const;
    void startRequest(RequestKind kind, int stationNo);
    void finishRequest(QNetworkReply *reply, RequestKind kind, int stationNo);
    QString nowText() const;
    QString prettyJson(const QByteArray &json) const;

    IAuthService *m_auth;
    QNetworkAccessManager *m_network;
    bool m_requestInFlight;
};

#endif // KEYPROVISIONINGSERVICE_H
