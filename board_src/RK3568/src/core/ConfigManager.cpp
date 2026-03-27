/**
 * @file ConfigManager.cpp
 * @brief 配置管理器实现
 */

#include "ConfigManager.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QSet>

ConfigManager *ConfigManager::s_instance = nullptr;

ConfigManager::ConfigManager(QObject *parent)
    : QObject(parent)
{
    m_settings = new QSettings("RK3568", "DigitalPowerSystem", this);
    loadDefaults(); // ← 跳转函数，加载默认配置
}

ConfigManager::~ConfigManager()
{
}

ConfigManager *ConfigManager::instance()
{
    if (!s_instance)
    {
        s_instance = new ConfigManager();
    }
    return s_instance;
}

void ConfigManager::loadDefaults()
{
#ifdef Q_OS_LINUX
    auto linuxBoardRecoveryScriptPath = []() -> QString {
        const QFileInfo info(QStringLiteral("/home/linaro/outage/start.sh"));
        if (!info.exists())
        {
            return QString();
        }
        return QDir::cleanPath(info.absoluteFilePath());
    };

    auto linuxBoardRecoveryWorkDir = [&linuxBoardRecoveryScriptPath]() -> QString {
        const QString scriptPath = linuxBoardRecoveryScriptPath();
        if (!scriptPath.isEmpty())
        {
            return QDir::cleanPath(QFileInfo(scriptPath).absolutePath());
        }

        const QString dirPath = QStringLiteral("/home/linaro/outage");
        if (QDir(dirPath).exists())
        {
            return QDir::cleanPath(dirPath);
        }
        return QString();
    };

    auto linuxHasLocalStartScript = [this, &linuxBoardRecoveryScriptPath]() -> bool {
        const QString configuredScript = m_settings->value("service/scriptPath", "start-all.sh")
                                             .toString()
                                             .trimmed();
        QStringList candidates;
        if (!configuredScript.isEmpty())
        {
            QFileInfo configuredInfo(configuredScript);
            if (configuredInfo.isAbsolute())
            {
                candidates << configuredInfo.absoluteFilePath();
            }

            const QString workDir = m_settings->value("service/workDir", QCoreApplication::applicationDirPath())
                                        .toString()
                                        .trimmed();
            if (!workDir.isEmpty())
            {
                candidates << QDir(workDir).filePath(configuredScript);
            }

            candidates << QDir(QCoreApplication::applicationDirPath()).filePath(configuredScript);
        }

        const QString boardScript = linuxBoardRecoveryScriptPath();
        if (!boardScript.isEmpty())
        {
            candidates << boardScript;
        }

        candidates << QDir(QCoreApplication::applicationDirPath()).filePath("start.sh")
                   << QDir(QCoreApplication::applicationDirPath()).filePath("start-all.sh");

        QSet<QString> normalized;
        for (const QString &candidate : candidates)
        {
            const QString cleanPath = QDir::cleanPath(candidate);
            if (cleanPath.isEmpty() || normalized.contains(cleanPath))
            {
                continue;
            }
            normalized.insert(cleanPath);
            if (QFileInfo::exists(cleanPath))
            {
                return true;
            }
        }
        return false;
    };
#endif

    // 如果配置不存在,设置默认值
    if (!m_settings->contains("system/homeUrl"))
    {
        setHomeUrl("http://localhost/pad");
    }
    if (!m_settings->contains("system/apiUrl"))
    {
        setApiUrl("http://localhost/api/kids-outage/third-api");
    }
    if (!m_settings->contains("system/stationId"))
    {
        setStationId("001");
    }
    if (!m_settings->contains("system/tenantCode"))
    {
        setTenantCode("000000");
    }
    if (!m_settings->contains("serial/keyPort"))
    {
#ifdef Q_OS_LINUX
        // RK3568/RK3588 Ubuntu: CODEMAP 指定钥匙柜串口固定在 /dev/ttyS4
        setKeySerialPort("/dev/ttyS4");
#else
        // Windows 开发：空字符串，首次运行在系统设置页手动选择/输入端口
        setKeySerialPort("");
#endif
    }
    if (!m_settings->contains("serial/cardPort"))
    {
#ifdef Q_OS_LINUX
        // RK3568/RK3588 Ubuntu: CODEMAP 指定读卡器串口固定在 /dev/ttyS3
        setCardSerialPort("/dev/ttyS3");
#else
        setCardSerialPort("");
#endif
    }
    if (!m_settings->contains("serial/cardPortMigratedV1LinuxBoardDefault"))
    {
#ifdef Q_OS_LINUX
        const QString currentCardPort = m_settings->value("serial/cardPort").toString().trimmed();
        const QFileInfo boardCardPortInfo(QStringLiteral("/dev/ttyS3"));
        if ((currentCardPort.isEmpty() || currentCardPort == QLatin1String("/dev/ttyACM0"))
            && boardCardPortInfo.exists())
        {
            setCardSerialPort(QStringLiteral("/dev/ttyS3"));
        }
#endif
        setValue("serial/cardPortMigratedV1LinuxBoardDefault", true);
    }
    if (!m_settings->contains("serial/baudRate"))
    {
        setBaudRate(115200);  // X5 钥匙底座协议固定波特率，不得改为 9600
    }
    if (!m_settings->contains("serial/dataBits"))
    {
        setDataBits(8);
    }
    if (!m_settings->contains("advanced/debugMode"))
    {
        setDebugMode(0);
    }
    if (!m_settings->contains("advanced/logLevel"))
    {
        setLogLevel(2); // Info
    }
    if (!m_settings->contains("advanced/connectionTimeout"))
    {
        setConnectionTimeout(30);
    }
    if (!m_settings->contains("advanced/retryCount"))
    {
        setRetryCount(3);
    }
    if (!m_settings->contains("advanced/autoStart"))
    {
        // 默认开启自动启动，满足部署环境开机后快速进入可用态
        setAutoStart(true);
    }
    if (!m_settings->contains("advanced/rememberLogin"))
    {
        setRememberLogin(false);
    }
    if (!m_settings->contains("advanced/soundEnabled"))
    {
        setSoundEnabled(true);
    }

    // ========== 服务启动配置 ==========
    if (!m_settings->contains("service/workDir"))
    {
#ifdef Q_OS_LINUX
        const QString preferredWorkDir = linuxBoardRecoveryWorkDir();
        if (!preferredWorkDir.isEmpty())
        {
            setValue("service/workDir", preferredWorkDir);
        }
        else
        {
            setValue("service/workDir", QCoreApplication::applicationDirPath());
        }
#else
        setValue("service/workDir", QCoreApplication::applicationDirPath());
#endif
    }
    if (!m_settings->contains("service/scriptPath"))
    {
#ifdef Q_OS_WIN
        setValue("service/scriptPath", "start-all.bat");
#else
        const QString preferredScriptPath = linuxBoardRecoveryScriptPath();
        if (!preferredScriptPath.isEmpty())
        {
            setValue("service/scriptPath", preferredScriptPath);
        }
        else
        {
            setValue("service/scriptPath", "start-all.sh");
        }
#endif
    }
    if (!m_settings->contains("service/ports"))
    {
#ifdef Q_OS_LINUX
        if (!linuxBoardRecoveryScriptPath().isEmpty())
        {
            setValue("service/ports", "8081");
        }
        else
        {
            setValue("service/ports", "8081,9000,9001");
        }
#else
        setValue("service/ports", "8081,9000,9001");
#endif
    }
    if (!m_settings->contains("service/logFiles"))
    {
        // 外部日志跟踪文件列表（支持绝对路径或相对 service/workDir 的相对路径）
        // 格式示例：
        // logs/application.log;logs/kids-api.log
        setValue("service/logFiles", "");
    }
    if (!m_settings->contains("service/externalLogTailEnabled"))
    {
        setValue("service/externalLogTailEnabled", true);
    }
    if (!m_settings->contains("service/externalLogTailIntervalMs"))
    {
        setValue("service/externalLogTailIntervalMs", 1000);
    }
    if (!m_settings->contains("service/autoCleanPorts"))
    {
        setValue("service/autoCleanPorts", false);
    }
    if (!m_settings->contains("service/forceRestartScript"))
    {
        // 默认回到更接近原产品的口径：
        // 服务健康时不重启脚本，服务异常时再按当前模式决定是否恢复。
        setValue("service/forceRestartScript", false);
    }
    if (!m_settings->contains("service/remoteFallbackLocal"))
    {
#ifdef Q_OS_WIN
        // Windows 联调环境默认开启回退：
        // remote 模式检测到服务不可达时，自动回退执行本地启动脚本。
        setValue("service/remoteFallbackLocal", true);
#else
        // Linux 板端优先采用“检查优先，失败再恢复”：
        // 若本地存在恢复脚本，则允许 remote 失败后回退本地拉起。
        setValue("service/remoteFallbackLocal", linuxHasLocalStartScript());
#endif
    }
    if (!m_settings->contains("service/remoteRequiredPorts"))
    {
        // remote 严格检查的关键端口：
        // 仅包含主业务链路必需端口，避免把非关键运维端口作为硬条件导致误判。
#ifdef Q_OS_LINUX
        if (!linuxBoardRecoveryScriptPath().isEmpty())
        {
            setValue("service/remoteRequiredPorts", "8081");
        }
        else
        {
            setValue("service/remoteRequiredPorts", "8081,9000");
        }
#else
        setValue("service/remoteRequiredPorts", "8081,9000");
#endif
    }
    if (!m_settings->contains("service/remoteRequirePorts"))
    {
#ifdef Q_OS_WIN
        // Windows 联调默认开启严格端口检查：
        // 即使 80 端口可达，也要求关键业务端口（如 8081/9000）可达，
        // 否则触发 remote->local 回退，自动拉起后端。
        setValue("service/remoteRequirePorts", true);
#else
        // Linux 产品板默认同机部署，优先要求关键端口可达；
        // 若没有本地恢复脚本，则仍按纯 remote 口径保持宽松检查。
        setValue("service/remoteRequirePorts", linuxHasLocalStartScript());
#endif
    }
    if (!m_settings->contains("service/startMode"))
    {
        // 启动策略默认值按平台区分：
        // Windows 联调默认 local（由前端拉起 start-all.bat）。
        // Linux 板端默认 remote，先做健康检查，必要时再通过 remoteFallbackLocal 恢复。
#ifdef Q_OS_WIN
        setValue("service/startMode", "local");
#else
        setValue("service/startMode", "remote");
#endif
    }

    // 兼容迁移：旧版本默认值是 false，会导致日志页不自动启动服务。
    // 该迁移只执行一次，避免每次启动都覆盖用户显式配置。
    if (!m_settings->contains("service/autoStartMigratedV1"))
    {
        setAutoStart(true);
        setValue("service/autoStartMigratedV1", true);
    }

    // 兼容迁移：旧版本没有 startMode 时，补齐平台默认值。
    if (!m_settings->contains("service/startModeMigratedV1"))
    {
#ifdef Q_OS_WIN
        setValue("service/startMode", "local");
#else
        setValue("service/startMode", "remote");
#endif
        setValue("service/startModeMigratedV1", true);
    }

    // 兼容迁移V2：将历史默认 local 迁移为 remote（仅执行一次）。
    // 目的：避免前端重复拉起后端导致端口冲突（例如 8081 已被占用）。
    if (!m_settings->contains("service/startModeMigratedV2RemoteDefault"))
    {
        const QString currentMode = m_settings->value("service/startMode").toString().trimmed().toLower();
#ifdef Q_OS_WIN
        if (currentMode.isEmpty() || currentMode == "local")
        {
            setValue("service/startMode", "remote");
        }
#else
        // Linux 板端当前默认口径就是 remote，空值时补 remote 即可。
        if (currentMode.isEmpty())
        {
            setValue("service/startMode", "remote");
        }
#endif
        setValue("service/startModeMigratedV2RemoteDefault", true);
    }

#ifdef Q_OS_WIN
    // 兼容迁移V3（Windows联调优先）：
    // 历史版本可能通过 V2 强制把 startMode 迁移成 remote，
    // 导致只做可达性检查、不执行本地脚本拉起。
    // 本迁移仅在 Windows 执行一次，把 remote 拉回 local。
    if (!m_settings->contains("service/startModeMigratedV3WindowsLocalDefault"))
    {
        const QString currentMode = m_settings->value("service/startMode").toString().trimmed().toLower();
        if (currentMode.isEmpty() || currentMode == "remote")
        {
            setValue("service/startMode", "local");
        }
        setValue("service/startModeMigratedV3WindowsLocalDefault", true);
    }
#endif

#ifdef Q_OS_LINUX
    // Linux 历史 V4 key 保留，但不再把板端默认强制迁回 local。
    if (!m_settings->contains("service/startModeMigratedV4LinuxLocalDefault"))
    {
        const QString currentMode = m_settings->value("service/startMode").toString().trimmed().toLower();
        if (currentMode.isEmpty())
        {
            setValue("service/startMode", "remote");
        }
        setValue("service/startModeMigratedV4LinuxLocalDefault", true);
    }

    // Linux 板端定稿迁移：
    // 默认采用“remote 健康检查优先，失败再回退本地脚本恢复”。
    // 若检测到原产品脚本 /home/linaro/outage/start.sh，则收口为显式绝对路径。
    if (!m_settings->contains("service/startModeMigratedV5LinuxCheckFirstDefault"))
    {
        const QString currentMode = m_settings->value("service/startMode").toString().trimmed().toLower();
        const QString currentScript = m_settings->value("service/scriptPath").toString().trimmed();
        const QString currentWorkDir = QDir::cleanPath(m_settings->value("service/workDir").toString().trimmed());
        const QString appDir = QDir::cleanPath(QCoreApplication::applicationDirPath());
        const QString boardScriptPath = linuxBoardRecoveryScriptPath();
        const QString boardWorkDir = linuxBoardRecoveryWorkDir();
        const bool scriptLooksFactoryDefault =
            currentScript.isEmpty()
            || currentScript == QLatin1String("start-all.sh")
            || currentScript == QLatin1String("start-all.bat");
        const bool workDirIsGeneric = currentWorkDir.isEmpty() || currentWorkDir == appDir;

        if (!boardScriptPath.isEmpty()
            && (currentMode.isEmpty() || currentMode == QLatin1String("local")
                || scriptLooksFactoryDefault || workDirIsGeneric))
        {
            setValue("service/startMode", "remote");
            setValue("service/remoteFallbackLocal", true);
            setValue("service/remoteRequirePorts", true);

            if (scriptLooksFactoryDefault)
            {
                setValue("service/scriptPath", boardScriptPath);
            }

            if (!boardWorkDir.isEmpty() && workDirIsGeneric)
            {
                setValue("service/workDir", boardWorkDir);
            }
        }

        setValue("service/startModeMigratedV5LinuxCheckFirstDefault", true);
    }

    // Linux 板端补充迁移V6：
    // 原产品 outage/start.sh 只显式处理 8081，因此当前板端默认关键端口也收口到 8081，
    // 避免把 9000/9001 误当成前置门禁导致无法按原产品方式恢复。
    if (!m_settings->contains("service/portsMigratedV6LinuxOutageDefault"))
    {
        if (!linuxBoardRecoveryScriptPath().isEmpty())
        {
            const QString currentPorts = m_settings->value("service/ports").toString().trimmed();
            if (currentPorts.isEmpty()
                || currentPorts == QLatin1String("8081,9000,9001")
                || currentPorts == QLatin1String("8081,9000"))
            {
                setValue("service/ports", "8081");
            }
        }
        setValue("service/portsMigratedV6LinuxOutageDefault", true);
    }

    if (!m_settings->contains("service/remoteRequiredPortsMigratedV6LinuxOutageDefault"))
    {
        if (!linuxBoardRecoveryScriptPath().isEmpty())
        {
            const QString currentRequiredPorts = m_settings->value("service/remoteRequiredPorts").toString().trimmed();
            if (currentRequiredPorts.isEmpty()
                || currentRequiredPorts == QLatin1String("8081,9000")
                || currentRequiredPorts == QLatin1String("8081,9000,9001"))
            {
                setValue("service/remoteRequiredPorts", "8081");
            }
        }
        setValue("service/remoteRequiredPortsMigratedV6LinuxOutageDefault", true);
    }
#endif

    // 兼容迁移V6：
    // 2026-03-24 曾临时将 service/forceRestartScript 默认改为 true，
    // 但后续定稿决定恢复为“原产品式：服务健康就不重启”。
    // 本迁移只执行一次，把历史 true 拉回 false；之后用户如需强制重启，可再显式打开。
    if (!m_settings->contains("service/forceRestartScriptMigratedV1ProductStyleDefault"))
    {
        if (m_settings->value("service/forceRestartScript", false).toBool())
        {
            setValue("service/forceRestartScript", false);
        }
        setValue("service/forceRestartScriptMigratedV1ProductStyleDefault", true);
    }

    // ========== 传票 HTTP 接收配置 ==========
    if (!m_settings->contains("ticket/httpIngressHost"))
    {
        setValue("ticket/httpIngressHost", "127.0.0.1");
    }
    if (!m_settings->contains("ticket/httpIngressPort"))
    {
        setValue("ticket/httpIngressPort", 8888);
    }
    if (!m_settings->contains("ticket/httpIngressPath"))
    {
        setValue("ticket/httpIngressPath", "/ywticket/WebApi/transTicket");
    }
    if (!m_settings->contains("ticket/autoTransferEnabled"))
    {
        setValue("ticket/autoTransferEnabled", true);
    }
    if (!m_settings->contains("ticket/autoTransferEnabledMigratedV1"))
    {
        setValue("ticket/autoTransferEnabled", true);
        setValue("ticket/autoTransferEnabledMigratedV1", true);
    }
    if (!m_settings->contains("ticket/httpReturnUrl"))
    {
        setValue("ticket/httpReturnUrl", "");
    }
    if (!m_settings->contains("auth/accountListUrl"))
    {
        setValue("auth/accountListUrl", "");
    }
    if (!m_settings->contains("auth/cardLoginUrl"))
    {
        setValue("auth/cardLoginUrl", "");
    }
    if (!m_settings->contains("system/userListUrl"))
    {
        setValue("system/userListUrl", "");
    }
    if (!m_settings->contains("system/updateCardNoUrl"))
    {
        setValue("system/updateCardNoUrl", "");
    }
    if (!m_settings->contains("input/softKeyboardProvider"))
    {
        setValue("input/softKeyboardProvider", "none");
    }
    if (!m_settings->contains("input/softKeyboardEnabledPages"))
    {
        setValue("input/softKeyboardEnabledPages", "login,system");
    }
    if (!m_settings->contains("input/qtVirtualKeyboardStyle"))
    {
        setValue("input/qtVirtualKeyboardStyle", "");
    }
    if (!m_settings->contains("input/softKeyboardProviderMigratedV1EnableQtVk"))
    {
        // 历史兼容迁移：把旧 provider 统一降级为 none，避免残留的 qtvirtualkeyboard 口径误入当前主线。
        const QString currentProvider =
            m_settings->value("input/softKeyboardProvider").toString().trimmed().toLower();
        if (currentProvider.isEmpty() || currentProvider == "qtvirtualkeyboard")
        {
            setValue("input/softKeyboardProvider", "none");
        }
        setValue("input/softKeyboardProviderMigratedV1EnableQtVk", true);
    }
    if (!m_settings->contains("ticket/debugFrameChunkSize"))
    {
        // 调试开关：0 表示使用协议正式默认分帧大小，不强制拆帧。
        setValue("ticket/debugFrameChunkSize", 0);
    }
    if (!m_settings->contains("key/backendStationNo"))
    {
        // 初始化/RFID 后端取数使用的业务站号。0 表示按后端约定取全站聚合数据。
        setValue("key/backendStationNo", 0);
    }
}

// ========== 系统配置 ==========

QString ConfigManager::homeUrl() const
{
    return m_settings->value("system/homeUrl").toString();
}

void ConfigManager::setHomeUrl(const QString &url)
{
    m_settings->setValue("system/homeUrl", url);
    emit configChanged("system/homeUrl", url);
}

QString ConfigManager::apiUrl() const
{
    return m_settings->value("system/apiUrl").toString();
}

void ConfigManager::setApiUrl(const QString &url)
{
    m_settings->setValue("system/apiUrl", url);
    emit configChanged("system/apiUrl", url);
}

QString ConfigManager::stationId() const
{
    return m_settings->value("system/stationId").toString();
}

void ConfigManager::setStationId(const QString &id)
{
    m_settings->setValue("system/stationId", id);
    emit configChanged("system/stationId", id);
}

QString ConfigManager::tenantCode() const
{
    return m_settings->value("system/tenantCode").toString();
}

void ConfigManager::setTenantCode(const QString &code)
{
    m_settings->setValue("system/tenantCode", code);
    emit configChanged("system/tenantCode", code);
}

// ========== 串口配置 ==========

QString ConfigManager::keySerialPort() const
{
    return m_settings->value("serial/keyPort").toString();
}

void ConfigManager::setKeySerialPort(const QString &port)
{
    m_settings->setValue("serial/keyPort", port);
    emit configChanged("serial/keyPort", port);
}

QString ConfigManager::cardSerialPort() const
{
    return m_settings->value("serial/cardPort").toString();
}

void ConfigManager::setCardSerialPort(const QString &port)
{
    m_settings->setValue("serial/cardPort", port);
    emit configChanged("serial/cardPort", port);
}

int ConfigManager::baudRate() const
{
    return m_settings->value("serial/baudRate").toInt();
}

void ConfigManager::setBaudRate(int rate)
{
    m_settings->setValue("serial/baudRate", rate);
    emit configChanged("serial/baudRate", rate);
}

int ConfigManager::dataBits() const
{
    return m_settings->value("serial/dataBits").toInt();
}

void ConfigManager::setDataBits(int bits)
{
    m_settings->setValue("serial/dataBits", bits);
    emit configChanged("serial/dataBits", bits);
}

// ========== 高级配置 ==========

int ConfigManager::debugMode() const
{
    return m_settings->value("advanced/debugMode").toInt();
}

void ConfigManager::setDebugMode(int mode)
{
    m_settings->setValue("advanced/debugMode", mode);
    emit configChanged("advanced/debugMode", mode);
}

int ConfigManager::logLevel() const
{
    return m_settings->value("advanced/logLevel").toInt();
}

void ConfigManager::setLogLevel(int level)
{
    m_settings->setValue("advanced/logLevel", level);
    emit configChanged("advanced/logLevel", level);
}

int ConfigManager::connectionTimeout() const
{
    return m_settings->value("advanced/connectionTimeout").toInt();
}

void ConfigManager::setConnectionTimeout(int seconds)
{
    m_settings->setValue("advanced/connectionTimeout", seconds);
    emit configChanged("advanced/connectionTimeout", seconds);
}

int ConfigManager::retryCount() const
{
    return m_settings->value("advanced/retryCount").toInt();
}

void ConfigManager::setRetryCount(int count)
{
    m_settings->setValue("advanced/retryCount", count);
    emit configChanged("advanced/retryCount", count);
}

bool ConfigManager::autoStart() const
{
    return m_settings->value("advanced/autoStart").toBool();
}

void ConfigManager::setAutoStart(bool enabled)
{
    m_settings->setValue("advanced/autoStart", enabled);
    emit configChanged("advanced/autoStart", enabled);
}

bool ConfigManager::rememberLogin() const
{
    return m_settings->value("advanced/rememberLogin").toBool();
}

void ConfigManager::setRememberLogin(bool enabled)
{
    m_settings->setValue("advanced/rememberLogin", enabled);
    emit configChanged("advanced/rememberLogin", enabled);
}

bool ConfigManager::soundEnabled() const
{
    return m_settings->value("advanced/soundEnabled").toBool();
}

void ConfigManager::setSoundEnabled(bool enabled)
{
    m_settings->setValue("advanced/soundEnabled", enabled);
    emit configChanged("advanced/soundEnabled", enabled);
}

// ========== 通用方法 ==========

QVariant ConfigManager::value(const QString &key, const QVariant &defaultValue) const
{
    return m_settings->value(key, defaultValue);
}

void ConfigManager::setValue(const QString &key, const QVariant &value)
{
    m_settings->setValue(key, value);
    emit configChanged(key, value);
}

void ConfigManager::sync()
{
    m_settings->sync();
}

void ConfigManager::reload()
{
    m_settings->sync();
}

void ConfigManager::resetToDefaults()
{
    m_settings->clear();
    loadDefaults();
    sync();
}
