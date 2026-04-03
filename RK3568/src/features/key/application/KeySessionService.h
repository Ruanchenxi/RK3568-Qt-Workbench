/**
 * @file KeySessionService.h
 * @brief 钥匙会话服务默认实现（适配 KeySerialClient）
 */
#ifndef KEYSESSIONSERVICE_H
#define KEYSESSIONSERVICE_H

#include <QObject>
#include <QVariantList>

#include "shared/contracts/IKeySessionService.h"
#include "features/key/protocol/KeySerialClient.h"

class KeySessionService : public IKeySessionService
{
    Q_OBJECT
public:
    explicit KeySessionService(QObject *parent = nullptr);
    ~KeySessionService() override = default;

    bool connectPort(const QString &portName, int baud) override;
    void disconnectPort() override;
    void setPortSwitchInProgress(bool inProgress) override;
    void clearVerifiedPort() override;
    KeySessionSnapshot snapshot() const override;
    void execute(const CommandRequest &request) override;
    void transferTicket(const TicketTransferRequest &request) override;

private:
    QVariantList toTaskVariantList(const QList<KeyTaskInfo> &tasks) const;
    void stampEvent(KeySessionEvent &event) const;
    void emitStateChanged();

    KeySerialClient *m_client;
    int m_bridgeEpoch = 0;
};

#endif // KEYSESSIONSERVICE_H
