/**
 * @file CardSerialSource.h
 * @brief 读卡串口采集源占位实现（后续接 /dev/ttyS3）
 */
#ifndef CARD_SERIAL_SOURCE_H
#define CARD_SERIAL_SOURCE_H

#include "features/auth/domain/ports/ICredentialSource.h"

class CardSerialSource : public ICredentialSource
{
    Q_OBJECT
public:
    explicit CardSerialSource(QObject *parent = nullptr);
    ~CardSerialSource() override = default;

    AuthMode mode() const override;
    void start() override;
    void stop() override;
};

#endif // CARD_SERIAL_SOURCE_H
