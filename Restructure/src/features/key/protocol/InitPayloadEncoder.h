/**
 * @file InitPayloadEncoder.h
 * @brief 初始化打包数据编码器（HTTP JSON -> INIT payload）
 */
#ifndef INITPAYLOADENCODER_H
#define INITPAYLOADENCODER_H

#include <QByteArray>
#include <QJsonObject>
#include <QStringList>

class InitPayloadEncoder
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
    static void writeLe16(QByteArray &buf, int offset, quint16 value);
    static void writeLe32(QByteArray &buf, int offset, quint32 value);
};

#endif // INITPAYLOADENCODER_H
