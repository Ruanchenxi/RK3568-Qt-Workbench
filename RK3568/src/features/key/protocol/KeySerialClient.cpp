/**
 * @file KeySerialClient.cpp
 * @brief X5 智能钥匙柜串口协议客户端实现
 *
 * 模块定位：
 *   实现 X5 钥匙柜的完整串口通讯协议栈，包括：
 *   帧构建（buildFrame）→ 发送与重试（sendFrame / onRetryTimeout）
 *   → 数据接收（onReadyRead）→ 拆包与重同步（tryExtractOneFrame）
 *   → 帧解析与分发（handleFrame）→ 业务状态管理（钥匙稳定性/延迟命令）
 *
 * 协议帧格式：
 *   [7E 6C] [KeyId 1B] [FrameNo 1B] [Addr2 2B] [Len 2B LE] [Cmd 1B] [Data NB] [CRC16 2B BE]
 *   - CRC16 校验范围：Cmd(1) + Data(N)，高字节在前
 *   - Len 字段值 = 1(Cmd) + N(Data)
 *
 * 关键设计决策（不要修改协议逻辑）：
 *   1. resync 滑窗：CRC 失败或 Len 异常时仅丢弃 1 字节重新搜索帧头，
 *      避免跨帧误丢（比丢弃整帧更保守但更安全）
 *   2. KEY_STABLE 窗口：钥匙放上后需等待 600ms 无噪声才标记稳定，
 *      因为 USB 串口底座在钥匙接触瞬间会产生电气噪声
 *   3. drain 机制：业务命令发送前清空接收缓冲区，
 *      避免残留的异步事件帧被误认为业务响应
 *   4. startup probe：SET_COM ACK 后若无 KEY_EVENT，主动发 Q_TASK 探测，
 *      处理"钥匙已在位但未触发事件"的边界情况
 *
 * 依赖关系：
 *   - KeySerialClient.h（自身头文件，含 KeyProto 常量和 KeyTaskInfo 结构体）
 *   - KeyCrc16.h（CRC16 查表法计算）
 *   - QDateTime（日志时间戳）
 */
#include "KeySerialClient.h"
#include "KeyCrc16.h"
#include <QDateTime>
#include <QSerialPort>

using namespace KeyProto;

namespace {

quint16 readLe16(const QByteArray &bytes, int offset)
{
    const quint8 b0 = (offset >= 0 && offset < bytes.size())
            ? static_cast<quint8>(bytes.at(offset))
            : 0;
    const quint8 b1 = (offset + 1 >= 0 && offset + 1 < bytes.size())
            ? static_cast<quint8>(bytes.at(offset + 1))
            : 0;
    return static_cast<quint16>(b0)
            | (static_cast<quint16>(b1) << 8);
}

quint32 readLe32(const QByteArray &bytes, int offset)
{
    const quint8 b0 = (offset >= 0 && offset < bytes.size())
            ? static_cast<quint8>(bytes.at(offset))
            : 0;
    const quint8 b1 = (offset + 1 >= 0 && offset + 1 < bytes.size())
            ? static_cast<quint8>(bytes.at(offset + 1))
            : 0;
    const quint8 b2 = (offset + 2 >= 0 && offset + 2 < bytes.size())
            ? static_cast<quint8>(bytes.at(offset + 2))
            : 0;
    const quint8 b3 = (offset + 3 >= 0 && offset + 3 < bytes.size())
            ? static_cast<quint8>(bytes.at(offset + 3))
            : 0;
    return static_cast<quint32>(b0)
            | (static_cast<quint32>(b1) << 8)
            | (static_cast<quint32>(b2) << 16)
            | (static_cast<quint32>(b3) << 24);
}

QString rawTaskIdToDecimalString(const QByteArray &taskId16)
{
    if (taskId16.size() < 8) {
        return QString();
    }

    qulonglong value = 0;
    for (int i = 0; i < 8; ++i) {
        value |= (static_cast<qulonglong>(static_cast<quint8>(taskId16[i])) << (i * 8));
    }
    return QString::number(value);
}

QString compactHex(const QByteArray &bytes)
{
    return QString(bytes.toHex().toUpper());
}

QString bcd6ToDateTimeString(const QByteArray &bytes)
{
    if (bytes.size() < 6) {
        return QString();
    }

    auto part = [&bytes](int index) -> QString {
        const quint8 value = static_cast<quint8>(bytes[index]);
        return QStringLiteral("%1%2")
                .arg((value >> 4) & 0x0F)
                .arg(value & 0x0F);
    };

    return QStringLiteral("20%1-%2-%3 %4:%5:%6")
            .arg(part(0), part(1), part(2), part(3), part(4), part(5));
}

} // namespace

/**
 * @brief CRC16 计算委托（静态方法）
 * @param cmdPlusData Cmd(1) + Data(N) 拼接
 * @return 16 位 CRC 值
 * @note 委托给 KeyCrc16::calc()，此处仅做转发，便于单元测试 mock。
 */
uint16_t KeySerialClient::calcCrc(const QByteArray &cmdPlusData)
{
    return KeyCrc16::calc(cmdPlusData);
}

// ============================================================
// 构造 / 析构
// ============================================================

/**
 * @brief 构造函数
 * @param parent 父对象指针
 *
 * 初始化所有成员变量为安全默认值，创建串口和定时器实例，
 * 建立信号槽连接。所有定时器均为 singleShot 模式。
 *
 * 信号槽连接：
 *   - QSerialPort::readyRead → onReadyRead（数据接收驱动）
 *   - QSerialPort::errorOccurred → onSerialError（硬错误处理）
 *   - m_retryTimer::timeout → onRetryTimeout（重试/超时）
 *   - m_keyStableTimer::timeout → onKeyStableTimerTimeout（稳定性判定）
 *   - m_qtaskRecoveryTimer::timeout → onQTaskRecoveryTimeout（Q_TASK 恢复重试）
 */

KeySerialClient::KeySerialClient(ISerialTransport *transport, QObject *parent)
    : QObject(parent)
    , m_serial(transport)
    , m_frameNo(0)
    , m_retryTimer(new QTimer(this))
    , m_keyStableTimer(new QTimer(this))
    , m_qtaskRecoveryTimer(new QTimer(this))
    , m_expectedAckCmd(0)
    , m_retryCount(0)
    , m_inFlight(false)
    , m_inFlightType(INFLIGHT_NONE)
    , m_pendingAfterDelQuery(false)
    , m_keyPlaced(false)
    , m_keyStable(false)
    , m_sessionReady(false)
    , m_qtaskSawFrameHeader(false)
    , m_qtaskProbeInFlight(false)
    , m_resyncDropBytes(0)
    , m_noHeaderDropBytes(0)
    , m_lenInvalidCount(0)
    , m_crcFailCount(0)
    , m_deferredInFlightType(INFLIGHT_NONE)
    , m_qtaskRecoveryRound(0)
    , m_currentBaud(115200)
    , m_reopenInProgress(false)
    , m_reopenRetryCount(0)
    , m_verifiedPortName()
    , m_hasVerifiedPort(false)
    , m_currentOpId(-1)
    , m_stationId(1)   // 默认站号 1，由 KeySessionService 构造后通过 setStationId() 注入
    , m_ticketFrameIndex(-1)
    , m_pendingTaskLogTaskId()
    , m_pendingTaskLogFrames(0)
    , m_taskLogExpectedSeq(0)
    , m_taskLogPayloadBuffer()
{
    Q_ASSERT(m_serial != nullptr);
    connect(m_serial, &ISerialTransport::readyRead, this, &KeySerialClient::onReadyRead);
    connect(m_serial, &ISerialTransport::errorOccurred, this, &KeySerialClient::onSerialError);
    connect(m_retryTimer, &QTimer::timeout, this, &KeySerialClient::onRetryTimeout);
    connect(m_keyStableTimer, &QTimer::timeout, this, &KeySerialClient::onKeyStableTimerTimeout);
    connect(m_qtaskRecoveryTimer, &QTimer::timeout, this, &KeySerialClient::onQTaskRecoveryTimeout);
    m_retryTimer->setSingleShot(true);
    m_keyStableTimer->setSingleShot(true);
    m_qtaskRecoveryTimer->setSingleShot(true);
}

KeySerialClient::~KeySerialClient()
{
    if (m_serial->isOpen()) {
        m_serial->close();
    }
}

// ============================================================
// 公共接口：连接 / 断开
// ============================================================

/**
 * @brief 打开串口并发起 SET_COM 握手
 * @param portName 串口设备名（如 "COM3" 或 "/dev/ttyUSB0"）
 * @param baud 波特率，默认 115200（X5 底座固定此速率）
 * @return true=串口打开成功且握手已发出；false=打开失败
 *
 * 执行流程：
 *   1. 若已打开则先 disconnectPort() 确保干净状态
 *   2. 配置串口参数：8N1 无流控（X5 底座硬件要求）
 *   3. 打开串口，置位 DTR/RTS（USB 串口底座需要这两个信号才能通讯）
 *   4. 重置所有状态机变量和诊断计数器
 *   5. 发送 SET_COM(0x0F) 握手帧，等待设备 ACK
 *
 * @note DTR/RTS 置位是 USB 串口底座的硬件要求，不置位会导致底座不响应。
 * @note 连接成功后状态机进入 HANDSHAKE 阶段，等待 SET_COM ACK。
 */
bool KeySerialClient::connectPort(const QString &portName, int baud)
{
    if (m_serial->isOpen()) {
        disconnectPort();
    }
    m_currentBaud = baud;

    m_serial->setPortName(portName);

    if (!m_serial->open(baud)) {
        log(QString("[ERROR] 无法打开 %1: %2").arg(portName, m_serial->errorString()));
        emitLog(LogDir::ERROR, 0, 0,
                QString("无法打开 %1: %2").arg(portName, m_serial->errorString()));
        return false;
    }

    // DTR/RTS 必须置位（USB串口/底座需要）
    m_serial->setDataTerminalReady(true);
    m_serial->setRequestToSend(true);
    log(QString("[DTR/RTS] DTR=%1 RTS=%2")
            .arg(m_serial->isDataTerminalReady() ? "ON" : "OFF")
            .arg(m_serial->isRequestToSend() ? "ON" : "OFF"));

    m_recvBuffer.clear();
    m_frameNo = 0;
    m_tasks.clear();
    m_keyPlaced = false;
    m_keyStable = false;
    m_sessionReady = false;
    m_deferredInFlightType = INFLIGHT_NONE;
    m_deferredDeleteTaskId.clear();
    clearTicketTransferState();
    clearTaskLogState();
    m_qtaskRecoveryRound = 0;
    m_keyPlacedElapsed.invalidate();
    m_lastNoiseElapsed.invalidate();
    m_qtaskSawFrameHeader = false;
    m_qtaskProbeInFlight = false;
    m_resyncDropBytes = 0;
    m_noHeaderDropBytes = 0;
    m_lenInvalidCount = 0;
    m_crcFailCount = 0;
    m_keyStableTimer->stop();
    m_qtaskRecoveryTimer->stop();
    m_reopenRetryCount = 0;

    log(QString("[CONNECTED] %1 已打开 (%2/8N1)").arg(portName).arg(baud));
    emitLog(LogDir::EVENT, 0, 0,
            QString("串口已连接: %1 (%2/8N1)").arg(portName).arg(baud));
    emit connectedChanged(true);
    emit keyStableChanged(false);

    // 立即发送 SET_COM 握手
    sendSetComHandshake("connect");

    return true;
}

/**
 * @brief 断开串口连接并重置所有状态
 *
 * 执行流程：
 *   1. 停止所有定时器（重试、稳定性、恢复）
 *   2. 清除 inFlight 状态
 *   3. 关闭串口
 *   4. 重置所有状态机变量、缓冲区、诊断计数器
 *   5. 通知 UI 层连接已断开、钥匙不可用
 *
 * @note 此方法可安全重复调用（幂等），串口未打开时 close() 不会报错。
 */
void KeySerialClient::disconnectPort()
{
    stopRetryTimer();
    clearInFlight();
    m_keyStableTimer->stop();
    m_qtaskRecoveryTimer->stop();
    if (m_serial->isOpen()) {
        m_serial->close();
    }
    m_recvBuffer.clear();
    m_tasks.clear();
    m_frameNo = 0;
    m_pendingAfterDelQuery = false;
    m_keyPlaced = false;
    m_keyStable = false;
    m_sessionReady = false;
    m_deferredInFlightType = INFLIGHT_NONE;
    m_deferredDeleteTaskId.clear();
    clearTicketTransferState();
    clearTaskLogState();
    m_qtaskRecoveryRound = 0;
    m_keyPlacedElapsed.invalidate();
    m_lastNoiseElapsed.invalidate();
    m_qtaskSawFrameHeader = false;
    m_qtaskProbeInFlight = false;
    m_resyncDropBytes = 0;
    m_noHeaderDropBytes = 0;
    m_lenInvalidCount = 0;
    m_crcFailCount = 0;
    log("[DISCONNECTED] 串口已关闭");
    emitLog(LogDir::EVENT, 0, 0, "串口已断开");
    emit connectedChanged(false);
    emit keyStableChanged(false);
}

/** @brief 查询串口是否处于打开状态 */
bool KeySerialClient::isConnected() const
{
    return m_serial->isOpen();
}

// ============================================================
// 公共接口：查询 / 删除
// ============================================================

/**
 * @brief 查询钥匙上的全部任务（公共接口）
 *
 * 委托给 queryTasksAllInternal()，fromRecovery=false 表示用户主动触发，
 * allowWhenUnknownKey=false 表示必须钥匙在位且稳定才允许发送。
 */
void KeySerialClient::queryTasksAll()
{
    queryTasksAllInternal(false, false);
}

void KeySerialClient::requestTaskLog(const QByteArray &taskId16)
{
    if (taskId16.size() != TASK_ID_LEN) {
        emitLog(LogDir::WARN, CMD_I_TASK_LOG, 0,
                QString("requestTaskLog: taskId长度异常(%1!=16), 已拒绝发送").arg(taskId16.size()));
        return;
    }
    if (!m_serial->isOpen()) {
        emitNotice("I_TASK_LOG 失败：串口未连接");
        return;
    }
    if (m_inFlight) {
        emitNotice("I_TASK_LOG 失败：当前有命令在途");
        return;
    }
    if (!m_keyPlaced || !m_keyStable || !m_sessionReady) {
        emitNotice("I_TASK_LOG 失败：钥匙未就绪");
        return;
    }

    clearTaskLogState();
    m_pendingTaskLogTaskId = taskId16;

    log(QString("发送 I_TASK_LOG(0x05) taskId: %1")
            .arg(QString(taskId16.toHex(' ').toUpper())));
    drainBeforeBusinessSend("I_TASK_LOG");

    QByteArray addr2;
    addr2.append(static_cast<char>(m_stationId & 0xFF));
    addr2.append(static_cast<char>((m_stationId >> 8) & 0xFF));
    const QByteArray frame = buildFrame(CMD_I_TASK_LOG, taskId16, addr2);
    sendFrame(frame, CMD_I_TASK_LOG, INFLIGHT_TASKLOG);
}

/**
 * @brief Q_TASK 查询内部实现
 * @param fromRecovery true=由软超时恢复定时器触发的重试，false=用户主动或首次调用
 * @param allowWhenUnknownKey true=跳过钥匙在位检查（用于 startup probe 场景）
 *
 * 前置检查：
 *   1. 串口必须已打开
 *   2. 不能有 inFlight 命令（单命令串行模型）
 *   3. 钥匙必须在位且稳定（除非 allowWhenUnknownKey=true）
 *
 * 协议细节：
 *   - Data 区固定 16 字节 0xFF（广播查询所有任务）
 *   - Addr2 使用默认值 0x0000
 *   - 期望响应：CMD_Q_TASK(0x04) 回包（非 ACK）
 *
 * @note drain 机制：发送前清空接收缓冲区，避免残留异步事件帧干扰响应匹配。
 * @note fromRecovery=true 时不重置恢复轮数计数器，允许继续累计重试。
 */
void KeySerialClient::queryTasksAllInternal(bool fromRecovery, bool allowWhenUnknownKey)
{
    if (!m_serial->isOpen()) {
        log("[ERROR] 串口未打开");
        return;
    }
    if (m_inFlight) {
        log("[BUSY] 上一个请求尚未完成，请等待响应或超时");
        return;
    }
    if (!allowWhenUnknownKey && !ensureBusinessReadyOrDefer(INFLIGHT_QTASK)) {
        if (!fromRecovery && m_keyPlaced) {
            emitNotice("Q_TASK 已延迟，等待钥匙稳定/握手完成后自动发送");
        }
        return;
    }
    if (!fromRecovery) {
        m_qtaskRecoveryRound = 0;
        m_qtaskRecoveryTimer->stop();
    }

    log("发送 Q_TASK(0x04) 查询全部任务...");
    drainBeforeBusinessSend("Q_TASK");
    if (allowWhenUnknownKey) {
        emitLog(LogDir::EVENT, CMD_Q_TASK, 0,
                "Startup probe Q_TASK send (skip keyPresent gate)",
                {}, true, true);
    }
    m_qtaskSawFrameHeader = false;
    m_qtaskProbeInFlight = allowWhenUnknownKey;
    QByteArray data(16, static_cast<char>(0xFF));
    QByteArray addr2;
    addr2.append(static_cast<char>(KeyProtocol::Addr2DefaultLo));
    addr2.append(static_cast<char>(KeyProtocol::Addr2DefaultHi));
    QByteArray frame = buildFrame(CMD_Q_TASK, data, addr2);
    sendFrame(frame, CMD_Q_TASK, INFLIGHT_QTASK);
}

/**
 * @brief 删除钥匙上的指定任务
 * @param taskId16 任务 ID，固定 16 字节（GUID 二进制形式）
 *
 * 前置检查：
 *   1. taskId 必须恰好 16 字节（协议硬性要求，否则设备返回 NAK 0x04）
 *   2. 串口必须已打开
 *   3. 不能有 inFlight 命令
 *   4. 钥匙必须在位且稳定（否则延迟到稳定后自动发送）
 *
 * 协议细节：
 *   - Addr2 = 当前站号 m_stationId（小端 Lo Hi），由 KeySessionService::setStationId() 注入
 *     例：站号1 → Addr2 [01 00]；站号2 → [02 00]（2026-03-04 A/B 抓包验证，不是固定 0x0001）
 *   - 期望响应：ACK(0x5A)，ACK.data[0] == CMD_DEL(0x06)
 *   - DEL 成功后自动发起 Q_TASK 验证（m_pendingAfterDelQuery 标志）
 *
 * @note drain 机制同 queryTasksAllInternal。
 */
void KeySerialClient::deleteTask(const QByteArray &taskId16)
{
    // 协议约定 taskId 固定 16 字节，长度不符会导致帧格式错误，设备返回 NAK(0x04)。
    if (taskId16.size() != 16) {
        emitLog(LogDir::WARN, CMD_DEL, 0,
                QString("deleteTask: taskId长度异常(%1!=16), 已拒绝发送")
                    .arg(taskId16.size()));
        return;
    }
    if (!m_serial->isOpen()) {
        log("[ERROR] 串口未打开");
        return;
    }
    if (m_inFlight) {
        log("[BUSY] 上一个请求尚未完成，请等待响应或超时");
        return;
    }
    if (!ensureBusinessReadyOrDefer(INFLIGHT_DEL, taskId16)) {
        if (m_keyPlaced) {
            emitNotice("DEL 已延迟，等待钥匙稳定/握手完成后自动发送");
        }
        return;
    }

    log(QString("发送 DEL(0x06) stationId=%1 Addr2=%2 00 taskId: %3")
            .arg(m_stationId)
            .arg(m_stationId & 0xFF, 2, 16, QChar('0'))
            .arg(QString(taskId16.toHex(' ').toUpper())));
    drainBeforeBusinessSend("DEL");

    m_pendingAfterDelQuery = true;
    // Addr2 = 当前站号（小端 Lo Hi）
    // SET_COM/Q_TASK 始终用 0x0000，只有 DEL 用 stationId（2026-03-04 A/B 抓包验证）
    QByteArray addr2;
    addr2.append(static_cast<char>(m_stationId & 0xFF));
    addr2.append(static_cast<char>((m_stationId >> 8) & 0xFF));
    QByteArray frame = buildFrame(CMD_DEL, taskId16, addr2);
    sendFrame(frame, CMD_ACK, INFLIGHT_DEL);
}

/**
 * @brief 设置当前站号（影响 DEL 命令的 Addr2）
 * @param id 站号数值（1-255），例如系统设置 "001" → 1，"002" → 2；0 回退为 1
 *
 * 调用关系：KeySessionService 在构造后立即调用，并在 ConfigManager::configChanged
 * 信号触发时再次调用，确保系统设置界面"当前站号"修改后实时生效。
 *
 * SAFETY: 此字段填充规则与原产品行为完全对齐（2026-03-04 A/B 抓包验证）。
 *   - DEL 发包 Addr2 Lo = stationId & 0xFF，Hi = 0x00（小端）
 *   - 不得将 m_stationId 用于 SET_COM/Q_TASK/Q_KEYEQ，这些命令 Addr2 固定 0x0000
 *   - 此规则改动会导致 DEL 命令发往错误站点，设备返回 NAK(0x08)，不可随意修改
 */
void KeySerialClient::setStationId(quint16 id)
{
    m_stationId = (id > 0) ? id : 1;  // 防御：站号不允许为 0
    log(QString("[CONF] stationId 更新为 %1（DEL Addr2: %2 00）")
            .arg(m_stationId)
            .arg(m_stationId & 0xFF, 2, 16, QChar('0')));
}

/**
 * @brief 删除任务列表中的第一条任务（便捷方法）
 *
 * 从最近一次 Q_TASK 响应缓存的 m_tasks 列表中取第一条的 taskId，
 * 委托给 deleteTask() 执行。调用前需确保已执行过 queryTasksAll()。
 */
void KeySerialClient::deleteFirstTask()
{
    if (m_tasks.isEmpty()) {
        log("[ERROR] 无可删除的任务（请先查询）");
        return;
    }
    deleteTask(m_tasks.first().taskId);
}

void KeySerialClient::clearTicketTransferState()
{
    m_ticketFrames.clear();
    m_ticketFrameIndex = -1;
}

void KeySerialClient::clearTaskLogState()
{
    m_pendingTaskLogTaskId.clear();
    m_pendingTaskLogFrames = 0;
    m_taskLogExpectedSeq = 0;
    m_taskLogPayloadBuffer.clear();
}

void KeySerialClient::sendTaskLogAck(quint16 frameSeq)
{
    QByteArray ackData;
    ackData.append(static_cast<char>(CMD_UP_TASK_LOG));
    ackData.append(static_cast<char>(frameSeq & 0xFF));
    ackData.append(static_cast<char>((frameSeq >> 8) & 0xFF));

    QByteArray addr2;
    addr2.append(static_cast<char>(KeyProtocol::Addr2DefaultLo));
    addr2.append(static_cast<char>(KeyProtocol::Addr2DefaultHi));

    const QByteArray ackFrame = buildFrame(CMD_ACK, ackData, addr2);
    sendFrameNoWait(ackFrame,
                    CMD_ACK,
                    static_cast<quint16>(1 + ackData.size()),
                    QString("ACK for UP_TASK_LOG frame %1").arg(frameSeq));
}

bool KeySerialClient::parseAndEmitTaskLogPayload(const QByteArray &payload,
                                                 quint16 totalFrames,
                                                 quint16 lastFrameSeq,
                                                 quint16 lenField,
                                                 const QByteArray &rawFrame)
{
    if (payload.size() < 28) {
        emitLog(LogDir::ERROR, CMD_UP_TASK_LOG, lenField,
                "UP_TASK_LOG file payload too short", rawFrame);
        return false;
    }

    const quint32 fileSize = readLe32(payload, 0);
    if (fileSize < 28 || static_cast<int>(fileSize) > payload.size()) {
        emitLog(LogDir::ERROR, CMD_UP_TASK_LOG, lenField,
                QString("UP_TASK_LOG invalid fileSize=%1 payload=%2")
                    .arg(fileSize).arg(payload.size()),
                rawFrame);
        return false;
    }

    int offset = 4;
    const QByteArray versionBytes = payload.mid(offset, 2);
    offset += 2;
    offset += 2; // reserved
    const QByteArray taskIdRaw = payload.mid(offset, TASK_ID_LEN);
    offset += TASK_ID_LEN;
    if (!m_pendingTaskLogTaskId.isEmpty() && taskIdRaw != m_pendingTaskLogTaskId) {
        emitLog(LogDir::WARN, CMD_UP_TASK_LOG, lenField,
                "UP_TASK_LOG taskId mismatch with pending request",
                {}, true, true);
    }
    const int stationNo = static_cast<int>(static_cast<quint8>(payload[offset++]));
    const int taskAttr = static_cast<int>(static_cast<quint8>(payload[offset++]));
    const int steps = static_cast<int>(readLe16(payload, offset));
    offset += 2;

    const int itemsBytes = static_cast<int>(fileSize) - offset;
    if (itemsBytes < 0 || (itemsBytes % 16) != 0) {
        emitLog(LogDir::ERROR, CMD_UP_TASK_LOG, lenField,
                QString("UP_TASK_LOG invalid item bytes=%1").arg(itemsBytes),
                rawFrame);
        return false;
    }

    QVariantList items;
    for (int itemOffset = offset; itemOffset < static_cast<int>(fileSize); itemOffset += 16) {
        QVariantMap item;
        item.insert("serialNumber", static_cast<int>(readLe16(payload, itemOffset)));
        item.insert("opStatus", static_cast<int>(static_cast<quint8>(payload[itemOffset + 2])));
        item.insert("rfid", compactHex(payload.mid(itemOffset + 3, 5)));
        item.insert("opTime", bcd6ToDateTimeString(payload.mid(itemOffset + 8, 6)));
        item.insert("opType", static_cast<int>(static_cast<quint8>(payload[itemOffset + 14])));
        items.append(item);
    }

    QVariantMap eventPayload;
    eventPayload.insert("taskId", rawTaskIdToDecimalString(taskIdRaw));
    eventPayload.insert("taskIdRaw", taskIdRaw);
    eventPayload.insert("stationNo", stationNo);
    eventPayload.insert("taskAttr", taskAttr);
    eventPayload.insert("steps", steps);
    eventPayload.insert("frameSeq", static_cast<int>(lastFrameSeq));
    eventPayload.insert("totalFrames", static_cast<int>(totalFrames));
    eventPayload.insert("version", QString(versionBytes.toHex().toUpper()));
    eventPayload.insert("items", items);

    emitLog(LogDir::EVENT, CMD_UP_TASK_LOG, lenField,
            QString("UP_TASK_LOG parsed: taskId=%1 items=%2 frames=%3")
                .arg(eventPayload.value("taskId").toString())
                .arg(items.size())
                .arg(totalFrames),
            {}, true, true);
    emit taskLogReady(eventPayload);
    return true;
}

bool KeySerialClient::sendNextTicketFrame()
{
    const int nextIndex = m_ticketFrameIndex + 1;
    if (nextIndex < 0 || nextIndex >= m_ticketFrames.size()) {
        return false;
    }

    m_ticketFrameIndex = nextIndex;
    const QByteArray &frame = m_ticketFrames[m_ticketFrameIndex];
    const quint8 cmd = (frame.size() > OFF_CMD) ? static_cast<quint8>(frame[OFF_CMD]) : CMD_TICKET;
    const quint16 lenField = (frame.size() > OFF_LEN + 1)
            ? static_cast<quint16>(static_cast<quint8>(frame[OFF_LEN]))
              | (static_cast<quint16>(static_cast<quint8>(frame[OFF_LEN + 1])) << 8)
            : 0;
    const int payloadChunkLen = qMax(0, (int)lenField - 3);

    emitNotice(QString("发送传票帧 %1/%2 Cmd=0x%3 Payload=%4B")
                   .arg(m_ticketFrameIndex + 1)
                   .arg(m_ticketFrames.size())
                   .arg(cmd, 2, 16, QChar('0'))
                   .arg(payloadChunkLen));
    emit ticketTransferProgress(m_ticketFrameIndex + 1, m_ticketFrames.size(), cmd);
    emitLog(LogDir::EVENT, cmd, lenField,
            QString("TICKET frame %1/%2 send")
                .arg(m_ticketFrameIndex + 1)
                .arg(m_ticketFrames.size()),
            frame, true, true);
    sendFrame(frame, cmd, INFLIGHT_TICKET);
    return true;
}

void KeySerialClient::transferTicketJson(const QByteArray &jsonBytes,
                                         quint8 stationId,
                                         int debugFrameChunkSize)
{
    if (!m_serial->isOpen()) {
        emitNotice("传票发送失败：串口未连接");
        emit ticketTransferFailed("串口未连接");
        return;
    }
    if (m_inFlight) {
        emitNotice("传票发送失败：当前有命令在途");
        emit ticketTransferFailed("当前有命令在途");
        return;
    }
    if (!m_keyPlaced || !m_keyStable || !m_sessionReady) {
        emitNotice("传票发送失败：钥匙未就绪，请先完成握手并保持钥匙稳定");
        emit ticketTransferFailed("钥匙未就绪");
        return;
    }

    QJsonObject ticketObj;
    QString errorMsg;
    if (!TicketPayloadEncoder::extractTicketObject(jsonBytes, ticketObj, errorMsg)) {
        emitLog(LogDir::ERROR, 0, 0,
                QString("Ticket JSON parse failed: %1").arg(errorMsg));
        emitNotice(QString("传票发送失败：%1").arg(errorMsg));
        emit ticketTransferFailed(QString("JSON 解析失败：%1").arg(errorMsg));
        return;
    }

    const TicketPayloadEncoder::EncodeResult enc =
        TicketPayloadEncoder::encode(ticketObj, stationId,
                                     TicketPayloadEncoder::StringEncoding::GBK);
    for (const QString &line : enc.fieldLog) {
        emitLog(LogDir::RAW, 0, 0, line, {}, true, true);
    }
    if (!enc.ok) {
        emitLog(LogDir::ERROR, 0, 0,
                QString("Ticket payload encode failed: %1").arg(enc.errorMsg));
        emitNotice(QString("传票编码失败：%1").arg(enc.errorMsg));
        emit ticketTransferFailed(QString("payload 编码失败：%1").arg(enc.errorMsg));
        return;
    }

    TicketFrameBuilder builder(stationId, 0x00, 0x00);
    if (debugFrameChunkSize > 0) {
        builder.setMaxChunkSize(debugFrameChunkSize);
        emitLog(LogDir::EVENT, 0, 0,
                QString("Debug frame chunk size enabled: %1B")
                    .arg(debugFrameChunkSize),
                {}, true, true);
    }
    const QList<QByteArray> frames = builder.buildFrames(enc.payload);
    if (frames.isEmpty()) {
        emitLog(LogDir::ERROR, 0, 0, "Ticket frame build failed: no frames");
        emitNotice("传票封帧失败：未生成任何帧");
        emit ticketTransferFailed("封帧失败：未生成任何帧");
        return;
    }

    clearTicketTransferState();
    m_ticketFrames = frames;
    emitNotice(QString("传票已编码：payload=%1B frameCount=%2")
                   .arg(enc.payload.size())
                   .arg(m_ticketFrames.size()));
    if (!sendNextTicketFrame()) {
        emitLog(LogDir::ERROR, 0, 0, "Ticket send failed: cannot dispatch first frame");
        emitNotice("传票发送失败：首帧派发失败");
        emit ticketTransferFailed("首帧派发失败");
        clearTicketTransferState();
    }
}

// ============================================================
// 组帧
// ============================================================

/**
 * @brief 按 X5 协议格式组装一帧完整数据
 * @param cmd 命令码（如 CMD_SET_COM=0x0F, CMD_Q_TASK=0x04, CMD_DEL=0x06）
 * @param data 数据区内容（长度由具体命令决定）
 * @param addr2 2 字节地址扩展（SET_COM/Q_TASK 用 0x0000，DEL 用 0x0001）
 * @return 完整帧字节数组，可直接写入串口
 *
 * 帧结构（按字节顺序）：
 *   [0] 0x7E        帧头第一字节
 *   [1] 0x6C        帧头第二字节
 *   [2] KeyId       钥匙编号（固定 0xFF 广播）
 *   [3] FrameNo     帧序号（每次调用自增，0~255 循环）
 *   [4-5] Addr2     地址扩展（2 字节，原样写入）
 *   [6-7] Len       数据长度（小端：低字节在前），值 = 1(Cmd) + N(Data)
 *   [8] Cmd         命令码
 *   [9..] Data      数据区
 *   [末2字节] CRC16  校验（高字节在前），校验范围 = Cmd + Data
 *
 * @note m_frameNo 在每次调用时自增，用于请求-响应匹配（设备回包会回显此值）。
 */
QByteArray KeySerialClient::buildFrame(quint8 cmd, const QByteArray &data,
                                       const QByteArray &addr2)
{
    QByteArray frame;
    int dataLen = data.size();
    quint16 len = static_cast<quint16>(1 + dataLen); // Cmd(1) + Data(N)

    // Header
    frame.append(static_cast<char>(HEADER_0));
    frame.append(static_cast<char>(HEADER_1));

    // KeyId
    frame.append(static_cast<char>(REQ_KEYID));

    // FrameNo（自增）
    frame.append(static_cast<char>(m_frameNo++));

    // addr2（2字节）
    if (addr2.size() >= 2) {
        frame.append(addr2[0]);
        frame.append(addr2[1]);
    } else {
        frame.append(2, '\x00');
    }

    // Len（小端：低字节在前）
    frame.append(static_cast<char>(len & 0xFF));
    frame.append(static_cast<char>((len >> 8) & 0xFF));

    // Cmd
    frame.append(static_cast<char>(cmd));

    // Data
    frame.append(data);

    // CRC（只算 Cmd+Data，高字节在前）
    QByteArray cmdPlusData;
    cmdPlusData.append(static_cast<char>(cmd));
    cmdPlusData.append(data);
    uint16_t crc = calcCrc(cmdPlusData);
    frame.append(static_cast<char>((crc >> 8) & 0xFF));  // CRC hi
    frame.append(static_cast<char>(crc & 0xFF));          // CRC lo

    return frame;
}

// ============================================================
// 拆包：从缓冲区提取一帧完整数据
// 返回值：1=成功提取一帧, 0=数据不足需等待, -1=未使用（内部循环处理）
//
// 核心设计：resync 滑窗机制
//   当遇到 CRC 失败或 Len 异常时，仅丢弃 1 字节后重新搜索帧头，
//   而非丢弃整个疑似帧。这种保守策略避免了跨帧粘包时误丢有效数据，
//   代价是多几次循环迭代（可接受，因为串口速率远低于 CPU 处理速度）。
// ============================================================

/**
 * @brief 从接收缓冲区中尝试提取一帧完整协议数据
 * @param buf [in/out] 接收缓冲区，成功时会移除已提取的帧数据
 * @param outFrame [out] 提取出的完整帧（含帧头和 CRC）
 * @return 1=成功提取一帧；0=数据不足，需等待更多数据到达
 *
 * 提取流程（while 循环，直到成功或数据不足）：
 *   1. 线性搜索帧头 7E 6C（不假设帧头在 buf[0]，兼容脏数据前缀）
 *   2. 丢弃帧头前的脏数据（resync 日志 + 噪声标记）
 *   3. 读取 Len 字段（小端），计算期望帧总长
 *   4. Len 合法性检查（1 ≤ Len ≤ 1+MAX_DATA_LEN），异常则滑窗 1 字节
 *   5. 数据不足则返回 0 等待
 *   6. CRC16 校验（Cmd+Data），失败则滑窗 1 字节
 *   7. 校验通过，提取完整帧并从 buf 中移除
 *
 * @note 滑窗策略：步骤 4/6 失败时仅丢弃 1 字节（而非整帧），
 *       然后 continue 重新搜索帧头。这是刻意的保守设计，见文件头注释。
 */

int KeySerialClient::tryExtractOneFrame(QByteArray &buf, QByteArray &outFrame)
{
    while (true) {
        const int recvBufBefore = buf.size();
        // 1. 找帧头 7E 6C
        int headerPos = -1;
        for (int i = 0; i <= buf.size() - 2; ++i) {
            if (static_cast<quint8>(buf[i]) == HEADER_0 &&
                static_cast<quint8>(buf[i + 1]) == HEADER_1) {
                headerPos = i;
                break;
            }
        }

        if (headerPos < 0) {
            if (buf.size() > 1) {
                const int dropped = buf.size() - 1;
                markNoiseDuringStabilizing(buf.left(dropped), "no header");
                m_noHeaderDropBytes += static_cast<quint32>(dropped);
                // 简洁摘要（非专家模式可见）
                emitLog(LogDir::WARN, 0, 0,
                        QString("RESYNC no header, drop %1 bytes").arg(dropped),
                        buf.left(dropped), true, false);
                // 详细诊断（仅专家模式）
                emitLog(LogDir::WARN, 0, 0,
                        QString("  detail: recvBufBefore=%1 recvBufAfter=1").arg(recvBufBefore),
                        {}, true, true);
                buf = buf.right(1); // 仅保留最后一个字节，兼容 0x7E 跨 chunk
            }
            return 0;
        }

        // 丢弃帧头前的脏数据
        if (headerPos > 0) {
            markNoiseDuringStabilizing(buf.left(headerPos), "resync drop");
            m_resyncDropBytes += static_cast<quint32>(headerPos);
            log(QString("  [RESYNC] 丢弃 %1 字节脏数据").arg(headerPos));
            // 简洁摘要（非专家模式可见）
            emitLog(LogDir::WARN, 0, 0,
                    QString("RESYNC drop %1 bytes").arg(headerPos),
                    buf.left(headerPos), true, false);
            // 详细诊断（仅专家模式）
            emitLog(LogDir::WARN, 0, 0,
                    QString("  detail: headerShift recvBufBefore=%1").arg(recvBufBefore),
                    {}, true, true);
            buf.remove(0, headerPos);
        }

        // 2. 检查是否够读 Len 字段
        if (buf.size() < OFF_LEN + 2) return 0;

        // 3. 读 Len（小端）
        quint16 lenField = static_cast<quint8>(buf[OFF_LEN])
                         | (static_cast<quint8>(buf[OFF_LEN + 1]) << 8);
        int dataLen = lenField - 1;
        int expectedTotal = FRAME_OVERHEAD + dataLen;

        // Len 合法性检查
        if (lenField < 1 || lenField > (1 + MAX_DATA_LEN)) {
            m_lenInvalidCount++;
            markNoiseDuringStabilizing(buf.left(1), "len invalid");
            buf.remove(0, 1); // 滑窗重同步，避免错过后续真实帧头
            const int nextHeaderIndex = buf.indexOf(QByteArray::fromHex("7E6C"));
            log(QString("  [ERROR] Len=%1 异常，滑窗丢弃1字节重同步").arg(lenField));
            // 简洁摘要
            emitLog(LogDir::ERROR, 0, lenField,
                    QString("RESYNC Len invalid (%1), drop 1 byte").arg(lenField),
                    {}, false, false);
            // 详细诊断（仅专家模式）
            emitLog(LogDir::ERROR, 0, lenField,
                    QString("  detail: LEN_INVALID nextHdr=%1 total=%2 count=%3 bufNow=%4")
                        .arg(nextHeaderIndex).arg(expectedTotal)
                        .arg(m_lenInvalidCount).arg(buf.size()),
                    {}, false, true);
            continue;
        }

        // 4. 数据不足，等下次
        if (buf.size() < expectedTotal) return 0;

        // 5. CRC 校验
        QByteArray cmdPlusData = buf.mid(OFF_CMD, lenField);
        uint16_t calcedCrc = calcCrc(cmdPlusData);
        int crcOffset = OFF_DATA + dataLen;
        quint8 crcHi = static_cast<quint8>(buf[crcOffset]);
        quint8 crcLo = static_cast<quint8>(buf[crcOffset + 1]);
        uint16_t recvCrc = (static_cast<uint16_t>(crcHi) << 8) | crcLo;

        if (calcedCrc != recvCrc) {
            m_crcFailCount++;
            markNoiseDuringStabilizing(buf.left(expectedTotal), "crc fail");
            int dumpLen = qMin(buf.size(), 32);
            QString dumpHex = buf.left(dumpLen).toHex(' ').toUpper();
            log(QString("  [CRC FAIL] calc=0x%1 recv=0x%2 dump: %3")
                    .arg(calcedCrc, 4, 16, QChar('0'))
                    .arg(recvCrc, 4, 16, QChar('0'))
                    .arg(dumpHex));
            buf.remove(0, 1); // 滑窗重同步，避免跨帧误丢
            const int nextHeaderIndex = buf.indexOf(QByteArray::fromHex("7E6C"));
            // 简洁摘要（calc/recv 对调试最有用，保留；其余移入 detail）
            emitLog(LogDir::ERROR, 0, lenField,
                    QString("RESYNC CRC fail #%1: calc=0x%2 recv=0x%3")
                        .arg(m_crcFailCount)
                        .arg(calcedCrc, 4, 16, QChar('0'))
                        .arg(recvCrc, 4, 16, QChar('0')),
                    buf.left(qMin(buf.size(), expectedTotal)), false, false);
            // 详细诊断（仅专家模式）
            emitLog(LogDir::ERROR, 0, lenField,
                    QString("  detail: CRC_FAIL nextHdr=%1 Len=%2 total=%3")
                        .arg(nextHeaderIndex).arg(lenField).arg(expectedTotal),
                    {}, false, true);
            continue;
        }

        // 6. 提取完整帧
        outFrame = buf.left(expectedTotal);
        buf.remove(0, expectedTotal);
        return 1;
    }
}

// ============================================================
// 帧解析与分发
//
// 收到完整帧后按 Cmd 字段分发到对应处理逻辑：
//   CMD_KEY_EVENT(0x11) → 钥匙放上/拿走事件（异步，不影响 inFlight）
//   CMD_ACK(0x5A)       → 设备确认（匹配 inFlight 命令）
//   CMD_NAK(0x00)       → 设备拒绝（含错误码）
//   CMD_Q_TASK(0x04)    → 查询任务响应（含任务列表）
//   其他                → 未知命令，仅记录日志
// ============================================================

/**
 * @brief 解析完整帧并分发到对应命令处理器
 * @param frame 已通过 CRC 校验的完整帧数据
 *
 * 解析步骤：
 *   1. 从固定偏移量提取 KeyId、FrameNo、Len、Cmd、Data
 *   2. 调用 markPortVerified() 锁定已验证端口
 *   3. 按 Cmd 分发到对应处理分支
 *
 * 各命令处理要点：
 *   - KEY_EVENT：放上(01 00)时启动稳定性窗口+延迟握手；拿走(00 00)时重置全部状态
 *   - ACK：匹配 inFlight 类型后清除等待状态，SET_COM ACK 触发 startup probe
 *   - NAK：记录错误码，清除 inFlight，通知 UI
 *   - Q_TASK：解析任务列表（每条 20 字节），更新 m_tasks 缓存
 *
 * @note KEY_EVENT 是异步事件，不受 inFlight 机制约束，随时可能到达。
 *       收到 KEY_PLACED 时会主动清除当前 inFlight 状态并 drain 缓冲区。
 */
void KeySerialClient::handleFrame(const QByteArray &frame)
{
    quint8 keyId   = static_cast<quint8>(frame[OFF_KEYID]);
    quint8 frameNo = static_cast<quint8>(frame[OFF_FRAMENO]);
    quint16 lenField = static_cast<quint8>(frame[OFF_LEN])
                     | (static_cast<quint8>(frame[OFF_LEN + 1]) << 8);
    quint8 cmd = static_cast<quint8>(frame[OFF_CMD]);
    int dataLen = lenField - 1;
    QByteArray data = frame.mid(OFF_DATA, dataLen);

    log(QString("  KeyId=0x%1 FrameNo=0x%2 Cmd=0x%3 DataLen=%4")
            .arg(keyId, 2, 16, QChar('0'))
            .arg(frameNo, 2, 16, QChar('0'))
            .arg(cmd, 2, 16, QChar('0'))
            .arg(dataLen));

    // 收到合法协议帧（已通过 7E6C + CRC 校验）后，锁定验证端口。
    markPortVerified(QString("cmd=0x%1").arg(cmd, 2, 16, QChar('0')));

    // --- 钥匙放上/拿走事件 (0x11) — 异步，不影响 inFlight ---
    if (cmd == CMD_KEY_EVENT) {
        if (dataLen == 4) {
            quint8 b0 = static_cast<quint8>(data[0]);
            quint8 b1 = static_cast<quint8>(data[1]);
            if (b0 == 0x01 && b1 == 0x00) {
                // 钥匙重新放上时若有命令在途，需通知 UI 该命令已被取消
                const bool hadInFlight = m_inFlight;
                stopRetryTimer();
                clearInFlight();
                clearTicketTransferState();
                clearTaskLogState();
                m_pendingAfterDelQuery = false;
                m_qtaskRecoveryTimer->stop();
                m_qtaskRecoveryRound = 0;
                m_qtaskProbeInFlight = false;
                if (hadInFlight) {
                    emitNotice("钥匙重新放上，当前命令已取消，稳定后将重新握手");
                }
                const qint64 drained = drainSerialInput("KEY_PLACED", true);
                m_keyPlaced = true;
                m_sessionReady = false;
                log("  >> [EVENT] KEY_PLACED (钥匙放上)");
                emitLog(LogDir::EVENT, CMD_KEY_EVENT, lenField,
                        QString("KEY_PLACED (钥匙放上), drainBytes=%1").arg(drained), frame);
                emit keyPlacedChanged(true);
                startKeyStabilityWindow("key placed");
                QTimer::singleShot(KEY_PLACED_HANDSHAKE_DELAY_MS, this, [this]() {
                    if (!m_serial->isOpen() || !m_keyPlaced || m_inFlight) {
                        return;
                    }
                    sendSetComHandshake("key placed settle delay");
                });
            } else if (b0 == 0x00 && b1 == 0x00) {
                // 拔钥匙时若有命令在途，必须立即取消，避免继续重试已拔走的设备
                const bool hadInFlight = m_inFlight;
                stopRetryTimer();
                clearInFlight();
                clearTicketTransferState();
                clearTaskLogState();
                m_pendingAfterDelQuery = false;
                if (hadInFlight) {
                    emitNotice("钥匙已拔走，当前命令已取消");
                }
                m_keyPlaced = false;
                m_keyStable = false;
                m_sessionReady = false;
                m_keyStableTimer->stop();
                m_qtaskRecoveryTimer->stop();
                m_qtaskRecoveryRound = 0;
                m_qtaskProbeInFlight = false;
                m_deferredInFlightType = INFLIGHT_NONE;
                m_deferredDeleteTaskId.clear();
                m_keyPlacedElapsed.invalidate();
                m_lastNoiseElapsed.invalidate();
                log("  >> [EVENT] KEY_REMOVED (钥匙拿走)");
                emitLog(LogDir::EVENT, CMD_KEY_EVENT, lenField,
                        "KEY_REMOVED (钥匙拿走)", frame);
                emit keyPlacedChanged(false);
                emit keyStableChanged(false);
            } else {
                log(QString("  >> [EVENT] KEY_EVENT_UNKNOWN data=%1")
                        .arg(QString(data.toHex(' ').toUpper())));
                emitLog(LogDir::EVENT, CMD_KEY_EVENT, lenField,
                        QString("KEY_EVENT_UNKNOWN data=%1")
                            .arg(QString(data.toHex(' ').toUpper())), frame);
            }
        }
        return;
    }

    // --- ACK (0x5A) ---
    if (cmd == CMD_ACK) {
        quint8 ackedCmd = dataLen > 0 ? static_cast<quint8>(data[0]) : 0x00;
        const quint8 inFlightBeforeAck = m_inFlightType;
        log(QString("  >> ACK for Cmd=0x%1").arg(ackedCmd, 2, 16, QChar('0')));

        // 生成 ACK 摘要：标明被确认的命令
        QString ackSummary = QString("ACK for 0x%1").arg(ackedCmd, 2, 16, QChar('0'));
        switch (ackedCmd) {
        case CMD_SET_COM: ackSummary = "ACK for SET_COM"; break;
        case CMD_DEL:     ackSummary = "ACK for DEL"; break;
        }
        emitLog(LogDir::RX, CMD_ACK, lenField, ackSummary, frame);

        bool matched = false;
        if (m_inFlightType == INFLIGHT_SETCOM && ackedCmd == CMD_SET_COM)
            matched = true;
        else if (m_inFlightType == INFLIGHT_DEL && ackedCmd == CMD_DEL)
            matched = true;
        else if (m_inFlightType == INFLIGHT_TICKET && ackedCmd == m_expectedAckCmd)
            matched = true;

        if (matched) {
            stopRetryTimer();
            clearInFlight();
            m_qtaskProbeInFlight = false;
        }

        if (ackedCmd == CMD_SET_COM) {
            m_sessionReady = true;
            markPortVerified("SET_COM ACK");
            scheduleStartupProbeQTask();
        }

        if (matched && inFlightBeforeAck != INFLIGHT_TICKET) {
            tryDispatchDeferredCommand();
        }

        emit ackReceived(ackedCmd);

        if (ackedCmd != CMD_SET_COM && !m_ticketFrames.isEmpty() && m_ticketFrameIndex >= 0) {
            emitLog(LogDir::EVENT, ackedCmd, lenField,
                    QString("TICKET frame %1/%2 ACK")
                        .arg(m_ticketFrameIndex + 1)
                        .arg(m_ticketFrames.size()),
                    {}, true, true);
            if (m_ticketFrameIndex + 1 < m_ticketFrames.size()) {
                if (!sendNextTicketFrame()) {
                    emitNotice("传票发送失败：后续帧派发失败");
                    emit ticketTransferFailed("后续帧派发失败");
                    clearTicketTransferState();
                }
            } else {
                emitNotice(QString("传票发送完成，共 %1 帧").arg(m_ticketFrames.size()));
                emit ticketTransferFinished();
                clearTicketTransferState();
            }
            return;
        }

        // DEL ACK → 自动再查一次验证
        if (ackedCmd == CMD_DEL && m_pendingAfterDelQuery) {
            m_pendingAfterDelQuery = false;
            log("  >> DEL 成功，自动发起 Q_TASK 验证...");
            queryTasksAll();
        }
        return;
    }

    // --- I_TASK_LOG 回包 (0x05) ---
    if (cmd == CMD_I_TASK_LOG) {
        const quint16 totalFrames = (dataLen >= 2) ? readLe16(data, 0) : 0;
        emitLog(LogDir::RX, CMD_I_TASK_LOG, lenField,
                QString("I_TASK_LOG resp: totalFrames=%1").arg(totalFrames),
                frame);

        if (m_inFlightType == INFLIGHT_TASKLOG) {
            stopRetryTimer();
            clearInFlight();
        }

        m_pendingTaskLogFrames = totalFrames;
        m_taskLogExpectedSeq = 0;
        m_taskLogPayloadBuffer.clear();
        if (m_pendingTaskLogTaskId.isEmpty()) {
            emitLog(LogDir::WARN, CMD_I_TASK_LOG, lenField,
                    "I_TASK_LOG resp without active task context",
                    {}, true, true);
        }

        QByteArray ackData;
        ackData.append(static_cast<char>(CMD_I_TASK_LOG));
        ackData.append('\x00');
        ackData.append('\x00');
        QByteArray addr2;
        addr2.append(static_cast<char>(KeyProtocol::Addr2DefaultLo));
        addr2.append(static_cast<char>(KeyProtocol::Addr2DefaultHi));
        const QByteArray ackFrame = buildFrame(CMD_ACK, ackData, addr2);
        sendFrameNoWait(ackFrame,
                        CMD_ACK,
                        static_cast<quint16>(1 + ackData.size()),
                        QString("ACK for I_TASK_LOG, start upload from frame 0"));

        if (totalFrames > 1) {
            emitNotice(QString("检测到 %1 帧回传日志，进入多帧接收模式").arg(totalFrames));
        }
        return;
    }

    // --- NAK (0x00) ---
    if (cmd == CMD_NAK) {
        quint8 origCmd = dataLen > 0 ? static_cast<quint8>(data[0]) : 0xFF;
        // NAK for SET_COM 意味着握手失败，不能标记会话就绪
        if (origCmd != CMD_SET_COM) {
            m_sessionReady = true;
        }
        quint8 errCode = dataLen > 1 ? static_cast<quint8>(data[1]) : 0xFF;
        QString errDesc;
        switch (errCode) {
        case ERR_CRC_FAIL:     errDesc = "CRC校验错误"; break;
        case ERR_LEN_MISMATCH: errDesc = "字节数不符"; break;
        case ERR_GUID_MISSING: errDesc = "GUID不存在"; break;
        default: errDesc = QString("未知错误码 0x%1").arg(errCode, 2, 16, QChar('0'));
        }
        log(QString("  >> NAK! OrigCmd=0x%1 ErrCode=0x%2 (%3)")
                .arg(origCmd, 2, 16, QChar('0'))
                .arg(errCode, 2, 16, QChar('0'))
                .arg(errDesc));
        emitLog(LogDir::ERROR, CMD_NAK, lenField,
                QString("NAK for 0x%1: %2")
                    .arg(origCmd, 2, 16, QChar('0'))
                    .arg(errDesc),
                frame);
        stopRetryTimer();
        clearInFlight();
        clearTicketTransferState();
        clearTaskLogState();
        m_pendingAfterDelQuery = false;
        if (origCmd == CMD_TICKET || origCmd == CMD_TICKET_MORE) {
            emitNotice(QString("传票发送收到 NAK：%1").arg(errDesc));
            emit ticketTransferFailed(QString("收到 NAK：%1").arg(errDesc));
        } else if (origCmd == CMD_I_TASK_LOG || origCmd == CMD_UP_TASK_LOG) {
            emitNotice(QString("回传日志收到 NAK：%1").arg(errDesc));
        }
        tryDispatchDeferredCommand();
        emit nakReceived(origCmd, errCode);
        return;
    }

    // --- Q_TASK 回包 (0x04) ---
    if (cmd == CMD_Q_TASK) {
        m_sessionReady = true;
        m_qtaskRecoveryRound = 0;
        m_qtaskRecoveryTimer->stop();
        m_qtaskProbeInFlight = false;
        if (m_inFlightType == INFLIGHT_QTASK) {
            stopRetryTimer();
            clearInFlight();
        }

        if (!m_keyPlaced) {
            m_keyPlaced = true;
            emit keyPlacedChanged(true);
            emitLog(LogDir::EVENT, CMD_Q_TASK, lenField,
                    "Q_TASK probe confirmed key present");
        }
        if (!m_keyStable) {
            m_keyStable = true;
            m_keyStableTimer->stop();
            m_keyPlacedElapsed.invalidate();
            m_lastNoiseElapsed.invalidate();
            emit keyStableChanged(true);
            emitLog(LogDir::EVENT, CMD_Q_TASK, lenField,
                    "Q_TASK response marked key stable");
        }

        if (dataLen < 1) {
            log("  >> Q_TASK 回包异常：无数据");
            emitLog(LogDir::ERROR, CMD_Q_TASK, lenField,
                    "Q_TASK resp: empty payload", frame);
            return;
        }

        quint8 count = static_cast<quint8>(data[0]);
        log(QString("  >> Q_TASK 任务数: %1").arg(count));
        m_tasks.clear();

        // 提取第一条任务ID用于摘要（便于用户快速识别）
        QString firstTaskHex;
        int offset = 1;
        for (int i = 0; i < count && (offset + TASK_INFO_LEN) <= dataLen; ++i) {
            KeyTaskInfo info;
            info.taskId   = data.mid(offset, TASK_ID_LEN);
            info.status   = static_cast<quint8>(data[offset + 16]);
            info.type     = static_cast<quint8>(data[offset + 17]);
            info.source   = static_cast<quint8>(data[offset + 18]);
            info.reserved = static_cast<quint8>(data[offset + 19]);
            m_tasks.append(info);

            if (i == 0) {
                firstTaskHex = info.taskIdHex();
            }

            log(QString("  [%1] taskId=%2 status=0x%3 type=0x%4")
                    .arg(i + 1)
                    .arg(info.taskIdHex())
                    .arg(info.status, 2, 16, QChar('0'))
                    .arg(info.type, 2, 16, QChar('0')));
            offset += TASK_INFO_LEN;
        }

        // 结构化日志：摘要包含任务数和首条ID
        QString qtaskSummary = QString("Q_TASK resp: count=%1").arg(count);
        if (!firstTaskHex.isEmpty()) {
            qtaskSummary += QString(", first=%1").arg(firstTaskHex.left(23));
        }
        emitLog(LogDir::RX, CMD_Q_TASK, lenField, qtaskSummary, frame);

        emit tasksUpdated(m_tasks);
        tryDispatchDeferredCommand();
        return;
    }

    // --- UP_TASK_LOG 回包 (0x15) ---
    if (cmd == CMD_UP_TASK_LOG) {
        emitLog(LogDir::RX, CMD_UP_TASK_LOG, lenField,
                QString("UP_TASK_LOG recv: payload=%1B").arg(dataLen),
                frame);

        if (m_pendingTaskLogFrames == 0) {
            emitLog(LogDir::WARN, CMD_UP_TASK_LOG, lenField,
                    "UP_TASK_LOG ignored: no active I_TASK_LOG context",
                    {}, true, true);
            return;
        }

        if (dataLen < 2) {
            emitLog(LogDir::ERROR, CMD_UP_TASK_LOG, lenField,
                    "UP_TASK_LOG payload too short", frame);
            clearTaskLogState();
            return;
        }

        const quint16 frameSeq = readLe16(data, 0);
        if (frameSeq != m_taskLogExpectedSeq) {
            emitLog(LogDir::ERROR, CMD_UP_TASK_LOG, lenField,
                    QString("UP_TASK_LOG unexpected seq=%1 expected=%2")
                        .arg(frameSeq)
                        .arg(m_taskLogExpectedSeq),
                    frame);
            emitNotice(QString("回传日志帧序号异常：seq=%1 expected=%2")
                           .arg(frameSeq)
                           .arg(m_taskLogExpectedSeq));
            clearTaskLogState();
            return;
        }

        m_taskLogPayloadBuffer.append(data.mid(2));
        emitLog(LogDir::EVENT, CMD_UP_TASK_LOG, lenField,
                QString("UP_TASK_LOG frame %1/%2 buffered, bytes=%3")
                    .arg(frameSeq + 1)
                    .arg(m_pendingTaskLogFrames)
                    .arg(m_taskLogPayloadBuffer.size()),
                {}, true, true);
        sendTaskLogAck(frameSeq);
        ++m_taskLogExpectedSeq;

        if (m_taskLogExpectedSeq < m_pendingTaskLogFrames) {
            return;
        }

        const QByteArray fullPayload = m_taskLogPayloadBuffer;
        const quint16 totalFrames = m_pendingTaskLogFrames;
        const bool ok = parseAndEmitTaskLogPayload(fullPayload,
                                                   totalFrames,
                                                   frameSeq,
                                                   lenField,
                                                   frame);
        clearTaskLogState();
        if (!ok) {
            emitNotice("回传日志解析失败，请查看串口报文");
        }
        return;
    }

    // --- 其他未知命令 ---
    log(QString("  >> 未处理命令 0x%1").arg(cmd, 2, 16, QChar('0')));
}

// ============================================================
// 串口数据到达
// ============================================================

/**
 * @brief QSerialPort::readyRead 信号回调，驱动数据接收和拆包
 *
 * 处理流程：
 *   1. readAll() 读取所有可用数据
 *   2. 记录 RAW_RX 日志（最多前 64 字节，避免日志过大）
 *   3. 检查是否包含 Q_TASK 帧头（用于超时诊断）
 *   4. 噪声检测：钥匙稳定期间收到非帧头数据视为噪声
 *   5. 追加到接收缓冲区
 *   6. 循环调用 tryExtractOneFrame() 提取并处理所有完整帧
 *
 * @note 单次 readyRead 可能包含多帧数据（粘包），因此用 while 循环提取。
 * @note 也可能只包含半帧数据（拆包），此时 tryExtractOneFrame 返回 0，
 *       数据留在 m_recvBuffer 中等待下次 readyRead 补齐。
 */
void KeySerialClient::onReadyRead()
{
    QByteArray incoming = m_serial->readAll();
    if (incoming.isEmpty()) return;

    // RAW_RX 日志（最多前64字节）
    int showLen = qMin(incoming.size(), 64);
    QString rawHex = incoming.left(showLen).toHex(' ').toUpper();
    if (incoming.size() > 64) rawHex += " ...";
    log(QString("RAW_RX +%1 bytes: %2").arg(incoming.size()).arg(rawHex));

    // RAW 结构化日志（仅专家模式显示）
    emitLog(LogDir::RAW, 0, 0,
            QString("RAW_RX +%1 bytes").arg(incoming.size()),
            incoming, true, true);

    markQTaskFrameHeaderSeen(incoming);

    if (m_keyPlaced && !m_keyStable &&
        incoming.indexOf(QByteArray::fromHex("7E6C")) < 0) {
        markNoiseDuringStabilizing(incoming, "raw chunk");
    }

    m_recvBuffer.append(incoming);

    // 循环提取完整帧
    QByteArray frame;
    while (tryExtractOneFrame(m_recvBuffer, frame) == 1) {
        logHex("REC", frame);
        handleFrame(frame);
        frame.clear();
    }
}

// ============================================================
// 发送与重试
// ============================================================

/**
 * @brief 发送协议帧并启动重试定时器
 * @param frame 完整帧数据（由 buildFrame 构建）
 * @param expectedAckCmd 期望的响应命令码（ACK 时为 CMD_ACK，Q_TASK 时为 CMD_Q_TASK）
 * @param inFlightType 当前命令类型标识（INFLIGHT_SETCOM/QTASK/DEL）
 *
 * 执行流程：
 *   1. 前置检查：串口已打开、无 inFlight 命令
 *   2. 记录 TX 结构化日志（含命令摘要）
 *   3. write() + flush() + waitForBytesWritten(200ms) 确保数据发出
 *   4. 设置 inFlight 状态，保存帧副本（用于重试）
 *   5. 启动重试定时器
 *
 * @note flush() 在不同平台表现不一致（Windows 可能返回 false），
 *       不作为断口依据，仅记录告警。
 * @note waitForBytesWritten(200ms) 是阻塞等待，确保数据已写入内核缓冲区。
 *       200ms 对于 115200 波特率下最大帧（~530 字节）绰绰有余。
 */
void KeySerialClient::sendFrame(const QByteArray &frame, quint8 expectedAckCmd,
                                quint8 inFlightType)
{
    if (!m_serial->isOpen()) return;

    if (m_inFlight) {
        log("[BUSY] sendFrame 被拒绝：上一个请求仍在途");
        emitLog(LogDir::WARN, 0, 0, "sendFrame 被拒绝：上一个请求仍在途");
        return;
    }

    logHex("SEN", frame);

    // 从帧中提取 cmd 和 lenField 用于结构化日志
    quint8 txCmd = (frame.size() > OFF_CMD) ? static_cast<quint8>(frame[OFF_CMD]) : 0;
    quint16 txLen = 0;
    if (frame.size() > OFF_LEN + 1) {
        txLen = static_cast<quint8>(frame[OFF_LEN])
              | (static_cast<quint8>(frame[OFF_LEN + 1]) << 8);
    }

    // 生成 TX 摘要
    QString txSummary;
    switch (txCmd) {
    case CMD_SET_COM: txSummary = "SET_COM send"; break;
    case CMD_Q_TASK:  txSummary = "Q_TASK(all) send"; break;
    case CMD_I_TASK_LOG: txSummary = "I_TASK_LOG send"; break;
    case CMD_TICKET:  txSummary = "TICKET send"; break;
    case CMD_TICKET_MORE: txSummary = "TICKET_MORE send"; break;
    case CMD_DEL: {
        // 从帧数据区提取 taskId 前4字节作为摘要
        QByteArray taskIdPart = frame.mid(OFF_DATA, qMin(4, frame.size() - OFF_DATA));
        txSummary = QString("DEL send: taskId=%1...").arg(QString(taskIdPart.toHex().toUpper()));
        break;
    }
    default:
        txSummary = QString("TX cmd=0x%1").arg(txCmd, 2, 16, QChar('0'));
    }
    emitLog(LogDir::TX, txCmd, txLen, txSummary, frame);

    qint64 written = m_serial->write(frame);
    const bool flushOk = m_serial->flush();
    const bool waitOk = m_serial->waitForBytesWritten(200);

    if (!flushOk) {
        // flush() 在不同平台表现不一致，不作为断口依据，仅记录告警（工程师模式）。
        log("[WARN] flush returned false (non-fatal)");
        emitLog(LogDir::WARN, txCmd, txLen, "flush returned false (non-fatal)", {}, true, true);
    }

    if (written != frame.size() || !waitOk) {
        log(QString("[WARN] write: %1/%2 bytes, wait=%3")
                .arg(written).arg(frame.size())
                .arg(waitOk ? "OK" : "FAIL"));
        emitLog(LogDir::WARN, txCmd, txLen,
                QString("write: %1/%2 bytes, wait=%3")
                    .arg(written).arg(frame.size())
                    .arg(waitOk ? "OK" : "FAIL"));
    } else {
        log(QString("  write OK: %1 bytes").arg(written));
    }

    m_inFlight = true;
    m_inFlightType = inFlightType;
    m_lastSentFrame = frame;
    startRetryTimer(expectedAckCmd);
}

/**
 * @brief 启动重试定时器
 * @param expectedAckCmd 期望的响应命令码，用于超时后判断是哪个命令超时
 *
 * 重置重试计数器并启动 singleShot 定时器（RETRY_TIMEOUT_MS=1000ms）。
 * 超时后触发 onRetryTimeout()，最多重试 MAX_RETRIES(3) 次。
 */
void KeySerialClient::startRetryTimer(quint8 expectedAckCmd)
{
    m_expectedAckCmd = expectedAckCmd;
    m_retryCount = 0;
    m_retryTimer->start(RETRY_TIMEOUT_MS);
}

/**
 * @brief 停止重试定时器并清理重试状态
 *
 * 停止定时器、重置计数器、清空上次发送帧副本。
 * 在收到响应（ACK/NAK/Q_TASK 回包）或超时放弃时调用。
 */
void KeySerialClient::stopRetryTimer()
{
    m_retryTimer->stop();
    m_retryCount = 0;
    m_lastSentFrame.clear();
}

/**
 * @brief 清除 inFlight 状态，允许发送新命令
 *
 * 若当前 inFlight 类型为 Q_TASK，同时重置帧头观察标志。
 * 在收到匹配响应、超时放弃、或 KEY_PLACED 事件时调用。
 */
void KeySerialClient::clearInFlight()
{
    if (m_inFlightType == INFLIGHT_QTASK) {
        m_qtaskSawFrameHeader = false;
    }
    m_inFlight = false;
    m_inFlightType = INFLIGHT_NONE;
}

void KeySerialClient::sendFrameNoWait(const QByteArray &frame,
                                      quint8 cmd,
                                      quint16 lenField,
                                      const QString &summary)
{
    if (!m_serial->isOpen()) {
        emitLog(LogDir::WARN, cmd, lenField,
                QString("skip immediate frame: port closed (%1)").arg(summary),
                {}, true, true);
        return;
    }

    logHex("SEN", frame);
    const qint64 written = m_serial->write(frame);
    const bool flushOk = m_serial->flush();
    const bool waitOk = m_serial->waitForBytesWritten(200);
    emitLog(LogDir::TX, cmd, lenField, summary, frame);
    if (written != frame.size() || !waitOk) {
        emitLog(LogDir::WARN, cmd, lenField,
                QString("immediate write: %1/%2 bytes, wait=%3")
                    .arg(written).arg(frame.size())
                    .arg(waitOk ? "OK" : "FAIL"),
                {}, true, true);
    }
    if (!flushOk) {
        emitLog(LogDir::WARN, cmd, lenField,
                "immediate flush returned false (non-fatal)",
                {}, true, true);
    }
}

/**
 * @brief 检查业务命令是否可以立即发送，否则延迟到条件满足后自动派发
 * @param inFlightType 待发送的命令类型（INFLIGHT_QTASK 或 INFLIGHT_DEL）
 * @param taskId DEL 命令的 taskId（Q_TASK 时为空）
 * @return true=条件满足可立即发送；false=已延迟暂存，稍后自动派发
 *
 * 判断逻辑（三层门控）：
 *   1. 钥匙不在位 → 直接拒绝（不延迟，因为无法预期钥匙何时放上）
 *   2. 钥匙稳定 + 会话就绪 → 允许发送
 *   3. 钥匙稳定但会话未就绪 → 延迟，并主动发起 SET_COM 握手
 *   4. 钥匙未稳定 → 延迟，启动稳定性观察窗口
 *
 * 延迟机制：将命令类型和参数暂存到 m_deferredInFlightType / m_deferredDeleteTaskId，
 * 待 onKeyStableTimerTimeout() 或 tryDispatchDeferredCommand() 时自动派发。
 *
 * @note 同一时刻只能暂存一条延迟命令，后到的会覆盖先到的。
 */
bool KeySerialClient::ensureBusinessReadyOrDefer(quint8 inFlightType, const QByteArray &taskId)
{
    if (!m_keyPlaced) {
        const QString cmdName = (inFlightType == INFLIGHT_DEL) ? "DEL" : "Q_TASK";
        emitLog(LogDir::WARN, 0, 0, QString("%1 blocked: key not present").arg(cmdName));
        emitNotice(QString("%1 失败：钥匙不在位").arg(cmdName));
        return false;
    }
    if (m_keyStable) {
        if (m_sessionReady) {
            return true;
        }
        m_deferredInFlightType = inFlightType;
        if (inFlightType == INFLIGHT_DEL) {
            m_deferredDeleteTaskId = taskId;
        } else {
            m_deferredDeleteTaskId.clear();
        }
        emitLog(LogDir::EVENT, 0, 0, "Session not ready, defer business cmd");
        if (!m_inFlight) {
            sendSetComHandshake("session not ready");
        }
        return false;
    }

    m_deferredInFlightType = inFlightType;
    if (inFlightType == INFLIGHT_DEL) {
        m_deferredDeleteTaskId = taskId;
    } else {
        m_deferredDeleteTaskId.clear();
    }

    const QString cmdName = (inFlightType == INFLIGHT_DEL) ? "DEL" : "Q_TASK";
    emitLog(LogDir::EVENT, 0, 0,
            QString("%1 deferred: waiting KEY_STABLE").arg(cmdName));
    startKeyStabilityWindow(QString("%1 deferred").arg(cmdName));
    return false;
}

/**
 * @brief 启动钥匙稳定性观察窗口
 * @param reason 触发原因（用于日志追踪，如 "key placed"、"Q_TASK deferred"）
 *
 * 钥匙放上后 USB 串口底座会产生电气噪声（持续数百毫秒），
 * 在此期间发送业务命令可能导致通讯失败。因此需要等待一个
 * "静默窗口"确认噪声消退后才标记钥匙为稳定状态。
 *
 * 机制：
 *   1. 重置 keyStable=false，启动 m_keyPlacedElapsed 计时
 *   2. 启动 KEY_STABLE_WINDOW_MS(600ms) 定时器
 *   3. 期间若收到噪声数据，markNoiseDuringStabilizing() 会重置噪声计时器
 *   4. 定时器到期时 onKeyStableTimerTimeout() 检查是否满足稳定条件
 *
 * @note 若钥匙不在位则直接返回（防御性编程）。
 */
void KeySerialClient::startKeyStabilityWindow(const QString &reason)
{
    if (!m_keyPlaced) {
        return;
    }
    m_keyStable = false;
    m_keyPlacedElapsed.restart();
    m_lastNoiseElapsed.invalidate();
    m_keyStableTimer->start(KEY_STABLE_WINDOW_MS);
    emit keyStableChanged(false);
    emitLog(LogDir::EVENT, 0, 0,
            QString("KEY_STABLE window start (%1, %2ms)")
                .arg(reason)
                .arg(KEY_STABLE_WINDOW_MS),
            {}, true, true);
}

/**
 * @brief 标记稳定性观察期间收到的噪声数据
 * @param noise 噪声数据内容（用于日志记录）
 * @param reason 噪声来源描述（如 "raw chunk"、"no header"、"crc fail"）
 *
 * 仅在钥匙在位且未稳定时生效。重置 m_lastNoiseElapsed 计时器，
 * 使 onKeyStableTimerTimeout() 在噪声消退后额外等待 KEY_QUIET_WINDOW_MS(400ms)。
 *
 * @note 钥匙已稳定或不在位时调用无效（防御性检查）。
 */
void KeySerialClient::markNoiseDuringStabilizing(const QByteArray &noise, const QString &reason)
{
    if (!m_keyPlaced || m_keyStable) {
        return;
    }
    m_lastNoiseElapsed.restart();
    emitLog(LogDir::WARN, 0, 0,
            QString("Noise during KEY_STABLE (%1)").arg(reason),
            noise, true, true);
}

/**
 * @brief 尝试派发之前延迟暂存的业务命令
 *
 * 前置条件（全部满足才派发）：
 *   1. 钥匙已稳定（m_keyStable=true）
 *   2. 无 inFlight 命令
 *   3. 串口已打开
 *
 * 从 m_deferredInFlightType 读取暂存的命令类型，清空暂存后调用对应方法。
 * 在以下时机被调用：
 *   - onKeyStableTimerTimeout()：钥匙稳定后
 *   - handleFrame() 中 ACK/NAK/Q_TASK 处理完成后
 *
 * @note 先清空暂存再调用业务方法，避免递归重入问题。
 */
void KeySerialClient::tryDispatchDeferredCommand()
{
    if (!m_keyStable || m_inFlight || !m_serial->isOpen()) {
        return;
    }

    const quint8 deferred = m_deferredInFlightType;
    const QByteArray deferredTaskId = m_deferredDeleteTaskId;
    m_deferredInFlightType = INFLIGHT_NONE;
    m_deferredDeleteTaskId.clear();

    if (deferred == INFLIGHT_QTASK) {
        emitNotice("钥匙已就绪，自动发送 Q_TASK");
        queryTasksAllInternal(false);
    } else if (deferred == INFLIGHT_DEL && deferredTaskId.size() == TASK_ID_LEN) {
        emitNotice("钥匙已就绪，自动发送 DEL");
        deleteTask(deferredTaskId);
    }
}

/**
 * @brief 发送 SET_COM(0x0F) 握手帧
 * @param reason 触发原因（用于日志追踪，如 "connect"、"key placed settle delay"）
 *
 * SET_COM 是与设备建立通讯会话的握手命令：
 *   - Data 区固定 1 字节 0x01（启用通讯）
 *   - Addr2 使用默认值 0x0000
 *   - 期望响应：ACK(0x5A)，ACK.data[0] == CMD_SET_COM(0x0F)
 *
 * 调用时机：
 *   1. connectPort() 成功后立即调用
 *   2. KEY_PLACED 事件后延迟 600ms 调用（等待电气噪声消退）
 *   3. 超时恢复时重新握手
 *   4. 钥匙稳定但会话未就绪时补发
 *
 * @note 若有 inFlight 命令则跳过（避免冲突），仅记录告警日志。
 */
void KeySerialClient::sendSetComHandshake(const QString &reason)
{
    if (!m_serial->isOpen()) {
        return;
    }
    if (m_inFlight) {
        emitLog(LogDir::WARN, 0, 0,
                QString("skip SET_COM re-handshake: inFlight (%1)").arg(reason),
                {}, true, true);
        return;
    }

    log(QString("发送 SET_COM(0x0F) 握手... (%1)").arg(reason));
    QByteArray data;
    data.append(static_cast<char>(0x01));
    QByteArray addr2;
    addr2.append(static_cast<char>(KeyProtocol::Addr2DefaultLo));
    addr2.append(static_cast<char>(KeyProtocol::Addr2DefaultHi));
    QByteArray frame = buildFrame(CMD_SET_COM, data, addr2);
    sendFrame(frame, CMD_ACK, INFLIGHT_SETCOM);
}

/**
 * @brief 排空串口接收缓冲区中的残留数据
 * @param reason 排空原因（用于日志追踪）
 * @param clearRecvBuffer true=同时清空应用层接收缓冲区 m_recvBuffer
 * @return 从串口硬件缓冲区中排空的字节数
 *
 * drain 机制的必要性：
 *   串口是全双工的，设备可能在任意时刻发送异步事件帧（如 KEY_EVENT）。
 *   如果在发送业务命令前不清空接收缓冲区，残留的异步帧可能被误认为
 *   业务响应，导致协议状态机混乱。
 *
 * 实现：循环调用 readAll() 直到 bytesAvailable()==0，丢弃所有读到的数据。
 *
 * @note clearRecvBuffer=true 时会同时清空 m_recvBuffer（应用层拆包缓冲区），
 *       确保后续 tryExtractOneFrame() 从干净状态开始。
 */
qint64 KeySerialClient::drainSerialInput(const QString &reason, bool clearRecvBuffer)
{
    if (!m_serial || !m_serial->isOpen()) {
        return 0;
    }

    const int recvBufBefore = m_recvBuffer.size();
    qint64 drainedBytes = 0;
    while (m_serial->bytesAvailable() > 0) {
        const QByteArray chunk = m_serial->readAll();
        if (chunk.isEmpty()) {
            break;
        }
        drainedBytes += chunk.size();
    }

    if (clearRecvBuffer) {
        m_recvBuffer.clear();
    }
    const int recvBufAfter = m_recvBuffer.size();

    emitLog(LogDir::EVENT, 0, 0,
            QString("RX drain reason=%1 drainBytes=%2 recvBufSizeBefore=%3 recvBufSizeAfter=%4")
                .arg(reason)
                .arg(drainedBytes)
                .arg(recvBufBefore)
                .arg(recvBufAfter),
            {}, true, true);
    return drainedBytes;
}

/**
 * @brief 业务命令发送前排空接收缓冲区（便捷方法）
 * @param cmdName 即将发送的命令名称（用于日志，如 "Q_TASK"、"DEL"）
 *
 * 委托给 drainSerialInput()，clearRecvBuffer=true 确保完全清空。
 */
void KeySerialClient::drainBeforeBusinessSend(const QString &cmdName)
{
    drainSerialInput(QString("pre-send %1").arg(cmdName), true);
}

/**
 * @brief 安排启动探测 Q_TASK（startup probe）
 *
 * 背景：SET_COM ACK 后正常流程是等待 KEY_EVENT(放上) 再查询任务。
 * 但存在边界情况：钥匙在连接前就已放在底座上，此时不会触发 KEY_EVENT。
 * startup probe 通过主动发送 Q_TASK 来探测钥匙是否已在位。
 *
 * 机制：
 *   1. 延迟 STARTUP_PROBE_DELAY_MS(200ms) 后执行（给 KEY_EVENT 一个到达窗口）
 *   2. 若延迟期间收到 KEY_EVENT 或 inFlight 变化，则取消探测
 *   3. 探测时 allowWhenUnknownKey=true，跳过钥匙在位检查
 *   4. 若探测超时（无响应），静默处理，不报错（钥匙确实不在位）
 *
 * @note 仅在 SET_COM ACK 后、钥匙未在位、无 inFlight 时才安排探测。
 */
void KeySerialClient::scheduleStartupProbeQTask()
{
    if (!m_serial->isOpen() || m_keyPlaced || m_inFlight) {
        return;
    }

    emitLog(LogDir::EVENT, 0, 0,
            QString("SET_COM ACK without KEY_EVT, schedule Q_TASK probe in %1ms")
                .arg(STARTUP_PROBE_DELAY_MS),
            {}, true, true);
    QTimer::singleShot(STARTUP_PROBE_DELAY_MS, this, [this]() {
        if (!m_serial->isOpen() || m_inFlight || m_keyPlaced || !m_sessionReady) {
            return;
        }
        queryTasksAllInternal(false, true);
    });
}

/**
 * @brief 检测 Q_TASK inFlight 期间是否观察到帧头 7E 6C
 * @param incoming 本次 readyRead 收到的原始数据
 *
 * 用于超时诊断：如果 Q_TASK 超时但曾观察到帧头，说明设备有响应但帧不完整
 * （可能是传输中断或 CRC 失败）；若未观察到帧头，说明设备完全无响应
 * （可能是钥匙不在位或底座故障）。
 *
 * 实现细节：
 *   将 m_recvBuffer 最后 1 字节与 incoming 拼接后搜索 7E 6C，
 *   兼容帧头跨两次 readyRead 到达的情况（0x7E 在上一次，0x6C 在本次）。
 *
 * @note 仅在 INFLIGHT_QTASK 且尚未观察到帧头时生效，避免重复标记。
 */
void KeySerialClient::markQTaskFrameHeaderSeen(const QByteArray &incoming)
{
    if (m_inFlightType != INFLIGHT_QTASK || m_qtaskSawFrameHeader) {
        return;
    }
    const QByteArray marker = QByteArray::fromHex("7E6C");
    QByteArray probe = m_recvBuffer;
    if (probe.size() > 1) {
        probe = probe.right(1);
    }
    probe.append(incoming);
    if (probe.indexOf(marker) >= 0) {
        m_qtaskSawFrameHeader = true;
        emitLog(LogDir::EVENT, CMD_Q_TASK, 0,
                "Q_TASK RX observed frame header 7E6C",
                {}, true, true);
    }
}

/**
 * @brief 锁定已验证的串口名称
 * @param reason 验证原因（如 "cmd=0x5a"、"SET_COM ACK"）
 *
 * 当收到合法协议帧（已通过帧头 + CRC 双重校验）后调用此方法，
 * 将当前串口名锁定为"已验证端口"。后续串口硬错误恢复时优先重连此端口，
 * 避免因系统串口枚举顺序变化而连错设备。
 *
 * @note 仅在端口名首次锁定或发生变化时记录日志，避免重复输出。
 */
void KeySerialClient::markPortVerified(const QString &reason)
{
    if (!m_serial->isOpen()) {
        return;
    }

    const QString currentPort = m_serial->portName();
    if (currentPort.isEmpty()) {
        return;
    }

    const bool changed = (!m_hasVerifiedPort || m_verifiedPortName != currentPort);
    m_verifiedPortName = currentPort;
    m_hasVerifiedPort = true;
    if (changed) {
        log(QString("[VERIFIED] 锁定底座串口: %1 (%2)").arg(currentPort, reason));
        emitLog(LogDir::EVENT, 0, 0,
                QString("Verified port: %1 (%2)").arg(currentPort, reason));
    }
}

/**
 * @brief 判断串口错误是否为不可恢复的硬错误
 * @param error Qt 串口错误枚举值
 * @return true=硬错误（设备拔出/权限丢失等），需要关闭并尝试重连
 *
 * 硬错误类型：
 *   - ResourceError：设备资源丢失（USB 拔出最常见的表现）
 *   - DeviceNotFoundError：设备不存在（串口被系统移除）
 *   - PermissionError：权限被拒（其他进程占用或权限变更）
 */
bool KeySerialClient::isHardSerialError(int errorCode) const
{
    return errorCode == static_cast<int>(QSerialPort::ResourceError) ||
           errorCode == static_cast<int>(QSerialPort::DeviceNotFoundError) ||
           errorCode == static_cast<int>(QSerialPort::PermissionError);
}

/**
 * @brief 尝试重新打开已验证的串口（硬错误恢复）
 * @param reason 触发重连的错误描述
 *
 * 恢复策略：
 *   1. 优先使用已验证端口名（m_verifiedPortName），其次使用当前端口名
 *   2. 延迟 300ms 后尝试重连（给操作系统时间释放串口资源）
 *   3. 重连成功：通知 UI "串口已恢复"
 *   4. 重连失败：通知 UI "串口重连失败"
 *
 * @note m_reopenInProgress 锁防止并发重连（300ms 延迟期间可能再次触发错误）。
 * @note 重连使用 connectPort()，会重新执行完整的初始化和 SET_COM 握手流程。
 */
void KeySerialClient::tryReopenVerifiedPort(const QString &reason)
{
    if (m_reopenInProgress) {
        return;
    }

    // 防止无限重连循环：超过上限后停止，通知 UI 人工介入
    if (m_reopenRetryCount >= MAX_REOPEN_RETRIES) {
        emitLog(LogDir::ERROR, 0, 0,
                QString("重连已达上限(%1次)，停止自动重连: %2")
                    .arg(MAX_REOPEN_RETRIES).arg(reason));
        emit timeoutOccurred(QString("串口重连失败(已重试%1次): %2")
                                 .arg(MAX_REOPEN_RETRIES).arg(reason));
        m_reopenRetryCount = 0;
        return;
    }

    QString targetPort = m_hasVerifiedPort ? m_verifiedPortName : m_serial->portName();
    if (targetPort.isEmpty()) {
        emit timeoutOccurred(QString("串口硬错误: %1").arg(reason));
        return;
    }

    m_reopenInProgress = true;
    m_reopenRetryCount++;
    emitLog(LogDir::WARN, 0, 0,
            QString("串口硬错误，尝试重连同一端口 %1 (%2/%3)")
                .arg(targetPort).arg(m_reopenRetryCount).arg(MAX_REOPEN_RETRIES));

    QTimer::singleShot(300, this, [this, targetPort, reason]() {
        // 关键修复：m_reopenInProgress 必须在 connectPort 之后才释放，
        // 否则 connectPort 失败触发的 onSerialError 会重入本函数形成无限循环。
        if (connectPort(targetPort, m_currentBaud)) {
            m_reopenInProgress = false;
            m_reopenRetryCount = 0;
            emit timeoutOccurred(QString("串口已恢复: %1").arg(targetPort));
            return;
        }
        m_reopenInProgress = false;
        emit timeoutOccurred(QString("串口重连失败(%1): %2").arg(targetPort).arg(reason));
    });
}

/**
 * @brief QSerialPort::errorOccurred 信号回调，处理串口硬错误
 * @param error Qt 串口错误枚举值
 *
 * 处理流程：
 *   1. NoError 直接忽略（Qt 在某些操作后会发出 NoError）
 *   2. 非硬错误仅记录日志，不做处理
 *   3. 硬错误（ResourceError/DeviceNotFoundError/PermissionError）：
 *      a. 停止所有定时器，清除所有状态
 *      b. 关闭串口
 *      c. 通知 UI 连接已断开
 *      d. 尝试重新打开已验证端口（tryReopenVerifiedPort）
 *
 * @note USB 串口拔出时 Qt 通常报告 ResourceError，
 *       此时串口句柄已失效，必须先 close() 再尝试重连。
 */
void KeySerialClient::onSerialError(int errorCode, const QString &errorMessage)
{
    if (errorCode == static_cast<int>(QSerialPort::NoError)) {
        return;
    }

    const QString err = errorMessage.isEmpty() ? m_serial->errorString() : errorMessage;
    emitLog(LogDir::ERROR, 0, 0,
            QString("SerialError(%1): %2").arg(errorCode).arg(err));

    if (!isHardSerialError(errorCode)) {
        return;
    }

    stopRetryTimer();
    clearInFlight();
    m_keyStableTimer->stop();
    m_qtaskRecoveryTimer->stop();
    m_pendingAfterDelQuery = false;
    m_keyStable = false;
    m_sessionReady = false;
    m_deferredInFlightType = INFLIGHT_NONE;
    m_deferredDeleteTaskId.clear();
    clearTaskLogState();
    m_qtaskRecoveryRound = 0;
    m_keyPlacedElapsed.invalidate();
    m_lastNoiseElapsed.invalidate();
    m_qtaskSawFrameHeader = false;
    m_qtaskProbeInFlight = false;

    if (m_serial->isOpen()) {
        m_serial->close();
    }
    emit connectedChanged(false);
    emit keyStableChanged(false);
    tryReopenVerifiedPort(err);
}

/**
 * @brief 重试定时器超时回调
 *
 * 每次超时（RETRY_TIMEOUT_MS=1000ms）触发一次，处理逻辑：
 *
 * 1. 重试次数未达上限（< MAX_RETRIES=3）：
 *    重发 m_lastSentFrame，重启定时器
 *
 * 2. 重试次数达上限，按命令类型分别处理：
 *    a. startup probe Q_TASK 超时：静默放弃（钥匙确实不在位）
 *    b. 普通 Q_TASK 超时且钥匙在位：进入软超时恢复流程
 *       （延迟 QTASK_RECOVERY_DELAY_MS 后重试，最多 QTASK_RECOVERY_ROUNDS 轮）
 *    c. 其他命令超时：放弃当前命令，保持串口连接，
 *       若钥匙在位则重新发起 SET_COM 握手
 *
 * Q_TASK 超时诊断：
 *   通过 m_qtaskSawFrameHeader 判断设备是否有部分响应：
 *   - 观察到帧头：设备有响应但帧不完整（传输问题）
 *   - 未观察到帧头：设备完全无响应（钥匙/底座问题）
 */
void KeySerialClient::onRetryTimeout()
{
    m_retryCount++;
    if (m_retryCount >= MAX_RETRIES) {
        const quint8 timeoutCmd = m_expectedAckCmd;
        const quint8 timeoutInFlight = m_inFlightType;
        const bool qtaskSawHeader = m_qtaskSawFrameHeader;
        QString cmdName = QString("0x%1").arg(timeoutCmd, 2, 16, QChar('0')).toUpper();
        if (timeoutInFlight == INFLIGHT_QTASK) {
            cmdName = "Q_TASK";
        } else if (timeoutInFlight == INFLIGHT_TASKLOG) {
            cmdName = "I_TASK_LOG";
        } else if (timeoutInFlight == INFLIGHT_DEL) {
            cmdName = "DEL";
        } else if (timeoutInFlight == INFLIGHT_SETCOM) {
            cmdName = "SET_COM";
        } else if (timeoutInFlight == INFLIGHT_TICKET) {
            cmdName = QString("TICKET frame %1/%2")
                    .arg(m_ticketFrameIndex + 1)
                    .arg(m_ticketFrames.size());
        }

        if (timeoutInFlight == INFLIGHT_QTASK && m_qtaskProbeInFlight) {
            stopRetryTimer();
            clearInFlight();
            m_pendingAfterDelQuery = false;
            m_qtaskRecoveryRound = 0;
            m_qtaskRecoveryTimer->stop();
            m_qtaskProbeInFlight = false;
            m_qtaskSawFrameHeader = false;
            emitLog(LogDir::EVENT, CMD_Q_TASK, 0,
                    "Startup Q_TASK probe timeout, key remains not-present");
            return;
        }

        if (timeoutInFlight == INFLIGHT_QTASK &&
            m_keyPlaced &&
            m_qtaskRecoveryRound < QTASK_RECOVERY_ROUNDS) {
            stopRetryTimer();
            clearInFlight();
            m_pendingAfterDelQuery = false;
            m_qtaskRecoveryRound++;
            m_qtaskProbeInFlight = false;

            emitLog(LogDir::WARN, CMD_Q_TASK, 0,
                    QString("Q_TASK soft-timeout, delay retry %1/%2")
                        .arg(m_qtaskRecoveryRound)
                        .arg(QTASK_RECOVERY_ROUNDS));
            emitNotice(QString("Q_TASK 暂未响应，%1ms 后自动重试(%2/%3)")
                           .arg(QTASK_RECOVERY_DELAY_MS)
                           .arg(m_qtaskRecoveryRound)
                           .arg(QTASK_RECOVERY_ROUNDS));
            m_qtaskRecoveryTimer->start(QTASK_RECOVERY_DELAY_MS);
            return;
        }

        log(QString("[TIMEOUT] %1 重发 %2 次仍无响应，保持串口连接").arg(cmdName).arg(MAX_RETRIES));
        emitLog(LogDir::ERROR, timeoutCmd, 0,
                QString("%1 timeout after %2 retries, keep port open").arg(cmdName).arg(MAX_RETRIES));
        if (timeoutInFlight == INFLIGHT_TICKET) {
            emit ticketTransferFailed(QString("%1 超时").arg(cmdName));
        }
        stopRetryTimer();
        clearInFlight();
        clearTicketTransferState();
        clearTaskLogState();
        m_pendingAfterDelQuery = false;
        m_sessionReady = false;
        m_qtaskRecoveryRound = 0;
        m_qtaskRecoveryTimer->stop();
        m_qtaskProbeInFlight = false;
        m_qtaskSawFrameHeader = false;

        if (m_serial->isOpen()) {
            if (m_keyPlaced) {
                sendSetComHandshake(QString("timeout recovery after %1").arg(cmdName));
            } else {
                emitLog(LogDir::EVENT, 0, 0,
                        "钥匙未在位，等待重新放稳后自动重握手");
            }
        }

        if (timeoutInFlight == INFLIGHT_QTASK && !qtaskSawHeader) {
            emitLog(LogDir::WARN, CMD_Q_TASK, 0,
                    "Q_TASK timeout: no frame header observed",
                    {}, true, true);
            emit timeoutOccurred(QString("%1 查询超时（未收到帧头）").arg(cmdName));
        } else {
            emit timeoutOccurred(QString("%1 超时，连接保持，请重新放稳钥匙").arg(cmdName));
        }
        return;
    }

    log(QString("[RETRY] 第 %1/%2 次重发...").arg(m_retryCount).arg(MAX_RETRIES));

    // 生成重试摘要：标明是哪个命令的重试
    QString retrySummary = QString("Retry %1/%2")
        .arg(m_retryCount).arg(MAX_RETRIES);
    // 按 inFlightType 区分命令名，而非按期望ACK字节（SET_COM/DEL 期待同一个 ACK 无法区分）
    switch (m_inFlightType) {
    case INFLIGHT_SETCOM: retrySummary += " for SET_COM"; break;
    case INFLIGHT_DEL:    retrySummary += " for DEL"; break;
    case INFLIGHT_QTASK:  retrySummary += " for Q_TASK"; break;
    case INFLIGHT_TASKLOG: retrySummary += " for I_TASK_LOG"; break;
    case INFLIGHT_TICKET: retrySummary += QString(" for TICKET frame %1/%2")
                                           .arg(m_ticketFrameIndex + 1)
                                           .arg(m_ticketFrames.size()); break;
    default:
        retrySummary += QString(" for 0x%1").arg(m_expectedAckCmd, 2, 16, QChar('0'));
    }
    emitLog(LogDir::WARN, m_expectedAckCmd, 0, retrySummary);

    if (!m_lastSentFrame.isEmpty() && m_serial->isOpen()) {
        logHex("SEN(retry)", m_lastSentFrame);
        qint64 written = m_serial->write(m_lastSentFrame);
        const bool flushOk = m_serial->flush();
        const bool waitOk = m_serial->waitForBytesWritten(200);
        if (written != m_lastSentFrame.size() || !waitOk) {
            emitLog(LogDir::WARN, m_expectedAckCmd, 0,
                    QString("retry write: %1/%2 bytes, wait=%3")
                        .arg(written).arg(m_lastSentFrame.size())
                        .arg(waitOk ? "OK" : "FAIL"));
        }
        if (!flushOk) {
            emitLog(LogDir::WARN, m_expectedAckCmd, 0,
                    "retry flush returned false (non-fatal)", {}, true, true);
        }
    }
    m_retryTimer->start(RETRY_TIMEOUT_MS);
}

/**
 * @brief 钥匙稳定性窗口定时器到期回调
 *
 * 判定逻辑（双窗口机制）：
 *   1. 钥匙不在位 → 重置 keyStable=false 并返回
 *   2. 检查 m_keyPlacedElapsed：距钥匙放上是否已过 KEY_STABLE_WINDOW_MS(600ms)
 *      未满足 → 重启定时器等待剩余时间
 *   3. 检查 m_lastNoiseElapsed：距最后一次噪声是否已过 KEY_QUIET_WINDOW_MS(400ms)
 *      未满足 → 重启定时器等待剩余时间
 *   4. 两个窗口都满足 → 标记 keyStable=true，通知 UI，派发延迟命令
 *
 * 为什么需要双窗口：
 *   KEY_STABLE_WINDOW_MS 确保钥匙放上后有足够的观察时间；
 *   KEY_QUIET_WINDOW_MS 确保最后一次噪声后有足够的静默时间。
 *   两者取较晚者，确保噪声完全消退。
 */
void KeySerialClient::onKeyStableTimerTimeout()
{
    if (!m_keyPlaced) {
        if (m_keyStable) {
            m_keyStable = false;
            emit keyStableChanged(false);
        }
        return;
    }

    if (m_keyPlacedElapsed.isValid()) {
        const qint64 placedElapsed = m_keyPlacedElapsed.elapsed();
        if (placedElapsed < KEY_STABLE_WINDOW_MS) {
            m_keyStableTimer->start(KEY_STABLE_WINDOW_MS - static_cast<int>(placedElapsed));
            return;
        }
    }

    if (m_lastNoiseElapsed.isValid()) {
        const qint64 noiseElapsed = m_lastNoiseElapsed.elapsed();
        if (noiseElapsed < KEY_QUIET_WINDOW_MS) {
            m_keyStableTimer->start(KEY_QUIET_WINDOW_MS - static_cast<int>(noiseElapsed));
            return;
        }
    }

    if (!m_keyStable) {
        m_keyStable = true;
        emit keyStableChanged(true);
        emitLog(LogDir::EVENT, 0, 0, "KEY_STABLE_PRESENT");
        emitNotice("钥匙已就绪，可发送 Q_TASK/DEL");
    }
    tryDispatchDeferredCommand();
}

/**
 * @brief Q_TASK 软超时恢复定时器回调
 *
 * 当 Q_TASK 首次超时但钥匙仍在位时，不立即放弃，而是延迟后重试。
 * 此方法由 m_qtaskRecoveryTimer 触发，委托给 queryTasksAllInternal(fromRecovery=true)。
 *
 * 重试条件：串口已打开且钥匙在位。
 * 最大重试轮数由 QTASK_RECOVERY_ROUNDS(2) 控制。
 */
void KeySerialClient::onQTaskRecoveryTimeout()
{
    if (!m_serial->isOpen() || !m_keyPlaced) {
        return;
    }
    queryTasksAllInternal(true);
}

// ============================================================
// 日志
//
// 双通道日志架构：
//   1. logLine 信号：旧版文本日志，格式 "[HH:mm:ss.zzz] 消息"，
//      用于调试控制台和兼容旧代码
//   2. logItemEmitted 信号：结构化日志（LogItem），
//      用于 UI 表格显示、CSV 导出、过滤筛选
// ============================================================

/**
 * @brief 发射带时间戳的文本日志
 * @param msg 日志消息内容
 *
 * 格式：[HH:mm:ss.zzz] 消息内容
 * 通过 logLine 信号发出，供调试面板或控制台显示。
 */
void KeySerialClient::log(const QString &msg)
{
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    emit logLine(QString("[%1] %2").arg(timestamp, msg));
}

/**
 * @brief 发射帧数据的十六进制日志
 * @param direction 方向标识（"SEN"=发送, "REC"=接收, "SEN(retry)"=重试发送）
 * @param data 帧原始字节数据
 *
 * 将帧数据转为大写十六进制字符串（空格分隔），格式：SEN: 7E 6C FF 00 ...
 */
void KeySerialClient::logHex(const QString &direction, const QByteArray &data)
{
    QString hex = data.toHex(' ').toUpper();
    log(QString("%1: %2").arg(direction, hex));
}

/**
 * @brief 构造并发射结构化日志条目
 *
 * 同时保留旧 logLine 信号的兼容输出，确保两套日志通道并行工作。
 * KEY_EVENT 等异步事件的 opId 固定为 -1，不受 m_currentOpId 影响。
 *
 * @param dir      日志方向/类型
 * @param cmd      协议命令字（非协议帧传 0）
 * @param lenField 协议 Len 字段（非协议帧传 0）
 * @param summary  用户友好摘要
 * @param hex      原始帧数据（可选）
 * @param crcOk    CRC 校验结果（默认 true）
 */
void KeySerialClient::emitLog(LogDir dir, quint8 cmd, quint16 lenField,
                              const QString &summary, const QByteArray &hex,
                              bool crcOk, bool expertOnly)
{
    LogItem item;
    item.timestamp = QDateTime::currentDateTime();
    item.dir      = dir;
    item.opId     = (dir == LogDir::EVENT) ? -1 : m_currentOpId;
    item.cmd      = cmd;
    item.length   = lenField;
    item.summary  = summary;
    item.hex      = hex;
    item.crcOk    = crcOk;
    item.expertOnly = expertOnly;

    emit logItemEmitted(item);
}

/**
 * @brief 发射用户友好通知并同时记录结构化日志
 * @param what 通知内容（如 "钥匙已就绪，可发送 Q_TASK/DEL"）
 *
 * 双重输出：
 *   1. notice 信号 → UI 层 statusBar 或 toast 显示
 *   2. emitLog(EVENT) → 结构化日志表格记录
 */
void KeySerialClient::emitNotice(const QString &what)
{
    emit notice(what);
    emitLog(LogDir::EVENT, 0, 0, what);
}
