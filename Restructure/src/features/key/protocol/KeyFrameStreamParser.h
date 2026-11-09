#ifndef KEYFRAMESTREAMPARSER_H
#define KEYFRAMESTREAMPARSER_H

#include <QByteArray>
#include <QtGlobal>

// [LEARNING GUIDE][协议拆帧检查的声明]
// 位置：src/features/key/protocol；本 .h 定义结果类型、声明函数，对应 .cpp 实现检查规则。
// 从已学代码进入：KeySerialClient::onReadyRead()
// -> KeySerialClient::tryExtractOneFrame() -> KeyFrameStreamParser::inspect()。
// 下面的 namespace 是名字分组，不是一个类；不需要 new，也没有 QObject parent。
// READ NOW：ResultKind 是结论的可选值，InspectResult 是装结论的结构体，inspect 是检查函数。
// RETURN LATER：CRC 的计算算法；这里先认识计算值与收到值两个字段即可。
namespace KeyFrameStreamParser {

// [LEARNING] enum class 定义枚举类型：列出“检查结论可以是哪一种”。
// ResultKind::NeedMoreData 是其中一个值；它既不是函数调用，也不是对象创建。
enum class ResultKind {
    NeedMoreData = 0, // 字节不足，需要等后续数据；0 是这个枚举值的数值。
    FrameReady,      // 已收齐且 CRC 通过，可以提取一帧。
    DropNoHeader,    // 找不到完整帧头；建议丢弃前面的字节，保留最后 1 字节。
    DropPrefix,      // 找到了帧头，但前面有杂字节；建议只丢弃该前缀。
    InvalidLength,   // 长度超出允许范围；建议丢 1 字节后重新找帧头。
    CrcMismatch      // 本机计算 CRC 与收到的 CRC 不同；建议丢 1 字节后重新找帧头。
};

// [LEARNING] struct 定义一种结果类型；这一段定义本身不会创建 result 对象。
// inspect() 的 .cpp 内执行 InspectResult result;，才创建局部结果对象。
// 各成员使用下面的默认值初始化，因此刚创建时 result.kind 就是 NeedMoreData。
struct InspectResult {
    ResultKind kind = ResultKind::NeedMoreData; // 类型 ResultKind；成员名 kind；默认结论“继续等”。
    int dropBytes = 0;          // 建议调用方丢弃的字节数；检查函数自身不删除缓冲区。
    int frameLength = 0;        // 根据长度字段推算的整帧字节数；是否可取还要看 kind。
    quint16 lengthField = 0;    // 从帧中读取的 Len 数值，只计 Cmd + Data。
    quint16 calculatedCrc = 0;  // 接收方根据 Cmd + Data 重新计算的 CRC。
    quint16 receivedCrc = 0;    // 帧尾携带的 CRC，用于与 calculatedCrc 比较。
};

/**
 * Inspect one receive-buffer position without mutating it. The caller owns
 * buffer removal, resynchronization logging, and key-session noise effects.
 */
// [LEARNING] 这是函数声明：输入只读累计字节 buffer、允许的数据长度上限 maxDataLength；
// 返回 InspectResult 结构体。函数体在 KeyFrameStreamParser.cpp 中。
// 调用位置在 KeySerialClient::tryExtractOneFrame() 内：
// const KeyFrameStreamParser::InspectResult inspected =
//         KeyFrameStreamParser::inspect(buf, MAX_DATA_LEN);
// 右边调用函数；函数内部用 result 填写并返回结论；左边用返回结果初始化局部变量 inspected。
// result 与 inspected 属于不同函数；调用方根据 inspected.kind 决定等待、丢弃或提取字节。
InspectResult inspect(const QByteArray &buffer, int maxDataLength);

} // namespace KeyFrameStreamParser

#endif // KEYFRAMESTREAMPARSER_H
