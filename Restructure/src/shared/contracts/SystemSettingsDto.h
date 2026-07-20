/**
 * @file SystemSettingsDto.h
 * @brief 系统设置页面 DTO
 */
#ifndef SYSTEMSETTINGSDTO_H
#define SYSTEMSETTINGSDTO_H

#include <QString>

struct SystemSettingsDto
{
    QString homeUrl;
    QString apiUrl;
    QString stationId;
    QString tenantCode;
    QString keySerialPort;
    QString cardSerialPort;
    int baudRate = 115200;
    int dataBits = 8;
};

#endif // SYSTEMSETTINGSDTO_H
