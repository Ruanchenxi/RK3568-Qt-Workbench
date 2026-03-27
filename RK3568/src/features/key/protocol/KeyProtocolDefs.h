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

// ---------- Addr2（2字节地址扩展） ----------
// 规则（2026-03-04 A/B 抓包验证）：
//   1) SET_COM / Q_TASK / Q_KEYEQ / SET_TIME：TX 固定 Addr2=0x0000（与"当前站号"无关）
//   2) DEL(0x06)：TX Addr2 = 当前站号（stationId），小端 Lo=stationId&0xFF Hi=0x00
//      例：stationId=1 → Addr2字节 01 00；stationId=2 → 02 00
//   3) 回包（RX）Addr2 常为 0x00FF，解析器不以 Addr2 做过滤条件
// 注意：Addr2DelLo/Hi 已作废，DEL 的 Addr2 由 KeySerialClient::setStationId() 动态计算
static constexpr quint8  Addr2DefaultLo = 0x00;  ///< SET_COM/Q_TASK/Q_KEYEQ：Lo=0x00
static constexpr quint8  Addr2DefaultHi = 0x00;  ///< SET_COM/Q_TASK/Q_KEYEQ：Hi=0x00
// Addr2DelLo/Hi 已废弃（原固定 0x01/0x00），请使用 KeySerialClient::setStationId() 动态注入

// ---------- 命令码 ----------
static constexpr quint8  CmdSetCom      = 0x0F;  ///< 握手命令
static constexpr quint8  CmdInit        = 0x02;  ///< 初始化电脑钥匙
static constexpr quint8  CmdSetTime     = 0x09;  ///< 校时（7 字节时间同步）
static constexpr quint8  CmdQTask       = 0x04;  ///< 查询任务列表
static constexpr quint8  CmdITaskLog    = 0x05;  ///< 请求任务操作日志
static constexpr quint8  CmdDel         = 0x06;  ///< 删除任务
static constexpr quint8  CmdDownloadRfid = 0x1A; ///< 下传 RFID/采码记录文件
static constexpr quint8  CmdTicket      = 0x03;  ///< 传票单帧/最后帧
static constexpr quint8  CmdTicketMore  = 0x83;  ///< 传票还有后续帧
static constexpr quint8  CmdQKeyEq      = 0x14;  ///< 查询钥匙电量（TX: Data 空；RX: Data[0]=0~100%，0xFF=无效）
static constexpr quint8  CmdUpTaskLog   = 0x15;  ///< 回传任务操作日志
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
