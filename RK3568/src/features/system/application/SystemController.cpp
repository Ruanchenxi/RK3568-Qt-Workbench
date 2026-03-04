/**
 * @file SystemController.cpp
 * @brief 系统设置页面控制器实现
 */
#include "features/system/application/SystemController.h"

#include <QSerialPortInfo>

#include "core/ConfigManager.h"

SystemController::SystemController(QObject *parent)
    : QObject(parent)
{
}

SystemSettingsDto SystemController::loadSettings() const
{
    ConfigManager *config = ConfigManager::instance();
    SystemSettingsDto dto;
    dto.homeUrl = config->homeUrl();
    dto.apiUrl = config->apiUrl();
    dto.stationId = config->stationId();
    dto.tenantCode = config->tenantCode();
    dto.keySerialPort = config->keySerialPort();
    dto.cardSerialPort = config->cardSerialPort();
    dto.baudRate = config->baudRate();
    dto.dataBits = config->dataBits();
    return dto;
}

void SystemController::saveSettings(const SystemSettingsDto &settings) const
{
    ConfigManager *config = ConfigManager::instance();
    config->setHomeUrl(settings.homeUrl);
    config->setApiUrl(settings.apiUrl);
    config->setStationId(settings.stationId);
    config->setTenantCode(settings.tenantCode);
    config->setKeySerialPort(settings.keySerialPort);
    config->setCardSerialPort(settings.cardSerialPort);
    config->setBaudRate(settings.baudRate);
    config->setDataBits(settings.dataBits);
    config->sync();
}

QList<QStringList> SystemController::loadUserList() const
{
    // NOTE:
    // 当前数据源为本地演示数据，后续接入后端时由 Controller 统一替换。
    QList<QStringList> users = {
        {"admin", "超级管理员", "", ""},
        {"s1", "审核人员", "", ""},
        {"y1", "一队负责人", "", ""},
        {"1d1", "一队队员1", "", ""}
    };
    return users;
}

QStringList SystemController::availableSerialPorts() const
{
    QStringList list;
    for (const QSerialPortInfo &info : QSerialPortInfo::availablePorts()) {
        list << info.portName();
    }

#ifdef Q_OS_LINUX
    // 补充硬件固定局路径：即使暂未注册，也让用户能在下拉中看到
    // /dev/ttyS4=钥匙柜, /dev/ttyS3=读卡器（见 CODEMAP 串口红线）
    static const QStringList kLinuxHints = {
        "/dev/ttyS3", "/dev/ttyS4",
        "/dev/ttyUSB0", "/dev/ttyUSB1",
        "/dev/ttyACM0"
    };
    for (const QString &path : kLinuxHints) {
        if (!list.contains(path)) {
            list << path;
        }
    }
#endif

    return list;
}
