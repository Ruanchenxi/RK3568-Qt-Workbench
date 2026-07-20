/**
 * @file SystemController.h
 * @brief 系统设置页面控制器
 */
#ifndef SYSTEMCONTROLLER_H
#define SYSTEMCONTROLLER_H

#include <QObject>
#include <QByteArray>
#include <QList>
#include <QPointer>
#include <QStringList>

#include "shared/contracts/SystemSettingsDto.h"
#include "shared/contracts/SystemIdentityUserDto.h"

class QNetworkAccessManager;
class QNetworkReply;
class QNetworkRequest;

class SystemController : public QObject
{
    Q_OBJECT
public:
    explicit SystemController(QObject *parent = nullptr);
    ~SystemController() override = default;

    SystemSettingsDto loadSettings() const;
    void saveSettings(const SystemSettingsDto &settings) const;
    /** @brief 返回可用串口名称列表，Windows 返回 COMx，Linux 返回 /dev/ttySx 并补充常用路径 */
    QStringList availableSerialPorts() const;
    QList<SystemIdentityUserDto> users() const;
    void requestUserList();
    void updateUserCardNo(const QString &userId, const QString &cardNo);
    void clearUserCardNo(const QString &userId);
    void updateUserFingerprint(const QString &userId, const QByteArray &templateData);
    void clearUserFingerprint(const QString &userId);
    bool isUserListLoading() const;
    bool isCardUpdateInProgress() const;
    bool isFingerprintUpdateInProgress() const;

signals:
    void userListChanged();
    void userListLoadFailed(const QString &errorMessage);
    void userListLoadingChanged(bool inProgress);
    void cardNoUpdated(const QString &userId, const QString &cardNo);
    void cardNoUpdateFailed(const QString &errorMessage);
    void cardNoUpdateStateChanged(bool inProgress);
    void fingerprintUpdated(const QString &userId, const QString &fingerprint);
    void fingerprintUpdateFailed(const QString &errorMessage);
    void fingerprintUpdateStateChanged(bool inProgress);

private:
    QString resolveTenantId() const;
    QString resolveUserListUrl() const;
    QString resolveUpdateCardNoUrl() const;
    QString resolveUpdateFingerprintUrl() const;
    void applyCommonHeaders(QNetworkRequest *request) const;
    QString extractApiErrorMessage(const QByteArray &responseData, const QString &fallbackMessage) const;
    QList<SystemIdentityUserDto> parseUserList(const QByteArray &responseData, QString *errorMessage) const;
    void submitUserCardNo(const QString &userId, const QString &cardNo);
    void submitUserFingerprint(const QString &userId, const QString &fingerprintBase64);
    void finishUserListRequest(QNetworkReply *reply);
    void finishCardUpdateRequest(QNetworkReply *reply);
    void finishFingerprintUpdateRequest(QNetworkReply *reply);

    QNetworkAccessManager *m_networkManager;
    QList<SystemIdentityUserDto> m_users;
    QPointer<QNetworkReply> m_pendingUserListReply;
    QPointer<QNetworkReply> m_pendingUpdateCardReply;
    QPointer<QNetworkReply> m_pendingUpdateFingerprintReply;
    bool m_userListLoading;
    bool m_cardUpdateInProgress;
    bool m_fingerprintUpdateInProgress;
};

#endif // SYSTEMCONTROLLER_H
