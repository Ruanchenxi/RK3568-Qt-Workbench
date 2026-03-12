/**
 * @file LogController.h
 * @brief 服务日志页面控制器
 */
#ifndef LOGCONTROLLER_H
#define LOGCONTROLLER_H

#include <QObject>

#include "shared/contracts/IProcessService.h"

class LogController : public QObject
{
    Q_OBJECT
public:
    explicit LogController(IProcessService *processService = nullptr, QObject *parent = nullptr);
    ~LogController() override = default;

    void start();
    void stop();

signals:
    void logGenerated(const QString &text);

private:
    IProcessService *m_processService;
};

#endif // LOGCONTROLLER_H
