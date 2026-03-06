#ifndef TICKETFRAMEBUILDER_H
#define TICKETFRAMEBUILDER_H

#include <QByteArray>
#include <QList>
#include <QString>
#include <QtGlobal>

/**
 * @brief 传票帧构建器（Step4.1 —— 只生成 HEX，不写串口）
 *
 * 帧格式（字节顺序，左=先发送）：
 *   [7E][6C][KeyId][FrameNo][Station][Device][Len_Lo][Len_Hi]
 *   [Cmd][Seq_Lo][Seq_Hi][Payload...][CRC_Hi][CRC_Lo]
 *
 * Len  = Cmd(1) + Seq(2) + len(Payload)，小端
 * Seq  = 帧序号（从0递增），小端
 * CRC  = 对 [Cmd][Seq_Lo][Seq_Hi][Payload...] 计算，大端写入
 * Cmd  = 0x03（单帧或最后帧），0x83（还有后续帧）
 */
class TicketFrameBuilder
{
public:
    // ---- 协议常量（不可改） ----
    static constexpr quint8  FRAME_HEADER_0  = 0x7E;
    static constexpr quint8  FRAME_HEADER_1  = 0x6C;
    static constexpr quint8  CMD_TICKET       = 0x03;   // 单帧/最后帧
    static constexpr quint8  CMD_TICKET_MORE  = 0x83;   // 还有后续帧

    /**
     * @brief 构造器
     * @param stationId  站号（1字节），默认 0x01
     * @param deviceByte 设备字节，默认 0x00（对齐原产品）
     * @param keyId      KeyId，默认 0x00（对齐原产品）
     */
    explicit TicketFrameBuilder(quint8 stationId  = 0x01,
                                quint8 deviceByte = 0x00,
                                quint8 keyId      = 0x00);

    // ---- 参数修改 ----
    void setStationId(quint8 id)      { m_stationId = id; }
    void setDeviceByte(quint8 dev)    { m_deviceByte = dev; }
    void setKeyId(quint8 kid)         { m_keyId = kid; }
    /// 设置每帧最大 payload 块大小（默认 512，实现单帧模式；原 240 = 分帧模式）
    void setMaxChunkSize(int sz)      { m_maxChunkSize = sz; }

    /**
     * @brief 用模板 payload（0xFF 填充 + TICK magic）生成帧列表
     * @param payloadSize  模板 payload 总长度（字节），默认 256
     * @return 各帧的二进制数据列表
     */
    QList<QByteArray> buildTemplateFrames(int payloadSize = 256) const;

    /**
     * @brief 用自定义 payload 生成帧列表（Step5 用）
     * @param payload  完整业务 payload
     * @return 各帧的二进制数据列表
     */
    QList<QByteArray> buildFrames(const QByteArray &payload) const;

    /**
     * @brief 把帧列表格式化为可读的 HEX + 字段解释日志
     * @param frames    buildFrames/buildTemplateFrames 的返回值
     * @param logLines  输出：日志文本列表（每帧一段）
     * @param hexLines  输出：SEN_HEX 行列表（每帧一行，空格分隔）
     */
    void formatFrameLog(const QList<QByteArray> &frames,
                        QStringList &logLines,
                        QStringList &hexLines) const;

    /**
     * @brief 把单帧保存为 .hex 文本 + .bin 二进制两个文件
     * @param frame     单帧二进制
     * @param basePath  不含扩展名的完整路径
     */
    static void saveFrameFiles(const QByteArray &frame,
                               const QString &basePath);

    // ---- CRC（与主工程 KeyCrc16 完全相同的查表法，右移，初值0） ----
    static quint16 calcCrc(const QByteArray &data);

private:
    quint8 m_stationId;
    quint8 m_deviceByte;
    quint8 m_keyId;             // KeyId 字节，默认 0x00
    int    m_maxChunkSize;      // 每帧最大 payload 块（默认 512）
    mutable quint8 m_frameNo;   // 0..255 循环（跨 build 调用累加）

    /// 构造单帧（内部）
    QByteArray buildOneFrame(quint8 cmd, quint8 seqLo, quint8 seqHi,
                             const QByteArray &chunk) const;
};

#endif // TICKETFRAMEBUILDER_H
