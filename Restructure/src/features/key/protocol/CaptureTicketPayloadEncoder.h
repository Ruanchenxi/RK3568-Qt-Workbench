#ifndef CAPTURETICKETPAYLOADENCODER_H
#define CAPTURETICKETPAYLOADENCODER_H

#include <QByteArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>

class CaptureTicketPayloadEncoder
{
public:
    struct EncodeResult {
        QByteArray payload;
        QString taskId;
        int stationNo = 0;
        QStringList fieldLog;
        bool ok = false;
        QString errorMsg;
    };

    static EncodeResult encode(const QJsonObject &claimData);
};

#endif // CAPTURETICKETPAYLOADENCODER_H
