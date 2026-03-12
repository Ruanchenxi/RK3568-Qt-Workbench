/**
 * @file SystemController.h
 * @brief 系统设置页面控制器
 */
#ifndef SYSTEMCONTROLLER_H
#define SYSTEMCONTROLLER_H

#include <QObject>
#include <QList>
#include <QStringList>

#include "shared/contracts/SystemSettingsDto.h"

class SystemController : public QObject
{
    Q_OBJECT
public:
    explicit SystemController(QObject *parent = nullptr);
    ~SystemController() override = default;

    SystemSettingsDto loadSettings() const;
    void saveSettings(const SystemSettingsDto &settings) const;
    QList<QStringList> loadUserList() const;
    /** @brief 返回可用串口名称列表，Windows 返回 COMx，Linux 返回 /dev/ttySx 并补充常用路径 */
    QStringList availableSerialPorts() const;
};

#endif // SYSTEMCONTROLLER_H
