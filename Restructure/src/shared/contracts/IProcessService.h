/**
 * @file IProcessService.h
 * @brief 服务进程治理抽象接口
 *
 * 设计说明：
 * 1. Page 只依赖该接口，不直接依赖 QProcess 和平台命令。
 * 2. 启动策略与端口治理封装在服务层，方便后续替换实现。
 * 3. 当前接口保持最小可用，避免过度设计。
 */
#ifndef IPROCESSSERVICE_H
#define IPROCESSSERVICE_H

#include <QObject>

class IProcessService : public QObject
{
    Q_OBJECT
public:
    explicit IProcessService(QObject *parent = nullptr) : QObject(parent) {}
    ~IProcessService() override = default;

    virtual void startAuto() = 0;
    virtual void stopAll() = 0;

signals:
    void logGenerated(const QString &message);
};

#endif // IPROCESSSERVICE_H
