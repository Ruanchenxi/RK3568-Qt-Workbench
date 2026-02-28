/**
 * @file ConfigManager.h
 * @brief 配置管理器 - 单例模式统一管理所有配置
 * @author RK3568项目组
 * @date 2026-02-01
 */

#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include <QObject>
#include <QString>
#include <QSettings>
#include <QVariant>

/**
 * @class ConfigManager
 * @brief 配置管理器单例类
 *
 * 统一管理应用配置，支持：
 * - 读取/写入配置
 * - 默认值设置
 * - 配置变更通知
 */
class ConfigManager : public QObject
{
    Q_OBJECT

private:
    explicit ConfigManager(QObject *parent = nullptr);
    ~ConfigManager();

    ConfigManager(const ConfigManager &) = delete;
    ConfigManager &operator=(const ConfigManager &) = delete;

public:
    static ConfigManager *instance();

    // ========== 系统配置 ==========

    QString homeUrl() const;
    void setHomeUrl(const QString &url);

    QString apiUrl() const;
    void setApiUrl(const QString &url);

    QString stationId() const;
    void setStationId(const QString &id);

    QString tenantCode() const;
    void setTenantCode(const QString &code);

    // ========== 串口配置 ==========

    QString keySerialPort() const;
    void setKeySerialPort(const QString &port);

    QString cardSerialPort() const;
    void setCardSerialPort(const QString &port);

    int baudRate() const;
    void setBaudRate(int rate);

    int dataBits() const;
    void setDataBits(int bits);

    // ========== 高级配置 ==========

    int debugMode() const;
    void setDebugMode(int mode);

    int logLevel() const;
    void setLogLevel(int level);

    int connectionTimeout() const;
    void setConnectionTimeout(int seconds);

    int retryCount() const;
    void setRetryCount(int count);

    bool autoStart() const;
    void setAutoStart(bool enabled);

    bool rememberLogin() const;
    void setRememberLogin(bool enabled);

    bool soundEnabled() const;
    void setSoundEnabled(bool enabled);

    // ========== 通用方法 ==========

    /**
     * @brief 获取任意配置值
     */
    QVariant value(const QString &key, const QVariant &defaultValue = QVariant()) const;

    /**
     * @brief 设置任意配置值
     */
    void setValue(const QString &key, const QVariant &value);

    /**
     * @brief 同步配置到磁盘
     */
    void sync();

    /**
     * @brief 重新加载配置
     */
    void reload();

    /**
     * @brief 重置为默认配置
     */
    void resetToDefaults();

signals:
    /**
     * @brief 配置变更信号
     */
    void configChanged(const QString &key, const QVariant &value);

private:
    void loadDefaults();

    static ConfigManager *s_instance;
    QSettings *m_settings;
};

#endif // CONFIGMANAGER_H
