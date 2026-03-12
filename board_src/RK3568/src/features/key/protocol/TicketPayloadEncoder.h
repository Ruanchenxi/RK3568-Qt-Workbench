/**
 * @file TicketPayloadEncoder.h
 * @brief 传票 payload 编码器（JSON -> payload）
 *
 * 说明：
 * 1. 复用已验证通过的测试工具编码规则，不在主程序中重新发明协议。
 * 2. 只承载纯协议编码逻辑，不做 UI、文件 I/O 或串口发送。
 */
#ifndef TICKETPAYLOADENCODER_H
#define TICKETPAYLOADENCODER_H

#include <QByteArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>

class TicketPayloadEncoder
{
public:
    enum class StringEncoding {
        GBK,
        UTF8
    };

    struct EncodeResult {
        QByteArray  payload;
        QStringList fieldLog;
        bool        ok = false;
        QString     errorMsg;
    };

    static bool extractTicketObject(const QByteArray &jsonBytes,
                                    QJsonObject &ticketObj,
                                    QString &errorMsg);

    static EncodeResult encode(const QJsonObject &ticketJson,
                               quint8 stationId = 0x01,
                               StringEncoding enc = StringEncoding::GBK);

    static QByteArray timeToBcd(const QString &timeStr, int targetLen = 8);
    static QByteArray uint64LeBytes(quint64 val, int totalLen = 16);

    static constexpr int HEADER_SIZE     = 160;
    static constexpr int STEP_ENTRY_SIZE = 8;

    static constexpr int OFF_FILE_SIZE    = 0;
    static constexpr int OFF_VERSION      = 4;
    static constexpr int OFF_RESERVED     = 6;
    static constexpr int OFF_TASK_ID      = 8;
    static constexpr int OFF_STATION_ID   = 24;
    static constexpr int OFF_TICKET_ATTR  = 25;
    static constexpr int OFF_STEP_NUM     = 26;
    static constexpr int OFF_TASKNAME_PTR = 28;
    static constexpr int OFF_CREATE_TIME  = 32;
    static constexpr int OFF_PLAN_TIME    = 40;
    static constexpr int OFF_AUTH         = 48;
    static constexpr int OFF_ADD_FIELDS   = 96;
    static constexpr int OFF_RESERVED2    = 128;

    static constexpr int STEP_OFF_INNER_CODE = 0;
    static constexpr int STEP_OFF_LOCKER_OP  = 2;
    static constexpr int STEP_OFF_STATE      = 3;
    static constexpr int STEP_OFF_DISP_PTR   = 4;

private:
    static void writeLE32(QByteArray &buf, int offset, quint32 v);
    static void writeLE16(QByteArray &buf, int offset, quint16 v);
    static QString hexDump(const QByteArray &data, int maxBytes = 32);
};

#endif // TICKETPAYLOADENCODER_H
