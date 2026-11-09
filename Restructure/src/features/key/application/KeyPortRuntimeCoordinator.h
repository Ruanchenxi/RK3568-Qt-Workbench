#ifndef KEYPORTRUNTIMECOORDINATOR_H
#define KEYPORTRUNTIMECOORDINATOR_H

#include <functional>

#include <QObject>

#include "shared/contracts/IKeySessionService.h"

struct KeyPortRuntimeConfig
{
    QString portName;
    int baudRate = 115200;
    // False is an explicit operator choice and must not trigger a fallback port.
    bool enabled = true;
};

// Owns startup connection and rejects events emitted by an obsolete adapter.
// Runtime port switching is intentionally outside the current product scope.
class KeyPortRuntimeCoordinator : public QObject
{
    Q_OBJECT
public:
    using ConfigProvider = std::function<KeyPortRuntimeConfig()>;

    explicit KeyPortRuntimeCoordinator(IKeySessionService *session,
                                       ConfigProvider configProvider,
                                       QObject *parent = nullptr);

    // Connects the configured port, or the last verified port when no port is configured.
    void autoConnect();

    int acceptedBridgeEpoch() const;
    bool acceptsBridgeEvent(int eventBridgeEpoch) const;
    void acceptBridgeEpoch(int bridgeEpoch);

signals:
    void statusMessage(const QString &message);
    void sessionSnapshotChanged(const KeySessionSnapshot &snapshot);

private:
    KeyPortRuntimeConfig currentConfig() const;

    IKeySessionService *m_session;
    ConfigProvider m_configProvider;
    int m_acceptedBridgeEpoch = 0;
};

#endif // KEYPORTRUNTIMECOORDINATOR_H
