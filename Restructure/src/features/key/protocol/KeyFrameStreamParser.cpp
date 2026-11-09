#include "KeyFrameStreamParser.h"

#include "KeyCrc16.h"
#include "KeyProtocolDefs.h"

// [LEARNING][串口接收 -> 协议拆帧]
// 路径：src/features/key/protocol；.h 定义检查结果和函数声明，.cpp 实现检查规则。
// KeyFrameStreamParser 是命名空间，不是新建的 QObject；inspect() 是其中的辅助函数。
// 进入顺序：onReadyRead() 累积字节 -> tryExtractOneFrame() -> inspect() 检查当前缓冲区。
// 本函数只读 buffer，返回“等更多数据 / 可取帧 / 应丢弃字节”的结果；不读串口、不删缓冲区。
// READ NOW：默认结果、找帧头、读长度、等整帧、比较 CRC；位运算与 CRC 算法可随后定点学习。
// 返回后：KeySerialClient 按结果修改累计缓冲区；取帧成功后由 handleFrame() 解释命令。

namespace {

constexpr int kLengthOffset = 6;
constexpr int kCommandOffset = 8;
constexpr int kDataOffset = 9;
constexpr int kFrameOverhead = 11;

} // namespace

namespace KeyFrameStreamParser {

InspectResult inspect(const QByteArray &buffer, int maxDataLength)
{
    // [LEARNING] InspectResult 在 .h 中默认 kind = NeedMoreData；直接 return result 就表示继续等。
    InspectResult result;

    int headerPosition = -1;
    for (int i = 0; i <= buffer.size() - 2; ++i)//在当前字节中寻找 7E 6C 
    {
        if (static_cast<quint8>(buffer[i]) == KeyProtocol::FrameHeader0
                && static_cast<quint8>(buffer[i + 1]) == KeyProtocol::FrameHeader1) {
            headerPosition = i;
            break;
        }
    }

    if (headerPosition < 0) {
        // [LEARNING] 找不到完整帧头时建议保留最后 1 字节，防止末尾 7E 与下次收到的 6C 被拆开。
        if (buffer.size() > 1) {
            result.kind = ResultKind::DropNoHeader;
            result.dropBytes = buffer.size() - 1;
        }
        return result;
    }

    if (headerPosition > 0) {
        // [LEARNING] 帧头前有杂字节：只建议丢弃前缀，调用方移除后立即重新检查。
        result.kind = ResultKind::DropPrefix;
        result.dropBytes = headerPosition;
        return result;
    }

    if (buffer.size() < kLengthOffset + 2) {
        return result;
    }

    // [LEARNING] 下标 6、7 是长度字段，低字节在前；Len = 1 + 数据长度，整帧长 = 10 + Len。
    result.lengthField = static_cast<quint8>(buffer[kLengthOffset])
            | (static_cast<quint8>(buffer[kLengthOffset + 1]) << 8);
    const int dataLength = static_cast<int>(result.lengthField) - 1;
    result.frameLength = kFrameOverhead + dataLength;

    if (result.lengthField < 1 || result.lengthField > (1 + maxDataLength)) {
        result.kind = ResultKind::InvalidLength;
        result.dropBytes = 1;
        return result;
    }

    if (buffer.size() < result.frameLength) {
        // [LEARNING] 这是半帧等待点：不删除已有数据，也不阻塞等待；返回调用方，等下次 readyRead。
        return result;
    }

    result.calculatedCrc =
            KeyCrc16::calc(buffer.mid(kCommandOffset, result.lengthField));
    const int crcOffset = kDataOffset + dataLength;
    result.receivedCrc =
            (static_cast<quint16>(static_cast<quint8>(buffer[crcOffset])) << 8)
            | static_cast<quint8>(buffer[crcOffset + 1]);

    if (result.calculatedCrc != result.receivedCrc) {
        // [LEARNING] 疑似帧校验失败时仅建议丢 1 字节再找帧头，避免按不可信长度误删后续好帧。
        result.kind = ResultKind::CrcMismatch;
        result.dropBytes = 1;
        return result;
    }

    result.kind = ResultKind::FrameReady;
    return result;
}

} // namespace KeyFrameStreamParser
