/**
 * @file systempage.h
 * @brief 系统设置页面头文件
 *
 * 基础页静态结构以 .ui 为主，C++ 仅承载数据绑定和页内交互编排。
 */

#ifndef SYSTEMPAGE_H
#define SYSTEMPAGE_H

#include <QList>
#include <QPointer>
#include <QWidget>
#include "features/auth/domain/AuthTypes.h"
#include "shared/contracts/SystemIdentityUserDto.h"
#include "features/system/application/SystemController.h"

class QLineEdit;
class CardSerialSource;

namespace Ui
{
    class SystemPage;
}

class SystemPage : public QWidget
{
    Q_OBJECT

public:
    explicit SystemPage(QWidget *parent = nullptr);
    ~SystemPage();

signals:
    void cardReaderStatusChanged(int status, const QString &message);

public slots:
    void onKeyboardTargetChanged(QWidget *target);
    void onKeyboardVisibilityChanged(bool visible, int height);

private slots:
    void onTabBasicClicked();
    void onTabAdvancedClicked();
    void onRefreshClicked();
    void onSaveClicked();
    void onDeleteCardClicked();
    void onCollectCardClicked();        // 采卡号按钮
    void onUserTableSelectionChanged(); // 用户表格选择变化
    void onUserListChanged();
    void onUserListLoadFailed(const QString &message);
    void onUserListLoadingChanged(bool inProgress);
    void onCardCaptured(const CardCredential &credential);
    void onCardSourceError(const QString &message);
    void onCardNoUpdated(const QString &userId, const QString &cardNo);
    void onCardNoUpdateFailed(const QString &message);
    void onCardNoUpdateStateChanged(bool inProgress);

private:
    void setupConnections();
    void configureUserTable();
    void updateTabStyles();
    void loadSettings();
    void saveSettings();
    void ensureKeyboardTargetVisible();
    void loadUserList();              // 加载用户列表
    void repopulateUserTable();
    void resetCardCollectionState();
    void resetIdentityViewForSessionChange();
    void updateCollectButtonState();
    void updateIdentityPermissionState();
    bool currentUserCanManageIdentityMedia() const;
    int selectedUserRow() const;
    QString displayNameForUser(const SystemIdentityUserDto &user) const;

    Ui::SystemPage *ui;
    SystemController *m_controller;
    CardSerialSource *m_cardSource;
    QPointer<QLineEdit> m_keyboardTarget;
    bool m_keyboardVisible;
    int m_keyboardHeight;
    int m_lastKeyboardScrollValue;
    bool m_keyboardAdjustedScroll;
    bool m_userListLoaded;
    bool m_canManageIdentityMedia;
    bool m_identityReadOnlyPromptShown;
    bool m_clearingCard;
    bool m_collectingCard;
    QString m_collectingUserId;
    QString m_collectingDisplayName;
    QList<SystemIdentityUserDto> m_users;
};

#endif // SYSTEMPAGE_H
