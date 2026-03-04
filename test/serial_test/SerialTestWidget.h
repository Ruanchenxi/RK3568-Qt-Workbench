#pragma once

#include <QWidget>
#include <QSerialPort>
#include <QByteArray>
#include <QList>
#include <QTimer>

class QPushButton;
class QTextEdit;
class QLabel;
class QLineEdit;
class QSpinBox;

// ============================================================
// SerialTestWidget
// 独立最小验证程序：与主项目业务代码完全无关。
// 实现 SET_COM / Q_TASK / DEL / Q_KEYEQ 四条命令，用于现场协议闭环验证。
// ============================================================
class SerialTestWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SerialTestWidget(QWidget *parent = nullptr);
    ~SerialTestWidget() override;

private slots:
    void onConnectClicked();
    void onQueryClicked();
    void onDeleteClicked();
    void onQueryBatteryClicked();     ///< 新增：查询电量
    void onReadyRead();
    void onRetryTimeout();
    void onSerialErrorOccurred(QSerialPort::SerialPortError error);

private:
    // --- 帧构建 / 拆包 ---
    QByteArray buildFrame(quint8 cmd, const QByteArray &data, const QByteArray &addr2);
    int        tryExtractOneFrame(QByteArray &buf, QByteArray &outFrame);
    void       handleFrame(const QByteArray &frame);
    void       handleKeyEqResp(const QByteArray &frame); ///< 新增：解析电量响应

    // --- 发送与超时 ---
    void sendFrame(const QByteArray &frame, quint8 expectedAckCmd);
    void startRetryTimer();
    void stopRetryTimer();

    // --- CRC ---
    static quint16 calcCrc(const QByteArray &cmdPlusData);
    static bool    selfTestCrc();   ///< 启动时自测，打印 PASS/FAIL

    // --- 日志 ---
    void log(const QString &msg);

    // --- UI 控件 ---
    QPushButton *m_btnConnect;
    QPushButton *m_btnQuery;
    QPushButton *m_btnDelete;
    QPushButton *m_btnQueryBattery;   ///< 新增
    QTextEdit   *m_logEdit;
    QLabel      *m_statusLabel;
    QLabel      *m_batteryLabel;      ///< 新增：显示电量百分比
    QSpinBox    *m_spinStationId;       ///< 当前站号（仅 DEL 用，1-99）

    // --- 串口 ---
    QSerialPort *m_serial;
    QByteArray   m_recvBuffer;
    quint8       m_frameNo;

    // --- 任务列表 ---
    QList<QByteArray> m_taskIds;   ///< 每条 16 字节（来自 Q_TASK 回包）

    // --- 重试机制 ---
    QTimer  *m_retryTimer;
    QByteArray m_lastSentFrame;
    quint8   m_expectedAckCmd;
    int      m_retryCount;
    bool     m_pendingAfterDelQuery;  ///< DEL ACK 后需要再发一次 Q_TASK

    // --- 层务状态 ---
    bool     m_handshakeDone;         ///< 新增： SET_COM 已收到 ACK
    bool     m_pendingBatteryQuery;   ///< 新增： 等握手完成后再发电量查询
};
