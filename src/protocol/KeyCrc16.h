/**
 * @file KeyCrc16.h
 * @brief CRC-16 校验计算（查表法）
 *
 * 算法：CRC-16/IBM   多项式 0x8005，初值 0x0000，低位先入
 * 用于 X5 钥匙柜串口协议帧的 Cmd+Data 区域校验。
 */
#ifndef KEYCRC16_H
#define KEYCRC16_H

#include <QByteArray>
#include <QtGlobal>

namespace KeyCrc16 {

namespace Detail {
// CRC-16/IBM 查找表（多项式 0x8005，反射输入/输出）
inline const quint16* table() {
    static quint16 t[256] = {0};
    static bool init = false;
    if (!init) {
        for (int i = 0; i < 256; ++i) {
            quint16 crc = static_cast<quint16>(i);
            for (int j = 0; j < 8; ++j) {
                if (crc & 1) crc = static_cast<quint16>((crc >> 1) ^ 0xA001u);
                else         crc >>= 1;
            }
            t[i] = crc;
        }
        init = true;
    }
    return t;
}
} // namespace Detail

/**
 * @brief 计算 CRC-16（对 QByteArray 数据）
 * @param data 输入数据（Cmd + Data 区域）
 * @return 16 位 CRC 值（高字节在前发送时需要调用方处理字节序）
 */
inline quint16 calc(const QByteArray &data) {
    const quint16 *t = Detail::table();
    quint16 crc = 0x0000;
    for (unsigned char c : data) {
        crc = static_cast<quint16>((crc >> 8) ^ t[(crc ^ c) & 0xFF]);
    }
    return crc;
}

} // namespace KeyCrc16

#endif // KEYCRC16_H
