/**
 * @file RfidPayloadEncoder.h
 * @brief RFID 打包数据编码器（HTTP JSON -> DN_RFID payload）
 */
#ifndef RFIDPAYLOADENCODER_H
#define RFIDPAYLOADENCODER_H

#include <QByteArray>
#include <QJsonObject>
#include <QStringList>

class RfidPayloadEncoder
{
public:
    struct EncodeResult {
        QByteArray payload;
        QStringList fieldLog;
        bool ok = false;
        QString errorMsg;
    };

    static EncodeResult encode(const QJsonObject &dataObj);

private:
    static void writeLe32(QByteArray &buf, int offset, quint32 value);
};

#endif // RFIDPAYLOADENCODER_H
