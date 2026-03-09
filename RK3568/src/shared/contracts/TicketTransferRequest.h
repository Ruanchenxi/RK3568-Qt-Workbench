/**
 * @file TicketTransferRequest.h
 * @brief 传票发送请求契约
 *
 * 说明：
 * 1. 该结构只承载“上层如何请求发送传票”，不包含协议细节。
 * 2. 当前最小闭环仅支持从 JSON 文件路径触发。
 * 3. 后续工作台自动触发可复用同一结构，直接填充 jsonBytes。
 */
#ifndef TICKETTRANSFERREQUEST_H
#define TICKETTRANSFERREQUEST_H

#include <QByteArray>
#include <QString>

struct TicketTransferRequest
{
    QString    jsonPath;              ///< JSON 文件路径（当前手动入口主要使用）
    QByteArray jsonBytes;             ///< 已加载的 JSON 原文（可选，优先于 jsonPath）
    quint8     stationId = 0x01;      ///< 传票外层帧站号
    int        opId = -1;             ///< 日志归属操作ID
};

#endif // TICKETTRANSFERREQUEST_H
