#include "SerialTestWidget.h"

#include <QApplication>
#include <QPushButton>
#include <QSpinBox>
#include <QTextEdit>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSerialPort>
#include <QDateTime>
#include <QDebug>

// ============================================================
// 协议常量
// ============================================================
namespace Proto {
    static constexpr quint8  HEADER_0        = 0x7E;
    static constexpr quint8  HEADER_1        = 0x6C;
    static constexpr quint8  REQ_KEYID       = 0xFF;

    // 帧字段偏移
    static constexpr int OFF_HEADER  = 0;
    static constexpr int OFF_KEYID   = 2;
    static constexpr int OFF_FRAMENO = 3;
    static constexpr int OFF_ADDR2   = 4;
    static constexpr int OFF_LEN     = 6;
    static constexpr int OFF_CMD     = 8;
    static constexpr int OFF_DATA    = 9;
    static constexpr int FRAME_OVERHEAD = 11; ///< Header(2)+KeyId+FrameNo+Addr2(2)+Len(2)+Cmd+CRC(2)
    static constexpr int MIN_FRAME_LEN  = 11; ///< Data 为空时最小帧长

    // 命令码
    static constexpr quint8 CMD_SET_COM   = 0x0F;
    static constexpr quint8 CMD_Q_TASK    = 0x04;
    static constexpr quint8 CMD_DEL       = 0x06;
    static constexpr quint8 CMD_ACK       = 0x5A;
    static constexpr quint8 CMD_NAK       = 0x00;
    static constexpr quint8 CMD_KEY_EVENT = 0x11; ///< 设备主动推送：钥匙插入/取出
    static constexpr quint8 CMD_Q_KEYEQ   = 0x14; ///< 查询钥匙电量 / 响应电量上报

    // Q_KEYEQ 响应数据长度： Byte0=电量, Byte1-7=保留 0xFF
    static constexpr int KEYEQ_DATA_LEN   = 8;
    static constexpr quint8 KEYEQ_INVALID = 0xFF; ///< 无效电量标记

    // addr2
    // 规则（抓包验证）：
    //   钥匙类命令 SET_COM / Q_TASK / Q_KEYEQ / SET_TIME — TX 固定 Addr2=0x0000
    //   站点命令  DEL(0x06)                            — TX Addr2 = stationId（小端 Lo Hi）
    //   回包                                           — Addr2 常为 0x00FF，不作过滤依据
    static const QByteArray ADDR2_DEFAULT = QByteArray("\x00\x00", 2); ///< 钥匙类命令固定用此值
    // DEL 的 Addr2 由 stationId 动态生成，见 buildAddr2ForDel()

    // Q_TASK 数据
    static constexpr int TASK_ENTRY_LEN  = 20;  ///< taskId(16) + status + type + source + reserved
    static constexpr int TASK_ID_LEN     = 16;

    // 超时与重试
    static constexpr int RETRY_TIMEOUT_MS = 1000;
    static constexpr int MAX_RETRIES      = 3;

    // NAK 错误码
    static constexpr quint8 ERR_CRC_FAIL     = 0x03;
    static constexpr quint8 ERR_LEN_MISMATCH = 0x04;
    static constexpr quint8 ERR_GUID_MISSING = 0x08;
}

// ============================================================
// CRC-16 查找表（原样复刻，真机验证通过，禁止替换为其他变种）
// 算法：右移，(crc >> 8) ^ table[(byte ^ crc) & 0xFF]
// ============================================================
static const quint16 kCrcTable[256] = {
    0x0000,0x1021,0x2042,0x3063,0x4084,0x50A5,0x60C6,0x70E7,
    0x8108,0x9129,0xA14A,0xB16B,0xC18C,0xD1AD,0xE1CE,0xF1EF,
    0x1231,0x0210,0x3273,0x2252,0x52B5,0x4294,0x72F7,0x62D6,
    0x9339,0x8318,0xB37B,0xA35A,0xD3BD,0xC39C,0xF3FF,0xE3DE,
    0x2462,0x3443,0x0420,0x1401,0x64E6,0x74C7,0x44A4,0x5485,
    0xA56A,0xB54B,0x8528,0x9509,0xE5EE,0xF5CF,0xC5AC,0xD58D,
    0x3653,0x2672,0x1611,0x0630,0x76D7,0x66F6,0x5695,0x46B4,
    0xB75B,0xA77A,0x9719,0x8738,0xF7DF,0xE7FE,0xD79D,0xC7BC,
    0x48C4,0x58E5,0x6886,0x78A7,0x0840,0x1861,0x2802,0x3823,
    0xC9CC,0xD9ED,0xE98E,0xF9AF,0x8948,0x9969,0xA90A,0xB92B,
    0x5AF5,0x4AD4,0x7AB7,0x6A96,0x1A71,0x0A50,0x3A33,0x2A12,
    0xDBFD,0xCBDC,0xFBBF,0xEB9E,0x9B79,0x8B58,0xBB3B,0xAB1A,
    0x6CA6,0x7C87,0x4CE4,0x5CC5,0x2C22,0x3C03,0x0C60,0x1C41,
    0xEDAE,0xFD8F,0xCDEC,0xDDCD,0xAD2A,0xBD0B,0x8D68,0x9D49,
    0x7E97,0x6EB6,0x5ED5,0x4EF4,0x3E13,0x2E32,0x1E51,0x0E70,
    0xFF9F,0xEFBE,0xDFDD,0xCFFC,0xBF1B,0xAF3A,0x9F59,0x8F78,
    0x9188,0x81A9,0xB1CA,0xA1EB,0xD10C,0xC12D,0xF14E,0xE16F,
    0x1080,0x00A1,0x30C2,0x20E3,0x5004,0x4025,0x7046,0x6067,
    0x83B9,0x9398,0xA3FB,0xB3DA,0xC33D,0xD31C,0xE37F,0xF35E,
    0x02B1,0x1290,0x22F3,0x32D2,0x4235,0x5214,0x6277,0x7256,
    0xB5EA,0xA5CB,0x95A8,0x8589,0xF56E,0xE54F,0xD52C,0xC50D,
    0x34E2,0x24C3,0x14A0,0x0481,0x7466,0x6447,0x5424,0x4405,
    0xA7DB,0xB7FA,0x8799,0x97B8,0xE75F,0xF77E,0xC71D,0xD73C,
    0x26D3,0x36F2,0x0691,0x16B0,0x6657,0x7676,0x4615,0x5634,
    0xD94C,0xC96D,0xF90E,0xE92F,0x99C8,0x89E9,0xB98A,0xA9AB,
    0x5844,0x4865,0x7806,0x6827,0x18C0,0x08E1,0x3882,0x28A3,
    0xCB7D,0xDB5C,0xEB3F,0xFB1E,0x8BF9,0x9BD8,0xABBB,0xBB9A,
    0x4A75,0x5A54,0x6A37,0x7A16,0x0AF1,0x1AD0,0x2AB3,0x3A92,
    0xFD2E,0xED0F,0xDD6C,0xCD4D,0xBDAA,0xAD8B,0x9DE8,0x8DC9,
    0x7C26,0x6C07,0x5C64,0x4C45,0x3CA2,0x2C83,0x1CE0,0x0CC1,
    0xEF1F,0xFF3E,0xCF5D,0xDF7C,0xAF9B,0xBFBA,0x8FD9,0x9FF8,
    0x6E17,0x7E36,0x4E55,0x5E74,0x2E93,0x3EB2,0x0ED1,0x1EF0
};

// ============================================================
// CRC 计算（校验范围：Cmd + Data）
// ============================================================
quint16 SerialTestWidget::calcCrc(const QByteArray &cmdPlusData)
{
    quint16 crc = 0x0000;
    for (int i = 0; i < cmdPlusData.size(); ++i) {
        const quint8 b = static_cast<quint8>(cmdPlusData[i]);
        crc = (crc >> 8) ^ kCrcTable[(b ^ (crc & 0xFF)) & 0xFF];
    }
    return crc;
}

// ============================================================
// 启动自测
// ============================================================
bool SerialTestWidget::selfTestCrc()
{
    // 向量 1：[0F 01] -> 0x1C11
    {
        QByteArray d; d.append(static_cast<char>(0x0F)); d.append(static_cast<char>(0x01));
        if (calcCrc(d) != 0x1C11) return false;
    }
    // 向量 2：[04 FF*16] -> 0xF326
    {
        QByteArray d; d.append(static_cast<char>(0x04));
        d.append(QByteArray(16, static_cast<char>(0xFF)));
        if (calcCrc(d) != 0xF326) return false;
    }
    // 向量 3：[11 01 00 00 00] -> 0xD162 (KEY_PLACED)
    {
        QByteArray d;
        d.append(static_cast<char>(0x11));
        d.append(static_cast<char>(0x01));
        d.append(static_cast<char>(0x00));
        d.append(static_cast<char>(0x00));
        d.append(static_cast<char>(0x00));
        if (calcCrc(d) != 0xD162) return false;
    }
    // 向量 4：[11 00 00 00 00] -> 0x44A0 (KEY_REMOVED)
    {
        QByteArray d;
        d.append(static_cast<char>(0x11));
        d.append(QByteArray(4, static_cast<char>(0x00)));
        if (calcCrc(d) != 0x44A0) return false;
    }
    return true;
}

// ============================================================
// 构造函数：UI 布局 + 串口初始化 + CRC 自测
// ============================================================
SerialTestWidget::SerialTestWidget(QWidget *parent)
    : QWidget(parent)
    , m_btnConnect(new QPushButton("连接 COM10", this))
    , m_btnQuery  (new QPushButton("查询任务 (Q_TASK)", this))
    , m_btnDelete (new QPushButton("删除第一张 (DEL)", this))
    , m_btnQueryBattery(new QPushButton("查询电量 (Q_KEYEQ)", this))
    , m_logEdit   (new QTextEdit(this))
    , m_statusLabel(new QLabel("未连接", this))
    , m_batteryLabel(new QLabel("电量：--", this))
    , m_spinStationId(new QSpinBox(this))
    , m_serial    (new QSerialPort(this))
    , m_frameNo   (0)
    , m_retryTimer(new QTimer(this))
    , m_expectedAckCmd(0)
    , m_retryCount(0)
    , m_pendingAfterDelQuery(false)
    , m_handshakeDone(false)
    , m_pendingBatteryQuery(false)
{
    // --- 站号 SpinBox ---
    m_spinStationId->setRange(1, 99);
    m_spinStationId->setValue(1);
    m_spinStationId->setPrefix("站号: ");
    m_spinStationId->setToolTip("仅影响 DEL 命令的 Addr2（TX: Lo=stationId Hi=00）\n"
                                "钥匙类命令 SET_COM/Q_TASK/Q_KEYEQ 的 Addr2 始终为 00 00");
    m_spinStationId->setFixedWidth(90);

    // --- 布局 ---
    auto *btnRow = new QHBoxLayout;
    btnRow->addWidget(m_btnConnect);
    btnRow->addWidget(m_btnQuery);
    btnRow->addWidget(m_btnDelete);
    btnRow->addWidget(m_btnQueryBattery);
    btnRow->addWidget(m_spinStationId);
    btnRow->addStretch();
    btnRow->addWidget(m_batteryLabel);  // 电量显示靠右排列

    auto *root = new QVBoxLayout(this);
    root->addLayout(btnRow);
    root->addWidget(m_statusLabel);
    root->addWidget(m_logEdit);

    m_logEdit->setReadOnly(true);
    m_logEdit->setFont(QFont("Consolas", 9));
    m_btnQuery ->setEnabled(false);
    m_btnDelete->setEnabled(false);
    m_btnQueryBattery->setEnabled(false);
    m_batteryLabel->setMinimumWidth(100);
    m_batteryLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    // --- 串口信号 ---
    connect(m_serial, &QSerialPort::readyRead,
            this, &SerialTestWidget::onReadyRead);
    connect(m_serial, &QSerialPort::errorOccurred,
            this, &SerialTestWidget::onSerialErrorOccurred);

    // --- 重试定时器 ---
    m_retryTimer->setSingleShot(true);
    m_retryTimer->setInterval(Proto::RETRY_TIMEOUT_MS);
    connect(m_retryTimer, &QTimer::timeout,
            this, &SerialTestWidget::onRetryTimeout);

    // --- 按钮信号 ---
    connect(m_btnConnect,      &QPushButton::clicked, this, &SerialTestWidget::onConnectClicked);
    connect(m_btnQuery,        &QPushButton::clicked, this, &SerialTestWidget::onQueryClicked);
    connect(m_btnDelete,       &QPushButton::clicked, this, &SerialTestWidget::onDeleteClicked);
    connect(m_btnQueryBattery, &QPushButton::clicked, this, &SerialTestWidget::onQueryBatteryClicked);

    // --- CRC 自测 ---
    const bool crcOk = selfTestCrc();
    log(QString("[CRC自测] %1").arg(crcOk ? "PASS" : "FAIL - kCrcTable 校验未通过！"));
}

SerialTestWidget::~SerialTestWidget()
{
    if (m_serial->isOpen())
        m_serial->close();
}

// ============================================================
// 按钮：连接 COM10
// ============================================================
void SerialTestWidget::onConnectClicked()
{
    if (m_serial->isOpen()) {
        m_serial->close();
        m_recvBuffer.clear();
        m_frameNo = 0;
        m_taskIds.clear();
        stopRetryTimer();
        m_handshakeDone        = false;
        m_pendingBatteryQuery  = false;
        m_btnQuery ->setEnabled(false);
        m_btnDelete->setEnabled(false);
        m_btnQueryBattery->setEnabled(false);
        m_batteryLabel->setText("电量：--");
        m_btnConnect->setText("连接 COM10");
        m_statusLabel->setText("已断开");
        log("[INFO] 串口已关闭");
        return;
    }

    m_serial->setPortName("COM10");
    m_serial->setBaudRate(QSerialPort::Baud115200);
    m_serial->setDataBits(QSerialPort::Data8);
    m_serial->setParity(QSerialPort::NoParity);
    m_serial->setStopBits(QSerialPort::OneStop);
    m_serial->setFlowControl(QSerialPort::NoFlowControl);

    if (!m_serial->open(QIODevice::ReadWrite)) {
        log(QString("[ERR] 无法打开 COM10: %1").arg(m_serial->errorString()));
        return;
    }

    m_recvBuffer.clear();
    m_frameNo = 0;
    m_taskIds.clear();
    m_pendingAfterDelQuery = false;
    m_handshakeDone        = false;
    m_pendingBatteryQuery  = false;
    m_batteryLabel->setText("电量：--");

    m_btnConnect->setText("断开 COM10");
    m_btnQuery ->setEnabled(true);
    m_btnQueryBattery->setEnabled(true);
    m_statusLabel->setText("已连接 COM10 115200/8N1");
    log("[INFO] COM10 已打开，发送 SET_COM...");

    // 连接后立即发握手
    QByteArray data;
    data.append(static_cast<char>(0x01));
    sendFrame(buildFrame(Proto::CMD_SET_COM, data, Proto::ADDR2_DEFAULT), Proto::CMD_SET_COM);
}

// ============================================================
// 按钮：查询任务
// ============================================================
void SerialTestWidget::onQueryClicked()
{
    if (!m_serial->isOpen()) { log("[WARN] 串口未打开"); return; }
    QByteArray data(16, static_cast<char>(0xFF));
    sendFrame(buildFrame(Proto::CMD_Q_TASK, data, Proto::ADDR2_DEFAULT), Proto::CMD_Q_TASK);
}

// ============================================================
// 按钮：删除第一张任务
// ============================================================
void SerialTestWidget::onDeleteClicked()
{
    if (!m_serial->isOpen()) { log("[WARN] 串口未打开"); return; }
    if (m_taskIds.isEmpty()) {
        log("[WARN] 没有可删除的任务，请先执行 Q_TASK");
        return;
    }
    const QByteArray taskId = m_taskIds.first();
    // Addr2 = stationId（小端），抓包验证：站号 1 → 01 00，站号 2 → 02 00
    const quint16 stationId = static_cast<quint16>(m_spinStationId->value());
    QByteArray addr2Del;
    addr2Del.append(static_cast<char>(stationId & 0xFF));        // Lo
    addr2Del.append(static_cast<char>((stationId >> 8) & 0xFF)); // Hi
    log(QString("[DEL] stationId=%1  Addr2=%2  taskId: %3")
            .arg(stationId)
            .arg(QString(addr2Del.toHex(' ').toUpper()))
            .arg(QString(taskId.toHex(' ').toUpper())));
    m_pendingAfterDelQuery = true;
    sendFrame(buildFrame(Proto::CMD_DEL, taskId, addr2Del), Proto::CMD_DEL);
}

// ============================================================
// 按钮：查询电量 (Q_KEYEQ)
// ============================================================
void SerialTestWidget::onQueryBatteryClicked()
{
    if (!m_serial->isOpen()) { log("[WARN] 串口未打开"); return; }

    if (!m_handshakeDone) {
        log("[INFO] 尚未完成握手，先发 SET_COM，握手后自动发电量查询...");
        m_pendingBatteryQuery = true;
        QByteArray data;
        data.append(static_cast<char>(0x01));
        sendFrame(buildFrame(Proto::CMD_SET_COM, data, Proto::ADDR2_DEFAULT),
                  Proto::CMD_SET_COM);
        return;
    }

    // 已握手：直接发送 Q_KEYEQ，Data 为空
    sendFrame(buildFrame(Proto::CMD_Q_KEYEQ, QByteArray(), Proto::ADDR2_DEFAULT),
              Proto::CMD_Q_KEYEQ);
}

// ============================================================
// 内部：解析电量响应帧 (UP_KEYEQ, Cmd=0x14)
// ============================================================
void SerialTestWidget::handleKeyEqResp(const QByteArray &frame)
{
    const quint16 lenField =
        static_cast<quint8>(frame[Proto::OFF_LEN]) |
        (static_cast<quint8>(frame[Proto::OFF_LEN + 1]) << 8);
    const int     dataLen = static_cast<int>(lenField) - 1; // 减去 Cmd 自身
    const QByteArray data = frame.mid(Proto::OFF_DATA, dataLen);

    if (dataLen < 1) {
        log("[Q_KEYEQ] 响应数据不足 1 字节");
        return;
    }

    stopRetryTimer();   // 成功收到响应，立即停止重试

    const quint8 raw = static_cast<quint8>(data[0]);
    if (raw == Proto::KEYEQ_INVALID) {
        m_batteryLabel->setText("电量：--");
        log(QString("[Q_KEYEQ] keyId=0x%1  电量无效(0xFF)")
                .arg(static_cast<quint8>(frame[Proto::OFF_KEYID]), 2, 16, QChar('0')));
    } else {
        const int pct = qBound(0, static_cast<int>(raw), 100);
        m_batteryLabel->setText(QString("电量：%1%").arg(pct));
        log(QString("[Q_KEYEQ] keyId=0x%1  电量=%2% (raw=0x%3)")
                .arg(static_cast<quint8>(frame[Proto::OFF_KEYID]), 2, 16, QChar('0'))
                .arg(pct)
                .arg(raw, 2, 16, QChar('0')));
    }
}

// ============================================================
// 帧构建
// ============================================================
QByteArray SerialTestWidget::buildFrame(quint8 cmd, const QByteArray &data,
                                        const QByteArray &addr2)
{
    QByteArray frame;
    frame.append(static_cast<char>(Proto::HEADER_0));
    frame.append(static_cast<char>(Proto::HEADER_1));
    frame.append(static_cast<char>(Proto::REQ_KEYID));
    frame.append(static_cast<char>(m_frameNo++));

    // addr2（2 字节，LE）
    frame.append(addr2.left(2));

    // Len = Cmd(1) + Data(N)，LE 小端
    const quint16 len = static_cast<quint16>(1 + data.size());
    frame.append(static_cast<char>(len & 0xFF));
    frame.append(static_cast<char>((len >> 8) & 0xFF));

    // Cmd + Data
    frame.append(static_cast<char>(cmd));
    frame.append(data);

    // CRC（只对 Cmd+Data，BE 大端：hi 在前）
    QByteArray cmdPlusData;
    cmdPlusData.append(static_cast<char>(cmd));
    cmdPlusData.append(data);
    const quint16 crc = calcCrc(cmdPlusData);
    frame.append(static_cast<char>((crc >> 8) & 0xFF));
    frame.append(static_cast<char>(crc & 0xFF));

    return frame;
}

// ============================================================
// 发送帧并启动重试计时器
// ============================================================
void SerialTestWidget::sendFrame(const QByteArray &frame, quint8 expectedAckCmd)
{
    m_lastSentFrame   = frame;
    m_expectedAckCmd  = expectedAckCmd;
    m_retryCount      = 0;

    m_serial->write(frame);
    log(QString("SEN: %1").arg(QString(frame.toHex(' ').toUpper())));
    startRetryTimer();
}

void SerialTestWidget::startRetryTimer()
{
    m_retryTimer->start(Proto::RETRY_TIMEOUT_MS);
}

void SerialTestWidget::stopRetryTimer()
{
    m_retryTimer->stop();
    m_expectedAckCmd = 0;
}

// ============================================================
// 重试超时
// ============================================================
void SerialTestWidget::onRetryTimeout()
{
    m_retryCount++;
    if (m_retryCount > Proto::MAX_RETRIES) {
        log(QString("[TIMEOUT] 命令 0x%1 重试 %2 次无响应，放弃")
                .arg(m_expectedAckCmd, 2, 16, QChar('0'))
                .arg(Proto::MAX_RETRIES));
        stopRetryTimer();
        return;
    }
    log(QString("[RETRY %1/%2] 重发: %3")
            .arg(m_retryCount).arg(Proto::MAX_RETRIES)
            .arg(QString(m_lastSentFrame.toHex(' ').toUpper())));
    m_serial->write(m_lastSentFrame);
    startRetryTimer();
}

// ============================================================
// 数据到达：拼接缓冲，循环提取完整帧
// ============================================================
void SerialTestWidget::onReadyRead()
{
    m_recvBuffer.append(m_serial->readAll());

    QByteArray frame;
    while (tryExtractOneFrame(m_recvBuffer, frame) > 0) {
        // 打印原始帧 + 诊断字段（Addr2 不做过滤，仅记录）
        const quint16 rxAddr2 =
            (frame.size() > Proto::OFF_ADDR2 + 1)
            ? (static_cast<quint8>(frame[Proto::OFF_ADDR2]) |
               (static_cast<quint8>(frame[Proto::OFF_ADDR2 + 1]) << 8))
            : 0xFFFF;
        log(QString("REC [Addr2=0x%1]: %2")
                .arg(rxAddr2, 4, 16, QChar('0'))
                .arg(QString(frame.toHex(' ').toUpper())));
        handleFrame(frame);
    }
}

// ============================================================
// 拆包：从 buf 中提取一帧（含 resync）
// 返回 1 = 成功提取（outFrame 已填充，buf 已消费）；0 = 数据不足
// ============================================================
int SerialTestWidget::tryExtractOneFrame(QByteArray &buf, QByteArray &outFrame)
{
    while (buf.size() >= Proto::MIN_FRAME_LEN) {
        // 1) 找帧头
        if (static_cast<quint8>(buf[0]) != Proto::HEADER_0 ||
            static_cast<quint8>(buf[1]) != Proto::HEADER_1) {
            log(QString("[RESYNC] 丢弃脏字节: 0x%1")
                    .arg(static_cast<quint8>(buf[0]), 2, 16, QChar('0')));
            buf.remove(0, 1);
            continue;
        }

        // 2) 读 Len（小端）
        const quint16 lenField =
            static_cast<quint8>(buf[Proto::OFF_LEN]) |
            (static_cast<quint8>(buf[Proto::OFF_LEN + 1]) << 8);

        // Len 合法范围：1 ~ 515（Cmd 至少 1 字节，Data 最多 514 字节）
        if (lenField < 1 || lenField > 515) {
            log(QString("[RESYNC] Len 异常(%1)，丢弃帧头").arg(lenField));
            buf.remove(0, 1);
            continue;
        }

        // 3) 期望总帧长 = Header(2)+KeyId+FrameNo+Addr2(2)+Len(2) + lenField字节 + CRC(2)
        //               = 8 + lenField + 2  (= OFF_CMD + lenField + 2)
        const int totalLen = 8 + static_cast<int>(lenField) + 2;
        if (buf.size() < totalLen)
            return 0;   // 数据不够，等待更多

        // 4) CRC 校验
        const QByteArray cmdPlusData = buf.mid(Proto::OFF_CMD, static_cast<int>(lenField));
        const quint16 calcedCrc = calcCrc(cmdPlusData);
        const quint16 frameCrc  =
            (static_cast<quint8>(buf[totalLen - 2]) << 8) |
             static_cast<quint8>(buf[totalLen - 1]);

        if (calcedCrc != frameCrc) {
            log(QString("[CRC FAIL] 计算=0x%1 帧内=0x%2，滑窗 1 字节")
                    .arg(calcedCrc, 4, 16, QChar('0'))
                    .arg(frameCrc,  4, 16, QChar('0')));
            buf.remove(0, 1);
            continue;
        }

        // 5) 提取并消费
        outFrame = buf.left(totalLen);
        buf.remove(0, totalLen);
        return 1;
    }
    return 0;
}

// ============================================================
// 帧解析与命令分发
// ============================================================
void SerialTestWidget::handleFrame(const QByteArray &frame)
{
    const quint16 lenField =
        static_cast<quint8>(frame[Proto::OFF_LEN]) |
        (static_cast<quint8>(frame[Proto::OFF_LEN + 1]) << 8);

    const quint8 cmd     = static_cast<quint8>(frame[Proto::OFF_CMD]);
    const int    dataLen = static_cast<int>(lenField) - 1;
    const QByteArray data = frame.mid(Proto::OFF_DATA, dataLen);

    // ------ ACK (0x5A) ------
    if (cmd == Proto::CMD_ACK) {
        const quint8 ackedCmd = dataLen > 0 ? static_cast<quint8>(data[0]) : 0;
        log(QString("[ACK] 被确认命令: 0x%1").arg(ackedCmd, 2, 16, QChar('0')));

        // 匹配期望 ACK，停止重试
        if (ackedCmd == m_expectedAckCmd)
            stopRetryTimer();
        // SET_COM ACK → 标记握手完成
        if (ackedCmd == Proto::CMD_SET_COM) {
            m_handshakeDone = true;
            log("[INFO] 握手完成");

            // 若有待缓电量查询，自动触发
            if (m_pendingBatteryQuery) {
                m_pendingBatteryQuery = false;
                log("[INFO] 握手完成，自动发电量查询...");
                sendFrame(buildFrame(Proto::CMD_Q_KEYEQ, QByteArray(), Proto::ADDR2_DEFAULT),
                          Proto::CMD_Q_KEYEQ);
                return;
            }
        }
        // DEL ACK 后自动再发 Q_TASK 验证
        if (ackedCmd == Proto::CMD_DEL && m_pendingAfterDelQuery) {
            m_pendingAfterDelQuery = false;
            log("[INFO] DEL 成功，自动发 Q_TASK 验证任务数...");
            QByteArray d(16, static_cast<char>(0xFF));
            sendFrame(buildFrame(Proto::CMD_Q_TASK, d, Proto::ADDR2_DEFAULT), Proto::CMD_Q_TASK);
        }
        return;
    }

    // ------ NAK (0x00) ------
    if (cmd == Proto::CMD_NAK) {
        const quint8 origCmd = dataLen > 0 ? static_cast<quint8>(data[0]) : 0;
        const quint8 errCode = dataLen > 1 ? static_cast<quint8>(data[1]) : 0;
        QString errDesc;
        switch (errCode) {
        case Proto::ERR_CRC_FAIL:     errDesc = "CRC 校验失败";  break;
        case Proto::ERR_LEN_MISMATCH: errDesc = "字节数不符";    break;
        case Proto::ERR_GUID_MISSING: errDesc = "GUID 不存在";   break;
        default: errDesc = QString("未知错误码(0x%1)").arg(errCode, 2, 16, QChar('0')); break;
        }
        log(QString("[NAK] origCmd=0x%1 errCode=0x%2 (%3)")
                .arg(origCmd, 2, 16, QChar('0'))
                .arg(errCode, 2, 16, QChar('0'))
                .arg(errDesc));
        stopRetryTimer();
        return;
    }

    // ------ Q_TASK 响应 (0x04) ------
    if (cmd == Proto::CMD_Q_TASK) {
        if (dataLen < 1) {
            log("[Q_TASK] 响应数据长度不足");
            return;
        }

        // 匹配期望，停止重试
        if (m_expectedAckCmd == Proto::CMD_Q_TASK)
            stopRetryTimer();

        const int count = static_cast<quint8>(data[0]);
        log(QString("[Q_TASK] count = %1").arg(count));

        m_taskIds.clear();
        const int expectedDataLen = 1 + count * Proto::TASK_ENTRY_LEN;
        if (dataLen < expectedDataLen) {
            log(QString("[Q_TASK] 数据不完整，期望 %1 字节，实际 %2 字节")
                    .arg(expectedDataLen).arg(dataLen));
            return;
        }

        for (int i = 0; i < count; ++i) {
            const int offset = 1 + i * Proto::TASK_ENTRY_LEN;
            QByteArray taskId = data.mid(offset, Proto::TASK_ID_LEN);
            const quint8 status   = static_cast<quint8>(data[offset + 16]);
            const quint8 type     = static_cast<quint8>(data[offset + 17]);
            const quint8 source   = static_cast<quint8>(data[offset + 18]);
            const quint8 reserved = static_cast<quint8>(data[offset + 19]);
            m_taskIds.append(taskId);
            log(QString("[Q_TASK] 任务[%1] taskId=%2  status=0x%3 type=0x%4 source=0x%5 reserved=0x%6")
                    .arg(i)
                    .arg(QString(taskId.toHex(' ').toUpper()))
                    .arg(status,   2, 16, QChar('0'))
                    .arg(type,     2, 16, QChar('0'))
                    .arg(source,   2, 16, QChar('0'))
                    .arg(reserved, 2, 16, QChar('0')));
        }

        if (count > 0)
            m_btnDelete->setEnabled(true);
        else
            m_btnDelete->setEnabled(false);

        return;
    }

    // ------ KEY_EVENT (0x11) — 设备主动推送，无需应答 ------
    if (cmd == Proto::CMD_KEY_EVENT) {
        // data[0]: 0x00=钥匙取出, 0x01=钥匙插入; data[1..3]: 保留
        if (dataLen >= 1) {
            const quint8 state = static_cast<quint8>(data[0]);
            if (state == 0x01)
                log(QString("[KEY_EVENT] keyId=0x%1  *** 钥匙插入 ***")
                        .arg(static_cast<quint8>(frame[Proto::OFF_KEYID]), 2, 16, QChar('0')));
            else if (state == 0x00)
                log(QString("[KEY_EVENT] keyId=0x%1  --- 钥匙取出 ---")
                        .arg(static_cast<quint8>(frame[Proto::OFF_KEYID]), 2, 16, QChar('0')));
            else
                log(QString("[KEY_EVENT] keyId=0x%1  state=0x%2")
                        .arg(static_cast<quint8>(frame[Proto::OFF_KEYID]), 2, 16, QChar('0'))
                        .arg(state, 2, 16, QChar('0')));
        } else {
            log(QString("[KEY_EVENT] keyId=0x%1 (data empty)")
                    .arg(static_cast<quint8>(frame[Proto::OFF_KEYID]), 2, 16, QChar('0')));
        }
        return;
    }

    // ------ Q_KEYEQ 响应 (0x14) ------
    if (cmd == Proto::CMD_Q_KEYEQ) {
        handleKeyEqResp(frame);
        return;
    }

    // ------ 未知命令 ------
    log(QString("[UNKNOWN] cmd=0x%1 dataLen=%2").arg(cmd, 2, 16, QChar('0')).arg(dataLen));
}

// ============================================================
// 串口错误处理
// ============================================================
void SerialTestWidget::onSerialErrorOccurred(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::NoError) return;
    log(QString("[SER ERR] %1 (%2)").arg(m_serial->errorString()).arg(error));
    if (error == QSerialPort::ResourceError) {
        // 硬件断开
        m_serial->close();
        stopRetryTimer();
        m_handshakeDone       = false;
        m_pendingBatteryQuery = false;
        m_btnConnect->setText("连接 COM10");
        m_btnQuery ->setEnabled(false);
        m_btnDelete->setEnabled(false);
        m_btnQueryBattery->setEnabled(false);
        m_batteryLabel->setText("电量：--");
        m_statusLabel->setText("串口断开（硬件错误）");
    }
}

// ============================================================
// 日志：带时间戳追加到 QTextEdit
// ============================================================
void SerialTestWidget::log(const QString &msg)
{
    const QString ts = QDateTime::currentDateTime().toString("HH:mm:ss.zzz");
    m_logEdit->append(QString("[%1] %2").arg(ts, msg));
    qDebug().noquote() << ts << msg;
}
