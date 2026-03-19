/**
 * @file systempage.h
 * @brief 系统设置页面头文件
 */

#ifndef SYSTEMPAGE_H
#define SYSTEMPAGE_H

#include <QWidget>
#include <QStackedWidget>
#include <QMessageBox>
#include <QComboBox>
#include <QPointer>
#include <QSpinBox>
#include <QCheckBox>
#include <QTableWidget>
#include <QPushButton>
#include "features/system/application/SystemController.h"

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

public slots:
    void onKeyboardTargetChanged(QWidget *target);
    void onKeyboardVisibilityChanged(bool visible, int height);

private slots:
    void onTabBasicClicked();
    void onTabAdvancedClicked();
    void onRefreshClicked();
    void onSaveClicked();
    void onCollectCardClicked();        // 采指纹按钮
    void onUserTableSelectionChanged(); // 用户表格选择变化

private:
    void setupConnections();
    void loadSettings();
    void saveSettings();
    void createUserIdentityContent(); // 创建用户身份采集页面
    void applyGlobalStyles();         // 应用全局样式
    void ensureKeyboardTargetVisible();
    void updateTabStyles();           // 更新标签按钮样式
    void loadUserList();              // 加载用户列表

    Ui::SystemPage *ui;
    SystemController *m_controller;
    QWidget *m_userIdentityWidget; // 用户身份采集页面
    QPointer<QLineEdit> m_keyboardTarget;
    bool m_keyboardVisible;
    int m_keyboardHeight;
    int m_lastKeyboardScrollValue;
    bool m_keyboardAdjustedScroll;

    // 用户身份采集控件
    QTableWidget *m_userTable;
    QPushButton *m_collectCardBtn;
    QPushButton *m_collectFingerprintBtn;
};

#endif // SYSTEMPAGE_H
