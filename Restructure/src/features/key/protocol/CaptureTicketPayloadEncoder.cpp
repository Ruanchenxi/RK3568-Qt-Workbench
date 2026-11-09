#include "features/key/protocol/CaptureTicketPayloadEncoder.h"

#include <QDateTime>
#include <QJsonArray>
#include <QSet>
#include <QTextCodec>

namespace {

constexpr int kHeaderSize = 160;
constexpr int kStepEntrySize = 8;
constexpr int kMaxCaptureSteps = 300;
constexpr int kMaxPayloadBytes = 64 * 1024;
constexpr int kOffsetFileSize = 0;
constexpr int kOffsetTaskId = 8;
constexpr int kOffsetStationNo = 24;
constexpr int kOffsetTicketAttr = 25;
constexpr int kOffsetStepCount = 26;
constexpr int kOffsetTaskNamePtr = 28;
constexpr int kOffsetCreateTime = 32;
constexpr int kOffsetPlanTime = 40;
constexpr int kStepOffsetInnerCode = 0;
constexpr int kStepOffsetAction = 2;
constexpr int kStepOffsetState = 3;
constexpr int kStepOffsetDisplayPtr = 4;
constexpr quint8 kCaptureTicketAttr = 0x02;
constexpr quint8 kCaptureStepAction = 0x02;
constexpr quint8 kCaptureStepState = 0x00;

// Header taken from the verified capture-ticket profile. Variable fields are overwritten below.
const char kCaptureTicketHeaderHex[] =
        "E70100000111000002004974ACF3D11C0000000000000000010003006E010000"
        "202607140042FFFF202607150042FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"
        "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"
        "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"
        "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF";

void writeLe16(QByteArray &bytes, int offset, quint16 value)
{
    bytes[offset] = static_cast<char>(value & 0xFF);
    bytes[offset + 1] = static_cast<char>((value >> 8) & 0xFF);
}

void writeLe32(QByteArray &bytes, int offset, quint32 value)
{
    bytes[offset] = static_cast<char>(value & 0xFF);
    bytes[offset + 1] = static_cast<char>((value >> 8) & 0xFF);
    bytes[offset + 2] = static_cast<char>((value >> 16) & 0xFF);
    bytes[offset + 3] = static_cast<char>((value >> 24) & 0xFF);
}

void writeLe64To16(QByteArray &bytes, int offset, quint64 value)
{
    for (int i = 0; i < 8; ++i) bytes[offset + i] = static_cast<char>((value >> (i * 8)) & 0xFF);
    for (int i = 8; i < 16; ++i) bytes[offset + i] = '\0';
}

quint8 toBcd(int value)
{
    return static_cast<quint8>(((value / 10) << 4) | (value % 10));
}

bool writeTime(QByteArray &bytes, int offset, const QString &value)
{
    const QDateTime time = QDateTime::fromString(value, QStringLiteral("yyyyMMddhhmm"));
    if (!time.isValid() || offset < 0 || offset + 8 > bytes.size()) return false;
    const QDate date = time.date();
    const QTime clock = time.time();
    bytes[offset] = static_cast<char>(toBcd(date.year() % 100));
    bytes[offset + 1] = static_cast<char>(toBcd(date.month()));
    bytes[offset + 2] = static_cast<char>(toBcd(date.day()));
    bytes[offset + 3] = static_cast<char>(toBcd(clock.hour()));
    bytes[offset + 4] = static_cast<char>(toBcd(clock.minute()));
    bytes[offset + 5] = static_cast<char>(0xFF);
    bytes[offset + 6] = static_cast<char>(0xFF);
    bytes[offset + 7] = static_cast<char>(0xFF);
    return true;
}

bool toInnerCode(const QJsonValue &value, int *innerCode)
{
    bool ok = false;
    int parsed = -1;
    if (value.isDouble()) {
        const double raw = value.toDouble(-1);
        parsed = value.toInt(-1);
        ok = raw == parsed;
    } else if (value.isString()) {
        parsed = value.toString().trimmed().toInt(&ok);
    }
    if (!ok || parsed < 0 || parsed > 0xFFFF) return false;
    *innerCode = parsed;
    return true;
}

} // namespace

CaptureTicketPayloadEncoder::EncodeResult
CaptureTicketPayloadEncoder::encode(const QJsonObject &claimData)
{
    EncodeResult result;
    const QString taskId = claimData.value(QStringLiteral("taskId")).toString().trimmed();
    bool taskIdOk = false;
    const quint64 taskIdValue = taskId.toULongLong(&taskIdOk);
    if (!taskIdOk || taskIdValue == 0) {
        result.errorMsg = QStringLiteral("Invalid capture taskId");
        return result;
    }

    const QJsonValue stationValue = claimData.value(QStringLiteral("stationNo"));
    const double stationNumber = stationValue.toDouble(-1);
    const int stationNo = stationValue.toInt(-1);
    if (!stationValue.isDouble() || stationNumber != stationNo || stationNo < 1 || stationNo > 0xFF) {
        result.errorMsg = QStringLiteral("capture stationNo must be an integer in 1..255");
        return result;
    }

    const QJsonArray steps = claimData.value(QStringLiteral("steps")).toArray();
    const QJsonValue declaredStepValue = claimData.value(QStringLiteral("stepNum"));
    const double declaredStepNumber = declaredStepValue.toDouble(-1);
    const int declaredStepCount = declaredStepValue.toInt(-1);
    if (!declaredStepValue.isDouble() || declaredStepNumber != declaredStepCount
            || steps.isEmpty() || declaredStepCount != steps.size()
            || steps.size() > kMaxCaptureSteps) {
        result.errorMsg = QStringLiteral("capture stepNum must equal steps and be in 1..%1")
                                  .arg(kMaxCaptureSteps);
        return result;
    }

    const QString taskName = claimData.value(QStringLiteral("taskName")).toString().trimmed();
    if (taskName.isEmpty()) {
        result.errorMsg = QStringLiteral("capture taskName is required");
        return result;
    }

    QTextCodec *codec = QTextCodec::codecForName("GBK");
    if (!codec) codec = QTextCodec::codecForName("GB18030");
    if (!codec) {
        result.errorMsg = QStringLiteral("GBK codec is unavailable");
        return result;
    }

    struct StepData {
        int innerCode = 0;
        QByteArray detail;
    };
    QList<StepData> encodedSteps;
    encodedSteps.reserve(steps.size());
    QSet<int> innerCodes;
    for (int i = 0; i < steps.size(); ++i) {
        if (!steps.at(i).isObject()) {
            result.errorMsg = QStringLiteral("capture steps[%1] is not an object").arg(i);
            return result;
        }
        const QJsonObject step = steps.at(i).toObject();
        if (step.value(QStringLiteral("stepId")).toString().trimmed().isEmpty()) {
            result.errorMsg = QStringLiteral("capture steps[%1].stepId is required").arg(i);
            return result;
        }
        int innerCode = -1;
        if (!toInnerCode(step.value(QStringLiteral("innerCode")), &innerCode) || innerCodes.contains(innerCode)) {
            result.errorMsg = QStringLiteral("capture steps[%1].innerCode is invalid or duplicated").arg(i);
            return result;
        }
        const QString detail = step.value(QStringLiteral("stepDetail")).toString().trimmed();
        if (detail.isEmpty()) {
            result.errorMsg = QStringLiteral("capture steps[%1].stepDetail is required").arg(i);
            return result;
        }
        QByteArray detailBytes = codec->fromUnicode(detail);
        detailBytes.append('\0');
        encodedSteps.append({innerCode, detailBytes});
        innerCodes.insert(innerCode);
    }

    QByteArray taskNameBytes = codec->fromUnicode(taskName);
    taskNameBytes.append('\0');

    QByteArray payload = QByteArray::fromHex(kCaptureTicketHeaderHex);
    if (payload.size() != kHeaderSize) {
        result.errorMsg = QStringLiteral("capture ticket header profile is invalid");
        return result;
    }
    payload.append(QByteArray(encodedSteps.size() * kStepEntrySize, '\0'));

    writeLe64To16(payload, kOffsetTaskId, taskIdValue);
    payload[kOffsetStationNo] = static_cast<char>(stationNo);
    payload[kOffsetTicketAttr] = static_cast<char>(kCaptureTicketAttr);
    writeLe16(payload, kOffsetStepCount, static_cast<quint16>(encodedSteps.size()));
    if (!writeTime(payload, kOffsetCreateTime, claimData.value(QStringLiteral("createTime")).toString())
            || !writeTime(payload, kOffsetPlanTime, claimData.value(QStringLiteral("planTime")).toString())) {
        result.errorMsg = QStringLiteral("capture times must use yyyyMMddHHmm");
        return result;
    }

    for (int i = 0; i < encodedSteps.size(); ++i) {
        const int stepOffset = kHeaderSize + i * kStepEntrySize;
        const StepData &step = encodedSteps.at(i);
        writeLe16(payload, stepOffset + kStepOffsetInnerCode, static_cast<quint16>(step.innerCode));
        payload[stepOffset + kStepOffsetAction] = static_cast<char>(kCaptureStepAction);
        payload[stepOffset + kStepOffsetState] = static_cast<char>(kCaptureStepState);
        writeLe32(payload, stepOffset + kStepOffsetDisplayPtr, static_cast<quint32>(payload.size()));
        payload.append(step.detail);
    }

    writeLe32(payload, kOffsetTaskNamePtr, static_cast<quint32>(payload.size()));
    payload.append(taskNameBytes);
    if (payload.size() > kMaxPayloadBytes) {
        result.errorMsg = QStringLiteral("capture ticket payload exceeds %1B").arg(kMaxPayloadBytes);
        return result;
    }
    writeLe32(payload, kOffsetFileSize, static_cast<quint32>(payload.size()));

    result.taskId = taskId;
    result.stationNo = stationNo;
    result.payload = payload;
    result.fieldLog << QStringLiteral("[CAPTURE_PAYLOAD] attr=0x02 payload=%1B steps=%2 stationNo=%3 table=%4B")
                           .arg(payload.size())
                           .arg(encodedSteps.size())
                           .arg(stationNo)
                           .arg(encodedSteps.size() * kStepEntrySize);
    result.ok = true;
    return result;
}
