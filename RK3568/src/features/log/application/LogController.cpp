/**
 * @file LogController.cpp
 * @brief 服务日志控制器实现
 */

#include "features/log/application/LogController.h"

#include "platform/process/ProcessService.h"

LogController::LogController(IProcessService *processService, QObject *parent)
    : QObject(parent),
      m_processService(processService ? processService : new ProcessService(this))
{
    connect(m_processService, &IProcessService::logGenerated,
            this, &LogController::logGenerated);
}

void LogController::start()
{
    m_processService->startAuto();
}

void LogController::stop()
{
    m_processService->stopAll();
}
