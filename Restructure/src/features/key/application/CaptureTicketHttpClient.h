#ifndef CAPTURETICKETHTTPCLIENT_H
#define CAPTURETICKETHTTPCLIENT_H

#include <QObject>
#include <QJsonArray>
#include <QJsonObject>

class IAuthService;
class QNetworkAccessManager;

// L4 adapter for the two backend capture-ticket endpoints.
class CaptureTicketHttpClient : public QObject
{
    Q_OBJECT
public:
    explicit CaptureTicketHttpClient(IAuthService *auth, QObject *parent = nullptr);

    void claim(int stationNo, int operationId);
    void finish(const QString &taskId,
                const QJsonArray &taskLogItems,
                int operationId);

signals:
    void logAppended(const QString &text);
    void claimSucceeded(int operationId, const QJsonObject &data);
    void claimFailed(int operationId, const QString &reason);
    void finishSucceeded(int operationId, const QString &taskId);
    void finishFailed(int operationId, const QString &taskId,
                      const QString &reason);

private:
    void post(bool claimRequest,
              int operationId,
              const QString &taskId,
              const QString &path,
              const QJsonObject &body);
    QString endpoint(const QString &path) const;

    IAuthService *m_auth;
    QNetworkAccessManager *m_network;
};

#endif // CAPTURETICKETHTTPCLIENT_H
