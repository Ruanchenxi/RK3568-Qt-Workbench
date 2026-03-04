/**
 * @file CardSerialSource.cpp
 * @brief 读卡串口采集源占位实现
 */
#include "features/auth/infra/device/CardSerialSource.h"

CardSerialSource::CardSerialSource(QObject *parent)
    : ICredentialSource(parent)
{
}

AuthMode CardSerialSource::mode() const
{
    return AuthMode::Card;
}

void CardSerialSource::start()
{
    emit sourceError(QStringLiteral("CardSerialSource 尚未接入设备驱动"));
}

void CardSerialSource::stop()
{
}
