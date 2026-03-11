/**
 * @file KeySerialClient.h
 * @brief X5 智能钥匙柜串口协议客户端头文件
 *
 * 模块定位：
 *   protocol 层的核心模块，封装了 X5 钥匙柜的完整串口通讯协议栈，
 *   包括帧构建、拆包、CRC 校验、重试、超时、钥匙稳定性检测等。
 *   是 KeyManagePage（UI 层）与物理钥匙柜硬件之间的唯一通讯桥梁。
 *
 * 协议概述（X5 智能钥匙柜串口通讯协议 V2.x）：
 *   帧格式：[7E 6C] [KeyId] [FrameNo] [Addr2(2B)] [Len(2B LE)] [Cmd] [Data...] [CRC16(2B BE)]
 *   - 帧头：固定 0x7E 0x6C
 *   - KeyId：钥匙编号，0xFF=广播查询所有钥匙
 *   - FrameNo：帧序号，每次发送自增（用于匹配请求-响应）
 *   - Addr2：2 字节地址扩展（SET_COM/Q_TASK 用 0x0000，DEL/I_TASK_LOG 使用当前站号）
 *   - Len：2 字节小端，值 = Cmd(1) + Data(N) 的字节数
 *   - Cmd：命令码（SET_COM=0x0F, Q_TASK=0x04, I_TASK_LOG=0x05, DEL=0x06, UP_TASK_LOG=0x15, ACK=0x5A）
 *   - CRC16：高字节在前，校验范围 = Cmd + Data
 *
 * 状态机概述：
 *   连接 → SET_COM 握手 → 等待 ACK → sessionReady
 *   KEY_EVENT(放上) → 稳定性窗口 → keyStable → 可发送 Q_TASK/DEL
 *   KEY_EVENT(拿走) → 重置所有状态
 *
 * 关键补丁说明（不要修改协议逻辑，仅可添加注释）：
 *   - KEY_STABLE 窗口：钥匙放上后等待 600ms 无噪声才标记稳定
 *   - drain 机制：业务命令发送前清空接收缓冲区，避免残留数据干扰
 *   - resync 机制：拆包时遇到非法帧头/CRC 失败，滑窗 1 字节重同步
 *   - startup probe：SET_COM ACK 后若无 KEY_EVENT，主动探测 Q_TASK
 *
 * 依赖关系：
 *   - ISerialTransport / QTimer / QElapsedTimer（协议层只依赖传输抽象）
 *   - KeyProtocolDefs.h（协议常量）
 *   - KeyCrc16.h（CRC16 计算，通过 calcCrc 间接调用）
 *   - LogItem.h（结构化日志条目）
 *
 * 被依赖：
 *   - KeySessionService（装配具体传输并转发会话事件）
 */
#ifndef KEYSERIALCLIENT_H
#define KEYSERIALCLIENT_H

#include <QObject>
#include <QTimer>
#include <QElapsedTimer>
#include <QList>
#include <QByteArray>
#include <QVariantMap>
#include <QStringList>
#include "platform/serial/ISerialTransport.h"
#include "KeyProtocolDefs.h"
#include "LogItem.h"
#include "features/key/protocol/TicketPayloadEncoder.h"
#include "features/key/protocol/TicketFrameBuilder.h"

// ============================================================
// 协议常量（来自 SerialTestWidget 验证通过，不要修改）
// 所有常量从 KeyProtocolDefs.h 引用，此处仅做短名别名，
// 避免在 .cpp 中频繁写 KeyProtocol:: 前缀。
// ============================================================
namespace KeyProto {

// --- 帧头 ---
static constexpr quint8 HEADER_0 = KeyProtocol::FrameHeader0;  ///< 帧头第一字节 0x7E
static constexpr quint8 HEADER_1 = KeyProtocol::FrameHeader1;  ///< 帧头第二字节 0x6C

// --- 帧字段偏移量（字节偏移，从帧头开始计算） ---
static const int OFF_HEADER  = 0;   ///< 帧头偏移（2 字节：0x7E 0x6C）
static const int OFF_KEYID   = 2;   ///< KeyId 偏移（1 字节）
static const int OFF_FRAMENO = 3;   ///< FrameNo 偏移（1 字节）
static const int OFF_ADDR2   = 4;   ///< Addr2 偏移（2 字节）
static const int OFF_LEN     = 6;   ///< Len 偏移（2 字节，小端）
static const int OFF_CMD     = 8;   ///< Cmd 偏移（1 字节）
static const int OFF_DATA    = 9;   ///< Data 起始偏移
static const int FRAME_OVERHEAD = 11; ///< 帧固定开销 = Header(2)+KeyId(1)+FrameNo(1)+Addr2(2)+Len(2)+Cmd(1)+CRC(2)
static const int MIN_FRAME_LEN = 11; ///< 最小帧长（Data 为空时）

// --- 命令码别名 ---
static constexpr quint8 CMD_SET_COM   = KeyProtocol::CmdSetCom;   ///< 握手命令 0x0F
static constexpr quint8 CMD_INIT      = KeyProtocol::CmdInit;     ///< 初始化电脑钥匙 0x02
static constexpr quint8 CMD_Q_TASK    = KeyProtocol::CmdQTask;    ///< 查询任务 0x04
static constexpr quint8 CMD_I_TASK_LOG = KeyProtocol::CmdITaskLog; ///< 请求回传任务日志 0x05
static constexpr quint8 CMD_DEL       = KeyProtocol::CmdDel;      ///< 删除任务 0x06
static constexpr quint8 CMD_DN_RFID   = KeyProtocol::CmdDownloadRfid; ///< 下传 RFID 数据 0x1A
static constexpr quint8 CMD_TICKET    = KeyProtocol::CmdTicket;   ///< 传票单帧/最后帧 0x03
static constexpr quint8 CMD_TICKET_MORE = KeyProtocol::CmdTicketMore; ///< 传票还有后续帧 0x83
static constexpr quint8 CMD_ACK       = KeyProtocol::CmdAck;      ///< 设备确认 0x5A
static constexpr quint8 CMD_NAK       = KeyProtocol::CmdNak;      ///< 设备拒绝 0x00
static constexpr quint8 CMD_KEY_EVENT = KeyProtocol::CmdKeyEvent;  ///< 钥匙事件 0x11
static constexpr quint8 CMD_UP_TASK_LOG = KeyProtocol::CmdUpTaskLog; ///< 回传任务日志 0x15

static constexpr quint8 REQ_KEYID = KeyProtocol::ReqKeyId;  ///< 广播 KeyId 0xFF

// --- 超时与重试 ---
static constexpr int RETRY_TIMEOUT_MS = KeyProtocol::RetryTimeoutMs;  ///< 单次重试超时 1000ms
static constexpr int MAX_RETRIES      = KeyProtocol::MaxRetries;      ///< 最大重试次数 3

// --- 钥匙稳定性检测时序常量 ---
// 这些时序参数经过现场调试确定，修改可能导致误判钥匙状态
static constexpr int KEY_STABLE_WINDOW_MS = 600;         ///< 钥匙放上后的稳定性观察窗口（ms）
static constexpr int KEY_QUIET_WINDOW_MS = 400;          ///< 最后一次噪声后的静默等待时间（ms）
static constexpr int KEY_PLACED_HANDSHAKE_DELAY_MS = 600; ///< 钥匙放上后延迟发送 SET_COM 握手（ms）
static constexpr int STARTUP_PROBE_DELAY_MS = 200;       ///< SET_COM ACK 后探测 Q_TASK 的延迟（ms）
static constexpr int QTASK_RECOVERY_DELAY_MS = 500;      ///< Q_TASK 软超时后重试延迟（ms）
static constexpr int QTASK_RECOVERY_ROUNDS = 2;          ///< Q_TASK 软超时最大重试轮数
// --- 数据长度约束 ---
static const int MAX_DATA_LEN     = 514;  ///< Data 区最大字节数（协议上限，超过视为 Len 异常）
static const int TASK_INFO_LEN    = 20;   ///< 单条任务信息长度：taskId(16) + status(1) + type(1) + source(1) + reserved(1)
static const int TASK_ID_LEN      = 16;   ///< 任务 ID 固定 16 字节（GUID 二进制形式）

// --- NAK 错误码别名 ---
static constexpr quint8 ERR_CRC_FAIL     = KeyProtocol::ErrCrcFail;      ///< 0x03 CRC 校验失败
static constexpr quint8 ERR_LEN_MISMATCH = KeyProtocol::ErrLenMismatch;  ///< 0x04 字节数不符
static constexpr quint8 ERR_GUID_MISSING = KeyProtocol::ErrGuidMissing;  ///< 0x08 GUID 不存在

// --- inFlight 状态标识 ---
// 标识当前正在等待响应的命令类型，用于匹配 ACK/NAK 和超时处理
static const quint8 INFLIGHT_NONE   = 0;  ///< 空闲，无待响应命令
static const quint8 INFLIGHT_SETCOM = 1;  ///< 正在等待 SET_COM 的 ACK
static const quint8 INFLIGHT_QTASK  = 2;  ///< 正在等待 Q_TASK 的响应帧
static const quint8 INFLIGHT_DEL    = 3;  ///< 正在等待 DEL 的 ACK
static const quint8 INFLIGHT_TICKET = 4;  ///< 正在等待传票分帧 ACK
static const quint8 INFLIGHT_TASKLOG = 5; ///< 正在等待 I_TASK_LOG 的响应帧
static const quint8 INFLIGHT_INIT_TRANSFER = 6; ///< 正在等待 INIT 分帧 ACK
static const quint8 INFLIGHT_RFID_TRANSFER = 7; ///< 正在等待 DN_RFID 分帧 ACK

} // namespace KeyProto

// ============================================================
// 任务信息结构体
// Q_TASK 响应帧中每条任务的解析结果，20 字节对应一个 KeyTaskInfo
// ============================================================
/**
 * @brief 钥匙上的任务信息
 *
 * 对应 Q_TASK 响应帧 Data 区中每 20 字节的一条任务记录。
 * 字段布局：[taskId 16B] [status 1B] [type 1B] [source 1B] [reserved 1B]
 *
 * taskId 是后端系统分配的 GUID（二进制形式），用于 DEL 命令删除指定任务。
 */
struct KeyTaskInfo {
    QByteArray taskId;   ///< 任务 ID，固定 16 字节原始二进制（GUID）
    quint8 status;       ///< 任务状态（协议定义，具体含义参见设备文档）
    quint8 type;         ///< 任务类型（协议定义）
    quint8 source;       ///< 任务来源（协议定义）
    quint8 reserved;     ///< 保留字段（协议预留，当前未使用）

    KeyTaskInfo() : status(0), type(0), source(0), reserved(0) {}

    /** @brief 将 taskId 转为大写十六进制字符串（空格分隔），用于日志和 UI 显示 */
    QString taskIdHex() const {
        return QString(taskId.toHex(' ').toUpper());
    }
};

// ============================================================
// KeySerialClient - X5 钥匙柜串口协议客户端
//
// 职责：封装 X5 协议的完整通讯流程，对外提供简洁的业务接口
//   connectPort()    → 打开串口 + SET_COM 握手
//   queryTasksAll()  → 发送 Q_TASK 查询钥匙上的任务列表
//   deleteTask()     → 发送 DEL 删除指定任务
//   disconnectPort() → 关闭串口 + 清理状态
//
// 内部状态机：
//   IDLE → [connectPort] → HANDSHAKE → [ACK] → SESSION_READY
//   → [KEY_PLACED] → STABILIZING → [timer] → KEY_STABLE
//   → [queryTasksAll/deleteTask] → IN_FLIGHT → [ACK/响应] → IDLE
//   → [KEY_REMOVED] → 重置所有状态
//
// 线程模型：
//   所有操作在主线程（Qt 事件循环）中执行，不涉及多线程。
//   QSerialPort 的 readyRead 信号驱动数据接收。
// ============================================================
class KeySerialClient : public QObject
{
    Q_OBJECT

public:
    explicit KeySerialClient(ISerialTransport *transport, QObject *parent = nullptr);
    ~KeySerialClient();

    // --- 公共接口 ---
    bool connectPort(const QString &portName, int baud = 115200);
    void disconnectPort();
    bool isConnected() const;

    void queryTasksAll();
    void requestTaskLog(const QByteArray &taskId16);
    void deleteTask(const QByteArray &taskId16);
    void deleteFirstTask();
    void sendInitPayload(const QByteArray &payload);
    void sendRfidPayload(const QByteArray &payload);
    void transferTicketJson(const QByteArray &jsonBytes,
                            quint8 stationId = 0x01,
                            int debugFrameChunkSize = 0);

    const QList<KeyTaskInfo> &tasks() const { return m_tasks; }
    bool hasVerifiedPort() const { return m_hasVerifiedPort; }
    QString verifiedPortName() const { return m_verifiedPortName; }
    QString currentPortName() const { return m_serial ? m_serial->portName() : QString(); }
    bool isKeyPresent() const { return m_keyPlaced; }
    bool isKeyStable() const { return m_keyStable; }
    bool isSessionReady() const { return m_sessionReady; }

    /**
     * @brief 设置当前操作ID（由 UI 层在用户点击按钮时调用）
     * @param opId 递增的操作ID，-1 表示无归属
     *
     * 该 opId 会附加到后续所有 logItem 信号中，
     * 直到下一次 setCurrentOpId 调用或 clearInFlight 重置。
     */
    void setCurrentOpId(int opId) { m_currentOpId = opId; }

    /**
     * @brief 设置当前站号（仅影响 DEL 命令的 Addr2，其他命令不受影响）
     * @param id 站号数值；\"001\"→1，\"002\"→2；0 或非法值自动回退 1
     *
     * Preconditions:
     *   - 无强制前提；可在 connectPort() 之前或之后任意时刻调用
     *   - 建议在 KeySessionService 构造时调用一次，之后随配置变更实时调用
     *
     * Side Effects:
     *   - 更新 m_stationId，影响后续所有 DEL 命令的 Addr2 字段
     *   - 输出一条 [CONF] 日志记录新值，供诊断（不产生 notice/timeout 信号）
     *   - SET_COM/Q_TASK/Q_KEYEQ 的 Addr2 始终为 0x0000，不受此方法影响
     *   - 不影响已 inFlight 的命令（下一次 DEL 才生效）
     *
     * Failure Strategy:
     *   - id=0 时自动修正为 1，不抛出异常；[CONF] 日志可见修正后的值
     *
     * SAFETY: 此字段填充规则与原产品行为对齐（2026-03-04 A/B 抓包验证）。
     *   不要将 stationId 用于 SET_COM/Q_TASK/Q_KEYEQ，否则偏离原产品行为。
     */
    void setStationId(quint16 id);

signals:
    /// 旧版文本日志信号（兼容保留，新代码应使用 logItemEmitted）
    void logLine(const QString &msg);
    /// 结构化日志信号，每条协议交互产生一个 LogItem，替代 logLine 用于表格显示
    void logItemEmitted(const LogItem &item);
    /// 串口连接状态变化（true=已连接，false=已断开）
    void connectedChanged(bool connected);
    /// 钥匙在位状态变化（true=KEY_PLACED，false=KEY_REMOVED）
    void keyPlacedChanged(bool placed);
    /// 钥匙稳定状态变化（true=稳定可操作，false=不稳定/未在位）
    void keyStableChanged(bool stable);
    /// Q_TASK 响应解析完成，携带最新任务列表
    void tasksUpdated(const QList<KeyTaskInfo> &tasks);
    /// 收到 ACK 帧，ackedCmd 为被确认的命令码
    void ackReceived(quint8 ackedCmd);
    /// 收到 NAK 帧，origCmd 为被拒绝的命令码，errCode 为错误码
    void nakReceived(quint8 origCmd, quint8 errCode);
    /// 收到并解析完整的任务回传日志（支持按 seq 累积多帧日志）
    void taskLogReady(const QVariantMap &payload);
    /// 传票发送进度（frameIndex 从 1 开始）
    void ticketTransferProgress(int frameIndex, int totalFrames, quint8 cmd);
    /// 传票发送完成
    void ticketTransferFinished();
    /// 传票发送失败
    void ticketTransferFailed(const QString &reason);
    /// 命令超时（重试耗尽），what 为超时描述（供 UI 显示）
    void timeoutOccurred(const QString &what);
    /// 用户友好通知（如"钥匙已就绪"），供 UI 层 statusBar 或 toast 显示
    void notice(const QString &what);

private slots:
    void onReadyRead();            ///< QSerialPort::readyRead 回调，驱动数据接收和拆包
    void onRetryTimeout();         ///< 重试定时器超时，重发或放弃
    void onSerialError(int errorCode, const QString &errorMessage);  ///< 串口硬错误处理
    void onKeyStableTimerTimeout(); ///< 钥匙稳定性窗口到期，判定是否稳定
    void onQTaskRecoveryTimeout();  ///< Q_TASK 软超时恢复定时器，延迟重试

private:
    // --- 协议层：帧构建、拆包、解析 ---
    static uint16_t calcCrc(const QByteArray &cmdPlusData);  ///< CRC16 计算（委托 KeyCrc16::calc）
    QByteArray buildFrame(quint8 cmd, const QByteArray &data,
                          const QByteArray &addr2);           ///< 组装完整协议帧
    int tryExtractOneFrame(QByteArray &buf, QByteArray &outFrame); ///< 从缓冲区提取一帧（含 resync）
    void handleFrame(const QByteArray &frame);                ///< 帧解析与命令分发

    // --- 发送与重试机制 ---
    void sendFrame(const QByteArray &frame, quint8 expectedAckCmd,
                   quint8 inFlightType);                      ///< 发送帧并启动重试定时器
    void startRetryTimer(quint8 expectedAckCmd);              ///< 启动重试定时器
    void stopRetryTimer();                                    ///< 停止重试定时器并清理
    void clearInFlight();                                     ///< 清除 inFlight 状态
    void sendFrameNoWait(const QByteArray &frame,
                         quint8 cmd,
                         quint16 lenField,
                         const QString &summary);             ///< 不进入 inFlight 的协议回复帧

    // --- 业务命令调度 ---
    void queryTasksAllInternal(bool fromRecovery, bool allowWhenUnknownKey = false); ///< Q_TASK 内部实现
    bool ensureBusinessReadyOrDefer(quint8 inFlightType, const QByteArray &taskId = QByteArray()); ///< 检查钥匙就绪或延迟命令
    void startKeyStabilityWindow(const QString &reason);      ///< 启动钥匙稳定性观察窗口
    void markNoiseDuringStabilizing(const QByteArray &noise, const QString &reason); ///< 标记稳定期间的噪声
    void tryDispatchDeferredCommand();                        ///< 尝试派发延迟的业务命令
    void sendSetComHandshake(const QString &reason);          ///< 发送 SET_COM 握手帧
    void clearTicketTransferState();                         ///< 清理传票发送状态
    void clearTaskLogState();                                ///< 清理回传日志状态
    void clearDataTransferState();                           ///< 清理 INIT/RFID 发送状态
    bool sendNextTicketFrame();                              ///< 发送下一帧传票
    bool sendNextDataFrame();                                ///< 发送下一帧 INIT/RFID 数据
    bool startDataTransfer(quint8 baseCmd,
                           const QByteArray &payload,
                           const QString &label);            ///< 启动 INIT/RFID 数据发送
    void sendTaskLogAck(quint16 frameSeq);                   ///< 确认一帧回传日志
    bool parseAndEmitTaskLogPayload(const QByteArray &payload,
                                    quint16 totalFrames,
                                    quint16 lastFrameSeq,
                                    quint16 lenField,
                                    const QByteArray &rawFrame); ///< 解析累计后的回传日志

    // --- 缓冲区管理与诊断 ---
    qint64 drainSerialInput(const QString &reason, bool clearRecvBuffer); ///< 排空串口接收缓冲区
    void drainBeforeBusinessSend(const QString &cmdName);     ///< 业务命令发送前排空
    void scheduleStartupProbeQTask();                         ///< 安排启动探测 Q_TASK
    void markQTaskFrameHeaderSeen(const QByteArray &incoming); ///< 标记 Q_TASK 响应帧头已出现

    // --- 端口验证与恢复 ---
    void markPortVerified(const QString &reason);             ///< 锁定已验证的串口
    bool isHardSerialError(int errorCode) const; ///< 判断是否为硬错误
    void tryReopenVerifiedPort(const QString &reason);        ///< 尝试重新打开已验证端口

    // --- 日志 ---
    void log(const QString &msg);
    void logHex(const QString &direction, const QByteArray &data);

    /** @brief 构造并 emit 一条结构化日志（同时兼容旧 logLine 信号） */
    void emitLog(LogDir dir, quint8 cmd, quint16 lenField,
                 const QString &summary, const QByteArray &hex = {},
                 bool crcOk = true, bool expertOnly = false);
    void emitNotice(const QString &what);

    // --- 成员变量 ---

    // 串口与接收缓冲
    ISerialTransport *m_serial;    ///< 串口传输实例（默认 QtSerialTransport，可替换为 Replay）
    QByteArray     m_recvBuffer;   ///< 接收缓冲区，累积 readyRead 数据直到拆出完整帧
    quint8         m_frameNo;      ///< 发送帧序号，每次 buildFrame 自增（0~255 循环）
    QList<KeyTaskInfo> m_tasks;    ///< 最近一次 Q_TASK 响应解析出的任务列表

    // 重试机制
    QTimer        *m_retryTimer;       ///< 重试定时器（singleShot），超时触发 onRetryTimeout
    QTimer        *m_keyStableTimer;   ///< 钥匙稳定性窗口定时器（singleShot）
    QTimer        *m_qtaskRecoveryTimer; ///< Q_TASK 软超时恢复定时器（singleShot）
    QByteArray     m_lastSentFrame;    ///< 最后发送的帧（用于重试时重发）
    quint8         m_expectedAckCmd;   ///< 当前期望的 ACK 命令码
    int            m_retryCount;       ///< 当前已重试次数（0~MAX_RETRIES）

    // 状态机
    bool           m_inFlight;             ///< true=有命令正在等待响应，禁止发送新命令
    quint8         m_inFlightType;         ///< 当前 inFlight 的命令类型（INFLIGHT_xxx）
    bool           m_pendingAfterDelQuery; ///< DEL 成功后是否自动发起 Q_TASK 验证
    bool           m_keyPlaced;            ///< 钥匙在位标志（KEY_EVENT 0x01 置 true，0x00 置 false）
    bool           m_keyStable;            ///< 钥匙稳定标志（稳定性窗口到期且无噪声时置 true）
    bool           m_sessionReady;         ///< 会话就绪标志（收到 SET_COM ACK 后置 true）

    // 钥匙稳定性检测
    QElapsedTimer  m_keyPlacedElapsed;     ///< 钥匙放上时刻计时器（用于稳定性窗口判定）
    QElapsedTimer  m_lastNoiseElapsed;     ///< 最后一次噪声时刻计时器（用于静默窗口判定）
    // 诊断与恢复
    bool           m_qtaskSawFrameHeader;  ///< Q_TASK inFlight 期间是否观察到 7E6C 帧头（用于超时诊断）
    bool           m_qtaskProbeInFlight;   ///< 当前 Q_TASK 是否为 startup probe（超时不报错）
    quint32        m_resyncDropBytes;      ///< 累计 resync 丢弃的脏数据字节数（诊断统计）
    quint32        m_noHeaderDropBytes;    ///< 累计无帧头丢弃的字节数（诊断统计）
    quint32        m_lenInvalidCount;      ///< 累计 Len 字段异常次数（诊断统计）
    quint32        m_crcFailCount;         ///< 累计 CRC 校验失败次数（诊断统计）

    // 延迟命令（钥匙未稳定时暂存，稳定后自动派发）
    quint8         m_deferredInFlightType; ///< 延迟的命令类型（INFLIGHT_xxx），NONE=无延迟
    QByteArray     m_deferredDeleteTaskId; ///< 延迟 DEL 命令的 taskId（16 字节）

    // Q_TASK 恢复重试
    int            m_qtaskRecoveryRound;   ///< Q_TASK 软超时已重试轮数（0~QTASK_RECOVERY_ROUNDS）

    // 端口管理
    int            m_currentBaud;          ///< 当前波特率（重连时使用）
    bool           m_reopenInProgress;     ///< 重连锁，防止并发重连
    int            m_reopenRetryCount;     ///< 连续重连尝试次数（达上限后停止，避免无限循环）
    static const int MAX_REOPEN_RETRIES = 5; ///< 最大连续重连次数
    QString        m_verifiedPortName;     ///< 已验证的串口名（收到合法协议帧后锁定）
    bool           m_hasVerifiedPort;      ///< 是否已锁定验证端口

    // 操作追踪
    int            m_currentOpId;          ///< 当前操作ID，由 UI 层设置，-1=无归属

    // 站号（DEL Addr2 来源）
    quint16        m_stationId;            ///< 当前站号，DEL Addr2 Lo=m_stationId&0xFF Hi=0x00
                                           ///< 默认 1，由 KeySessionService::setStationId() 注入

    // 传票发送状态
    QList<QByteArray> m_ticketFrames;      ///< 当前待发送的传票帧列表
    int               m_ticketFrameIndex;  ///< 当前发送中的帧索引

    // INIT/RFID 数据发送状态
    QList<QByteArray> m_dataFrames;        ///< 当前待发送的初始化/RFID 帧列表
    int               m_dataFrameIndex;    ///< 当前发送中的数据帧索引
    quint8            m_dataBaseCmd;       ///< 当前数据发送命令字（0x02 或 0x1A）
    QString           m_dataTransferLabel; ///< 当前数据发送名称

    // 回传日志状态
    QByteArray         m_pendingTaskLogTaskId; ///< 当前请求回传日志的任务ID（16B 原始值）
    quint16            m_pendingTaskLogFrames; ///< I_TASK_LOG 返回的总帧数
    quint16            m_taskLogExpectedSeq;   ///< 下一帧期望的日志序号（从 0 开始）
    QByteArray         m_taskLogPayloadBuffer; ///< 已累计的回传日志 payload（去掉每帧 seq）
};

#endif // KEYSERIALCLIENT_H
