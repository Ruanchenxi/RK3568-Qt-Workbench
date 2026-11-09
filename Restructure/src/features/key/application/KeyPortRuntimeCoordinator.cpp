#include "features/key/application/KeyPortRuntimeCoordinator.h"

#include <utility>

#include <QStringList>

// Binds startup serial selection to the injected session and configuration source.
KeyPortRuntimeCoordinator::KeyPortRuntimeCoordinator(
        IKeySessionService *session,
        ConfigProvider configProvider,
        QObject *parent)
    : QObject(parent)
    , m_session(session)
    , m_configProvider(std::move(configProvider))
{
    Q_ASSERT(m_session);
    Q_ASSERT(m_configProvider);
}

// Normalizes configuration once so startup uses one baud fallback.
KeyPortRuntimeConfig KeyPortRuntimeCoordinator::currentConfig() const
{
    KeyPortRuntimeConfig config = m_configProvider();
    config.portName = config.portName.trimmed();
    if (config.baudRate <= 0) {
        config.baudRate = 115200;
    }
    return config;
}

// Exposes the accepted adapter generation for trace correlation.
int KeyPortRuntimeCoordinator::acceptedBridgeEpoch() const
{
    return m_acceptedBridgeEpoch;
}

// Rejects callbacks from an adapter that no longer owns the key session.
bool KeyPortRuntimeCoordinator::acceptsBridgeEvent(int eventBridgeEpoch) const
{
    return m_acceptedBridgeEpoch <= 0
            || eventBridgeEpoch <= 0
            || eventBridgeEpoch == m_acceptedBridgeEpoch;
}

// Advances the accepted adapter generation when a new key session opens.
void KeyPortRuntimeCoordinator::acceptBridgeEpoch(int bridgeEpoch)
{
    m_acceptedBridgeEpoch = bridgeEpoch;
}

// Starts on the configured port, or the verified fallback when configuration is empty.
void KeyPortRuntimeCoordinator::autoConnect()
{
    const KeyPortRuntimeConfig config = currentConfig();
    if (!config.enabled) {
        emit statusMessage(QStringLiteral("钥匙串口已停用"));
        emit sessionSnapshotChanged(m_session->snapshot());
        return;
    }
    const KeySessionSnapshot snapshot = m_session->snapshot();
    QStringList candidates;

    if (!config.portName.isEmpty()) {
        candidates << config.portName;
    } else if (!snapshot.verifiedPortName.trimmed().isEmpty()) {
        candidates << snapshot.verifiedPortName.trimmed();
    }

    if (candidates.isEmpty()) {
        emit statusMessage(QStringLiteral("未检测到可用串口"));
        emit sessionSnapshotChanged(m_session->snapshot());
        return;
    }

    for (const QString &port : candidates) {
        if (!m_session->connectPort(port, config.baudRate)) {
            continue;
        }
        emit statusMessage(config.portName.isEmpty()
                ? QStringLiteral("串口已连接(已验证端口): %1").arg(port)
                : QStringLiteral("串口已连接(配置端口): %1").arg(port));
        emit sessionSnapshotChanged(m_session->snapshot());
        return;
    }

    emit statusMessage(QStringLiteral("串口连接失败，请检查底座或配置"));
    emit sessionSnapshotChanged(m_session->snapshot());
}
