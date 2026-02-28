/**
 * @file KeyProtocolDefs.h
 * @brief X5 智能钥匙柜串口协议常量定义
 *
 * 帧格式：[7E 6C] [KeyId 1B] [FrameNo 1B] [Addr2 2B] [Len 2B LE] [Cmd 1B] [Data N B] [CRC16 2B BE]
 *   - CRC16 校验范围：Cmd(1) + Data(N)，高字节在前
 *   - Len 字段值 = 1(Cmd) + N(Data)
 */
#ifndef KEYPROTOCOLDEFS_H
#define KEYPROTOCOLDEFS_H

#include <QtGlobal>

namespace KeyProtocol {

// ---------- 帧头 ----------
static constexpr quint8  FrameHeader0   = 0x7E;  ///< 帧头第一字节
static constexpr quint8  FrameHeader1   = 0x6C;  ///< 帧头第二字节

// ---------- 广播 KeyId ----------
static constexpr quint8  ReqKeyId       = 0xFF;  ///< 广播查询所有钥匙柜

// ---------- 命令码 ----------
static constexpr quint8  CmdSetCom      = 0x0F;  ///< 握手命令
static constexpr quint8  CmdQTask       = 0x04;  ///< 查询任务列表
static constexpr quint8  CmdDel         = 0x06;  ///< 删除任务
static constexpr quint8  CmdAck         = 0x5A;  ///< 设备确认
static constexpr quint8  CmdNak         = 0x00;  ///< 设备拒绝
static constexpr quint8  CmdKeyEvent    = 0x11;  ///< 钥匙事件（在位/离位）

// ---------- 超时与重试 ----------
static constexpr int     RetryTimeoutMs = 1000;  ///< 单次重试超时 1000ms
static constexpr int     MaxRetries     = 3;     ///< 最大重试次数

// ---------- NAK 错误码 ----------
static constexpr quint8  ErrCrcFail     = 0x03;  ///< CRC 校验失败
static constexpr quint8  ErrLenMismatch = 0x04;  ///< 字节数不符
static constexpr quint8  ErrGuidMissing = 0x08;  ///< GUID 不存在

} // namespace KeyProtocol

#endif // KEYPROTOCOLDEFS_H
