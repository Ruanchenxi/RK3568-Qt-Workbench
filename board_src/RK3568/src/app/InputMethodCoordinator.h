/**
 * @file InputMethodCoordinator.h
 * @brief 输入法协调层：隔离 Qt Virtual Keyboard 与业务页面
 */

#ifndef INPUTMETHODCOORDINATOR_H
#define INPUTMETHODCOORDINATOR_H

#include <QString>

class QWidget;

class InputMethodCoordinator
{
public:
    static void applyStartupEnvironment();
    static QString configuredProviderName();
    static bool isQtVirtualKeyboardEnabled();
    static QWidget *createEmbeddedHost(QWidget *parent);
    static bool pageAllowsVirtualKeyboard(const QString &pageKey);
    static void installAutoShowHooks(QWidget *root);
    static bool isInputMethodVisible();
    static void showInputMethod();
    static void hideInputMethod();
    static void blockSoftKeyboardForWidgetTree(QWidget *root);

private:
    static QString resolveProviderName();
    static QString readSettingString(const QString &key, const QString &defaultValue);
};

#endif // INPUTMETHODCOORDINATOR_H
