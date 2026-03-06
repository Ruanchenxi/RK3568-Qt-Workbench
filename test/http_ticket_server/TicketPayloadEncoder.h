#ifndef TICKETPAYLOADENCODER_H
#define TICKETPAYLOADENCODER_H

#include <QByteArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QtGlobal>

/**
 * @brief 操作票文件 payload 编码器（Step5，增量对齐原产品报文）
 *
 * ┌──────────────────────────────────────────────────────┐
 * │  固定头区  128 字节                                  │
 * │  [0..1]   版本号      2B BCD          = 01 11       │
 * │  [2..3]   保留        2B              = 00 00       │
 * │  [4..19]  操作票ID    16B  uint64-LE + FF×8         │
 * │  [20]     站号        1B              = stationId  │
 * │  [21]     票属性      1B              = 0x00       │
 * │  [22..23] 步数        2B LE           = stepNum    │
 * │  [24..27] taskName偏移 4B LE  (回填)               │
 * │  [28..35] 创建时间    8B BCD                       │
 * │  [36..43] 计划时间    8B BCD                       │
 * │  [44..91] 授权信息    48B             = FF         │
 * │  [92..127] 保留       36B             = FF         │
 * ├──────────────────────────────────────────────────────┤
 * │  步骤区   每步 8 字节（支持 step[0..N-1]）          │
 * │  [+0..1]  内码        2B LE                        │
 * │  [+2]     动作类型    1B                           │
 * │  [+3]     操作状态    1B              = 0x00       │
 * │  [+4..7]  显示项偏移  4B LE  (回填)               │
 * ├──────────────────────────────────────────────────────┤
 * │  字符串区  (步骤偏移之后)                            │
 * │  stepDetail  GBK(或UTF-8) + 0x00                   │
 * │  taskName    GBK(或UTF-8) + 0x00                   │
 * └──────────────────────────────────────────────────────┘
 *
 * 阶段：
 *  Stage 5.1 - 固定头 + 时间 BCD + 步数（字符串区全 FF）
 *  Stage 5.2 - 加入 stepDetail / taskName 字符串区 + 回填偏移
 *  Stage 5.4 - 支持多步票(stepNum>1) 的 step table / string area
 */
class TicketPayloadEncoder
{
public:
    // ---- 字符串编码选择 ----
    enum class StringEncoding {
        GBK,   ///< 优先；与原产品一致
        UTF8   ///< 调试备用
    };

    struct EncodeResult {
        QByteArray  payload;       ///< 最终 payload 字节流
        QStringList fieldLog;      ///< 每个字段的 offset/len/hex 描述（调试用）
        bool        ok = false;
        QString     errorMsg;
    };

    /**
     * @brief 将业务 JSON 对象编码为操作票 payload
     * @param ticketJson  业务 payload（已去掉外层 data 包装）
     * @param stationId   站号（写入 offset 20）
     * @param enc         字符串编码方式
     * @return EncodeResult
     */
    static EncodeResult encode(const QJsonObject &ticketJson,
                               quint8 stationId = 0x01,
                               StringEncoding enc = StringEncoding::GBK);

    // ---- 公共工具函数（可供外部使用） ----

    /**
     * @brief 时间字符串 -> BCD 字节序列
     * @param timeStr    "YYYYMMDDHHMM" 或 "YYYYMMDDHHMMSS"（其他长度也可）
     * @param targetLen  目标字节数，不足末位用 FF 填充
     * @return BCD bytes，例如 "202603042321" -> 20 26 03 04 23 21 00 FF
     */
    static QByteArray timeToBcd(const QString &timeStr, int targetLen = 8);

    /**
     * @brief uint64 -> 小端字节序列
     * @param val       64位无符号整数
     * @param totalLen  写入字节数（不足则高位补 FF 直到 totalLen）
     */
    static QByteArray uint64LeBytes(quint64 val, int totalLen = 16);

    // ---- 内部布局常量（Stage 5.3，160B 头部） ----
    static constexpr int HEADER_SIZE     = 160;  ///< 固定头区（含 fileSize 前缀）
    static constexpr int STEP_ENTRY_SIZE = 8;    ///< 每步数据大小

    // 头区各字段 offset（Stage 5.3，所有字段相对 payload[0]）
    static constexpr int OFF_FILE_SIZE    = 0;    ///< 文件大小 4B LE（回填）
    static constexpr int OFF_VERSION      = 4;
    static constexpr int OFF_RESERVED     = 6;
    static constexpr int OFF_TASK_ID      = 8;
    static constexpr int OFF_STATION_ID   = 24;
    static constexpr int OFF_TICKET_ATTR  = 25;
    static constexpr int OFF_STEP_NUM     = 26;
    static constexpr int OFF_TASKNAME_PTR = 28;   ///< 任务名偏移（回填）
    static constexpr int OFF_CREATE_TIME  = 32;
    static constexpr int OFF_PLAN_TIME    = 40;
    static constexpr int OFF_AUTH         = 48;   ///< 授权信息 48B = FF
    static constexpr int OFF_ADD_FIELDS   = 96;   ///< 额外字段 32B = FF [TODO]
    static constexpr int OFF_RESERVED2    = 128;  ///< 保留 32B = FF
    //   OFF_RESERVED2 + 32 = 160 = HEADER_SIZE

    // 目标对齐值（用于日志 diff 输出）
    static constexpr quint32 TARGET_TASKNAME_OFF = 0xD6;  // 214
    static constexpr quint32 TARGET_DISPLAY_OFF  = 0xA8;  // 168

    // 步骤区内各字段相对偏移
    static constexpr int STEP_OFF_INNER_CODE = 0;
    static constexpr int STEP_OFF_LOCKER_OP  = 2;
    static constexpr int STEP_OFF_STATE      = 3;
    static constexpr int STEP_OFF_DISP_PTR   = 4;  ///< displayOffset（回填）

private:
    // 辅助：以小端方式把 v 写入 buf 的 offset 位置
    static void writeLE32(QByteArray &buf, int offset, quint32 v);
    static void writeLE16(QByteArray &buf, int offset, quint16 v);

    // 辅助：把字节序列格式化为空格分隔的大写 HEX
    static QString hexDump(const QByteArray &data, int maxBytes = 32);
};

#endif // TICKETPAYLOADENCODER_H
