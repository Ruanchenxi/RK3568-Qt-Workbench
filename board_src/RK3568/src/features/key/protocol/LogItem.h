/**
 * @file LogItem.h
 * @brief 串口通信结构化日志条目
 *
 * 用于 KeySerialClient → KeyManagePage 的日志传递，
 * 每条协议交互产生一个 LogItem，由 serialLogTable 渲染显示。
 */
#ifndef LOGITEM_H
#define LOGITEM_H

#include <QString>
#include <QByteArray>
#include <QDateTime>

/**
 * @brief 日志方向枚举
 *
 * 颜色映射（UI 显示用）：
 *   TX    = 绿色  (主动发送)
 *   RX    = 蓝色  (收到正常帧)
 *   RAW   = 红色  (原始字节/resync 丢弃)
 *   EVENT = 紫色  (钥匙事件)
 *   WARN  = 橙色  (协议警告)
 *   ERROR = 红色  (错误/NAK)
 */
enum class LogDir {
    TX,     ///< 发送
    RX,     ///< 接收（正常帧）
    RAW,    ///< 原始字节 / resync 丢弃（专家模式）
    EVENT,  ///< 钥匙事件（KEY_PLACED / KEY_REMOVED）
    WARN,   ///< 协议警告
    ERROR   ///< 错误 / NAK
};

static constexpr quint8 LogCmdNone = 0xFF;  ///< 非协议帧占位命令码，仅用于日志显示

/**
 * @brief 结构化串口日志条目
 */
struct LogItem {
    QDateTime  timestamp;   ///< 产生时刻（本地时间）
    LogDir     dir;         ///< 方向
    quint8     cmd;         ///< 命令码（TX: 发出的 Cmd；RX: 响应中的 Cmd）
    quint16    length;      ///< Len 字段值（Cmd + Data 字节数）
    QString    summary;     ///< 人类可读摘要（如 "SET_COM ACK ok"）
    QByteArray hex;         ///< 完整帧或关键字节的 Hex 表示
    bool       crcOk;       ///< CRC 校验是否通过
    bool       expertOnly;  ///< true = 仅专家模式下显示（RAW / 工程诊断）
    int        opId;        ///< 操作 ID，-1 表示无归属（关联请求-响应日志）

    LogItem()
        : dir(LogDir::TX), cmd(LogCmdNone), length(0), crcOk(true), expertOnly(false), opId(-1)
    {}
};

#endif // LOGITEM_H
