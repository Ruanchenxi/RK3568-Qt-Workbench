/**
 * @file InputMethodCoordinator.cpp
 * @brief 旧 Qt Virtual Keyboard 历史兼容壳实现
 *
 * 当前仅保留为评估态/删除前条件参考，不再驱动主构建。
 */

#include "app/InputMethodCoordinator.h"

#include <QtGlobal>

void InputMethodCoordinator::applyStartupEnvironment()
{
}

QString InputMethodCoordinator::configuredProviderName()
{
    return QStringLiteral("retired-compat-shim");
}

bool InputMethodCoordinator::isQtVirtualKeyboardEnabled()
{
    return false;
}

QWidget *InputMethodCoordinator::createEmbeddedHost(QWidget *parent)
{
    Q_UNUSED(parent);
    return nullptr;
}

bool InputMethodCoordinator::pageAllowsVirtualKeyboard(const QString &pageKey)
{
    Q_UNUSED(pageKey);
    return false;
}

void InputMethodCoordinator::installAutoShowHooks(QWidget *root)
{
    Q_UNUSED(root);
}

bool InputMethodCoordinator::isInputMethodVisible()
{
    return false;
}

void InputMethodCoordinator::showInputMethod()
{
}

void InputMethodCoordinator::hideInputMethod()
{
}

void InputMethodCoordinator::blockSoftKeyboardForWidgetTree(QWidget *root)
{
    Q_UNUSED(root);
}
