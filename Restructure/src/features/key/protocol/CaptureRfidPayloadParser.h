#ifndef CAPTURERFIDPAYLOADPARSER_H
#define CAPTURERFIDPAYLOADPARSER_H

#include <QByteArray>
#include <QString>
#include <QVariantList>

struct CaptureRfidParseResult
{
    bool ok = false;
    int stationNo = 0;
    int recordSize = 0;
    QString version;
    QVariantList records;
    QString error;
};

class CaptureRfidPayloadParser
{
public:
    // Parses the cumulative UP_RFID file without transport side effects.
    static CaptureRfidParseResult parse(const QByteArray &payload);
};

#endif // CAPTURERFIDPAYLOADPARSER_H
