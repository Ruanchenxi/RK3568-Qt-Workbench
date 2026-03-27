/**
 * @file InputMethodCoordinator.h
 * @brief 旧 Qt Virtual Keyboard 历史兼容壳
 *
 * 该文件保留作为评估态/删除前条件参考，不再纳入当前主构建。
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
};

#endif // INPUTMETHODCOORDINATOR_H
