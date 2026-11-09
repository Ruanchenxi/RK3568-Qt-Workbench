/**
 * @file LogController.cpp
 * @brief 服务日志控制器实现
 */

#include "features/log/application/LogController.h"

#include <QtGlobal>

LogController::LogController(IProcessService *processService, QObject *parent)
    : QObject(parent),
      m_processService(processService)
{
    // ProcessService is an application-wide L4 capability; feature code must
    // consume the composition-owned instance instead of constructing a peer.
    Q_ASSERT(m_processService);
    if (!m_processService)
    {
        return;
    }

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
