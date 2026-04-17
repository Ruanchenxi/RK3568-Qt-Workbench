/**
 * @file KeyboardPagePolicy.cpp
 * @brief 键盘页面支持策略实现
 */

#include "features/keyboard/application/KeyboardPagePolicy.h"

bool KeyboardPagePolicy::supportsCustomKeyboard(const QString &pageKey)
{
    return pageKey == QLatin1String("login")
           || pageKey == QLatin1String("system")
           || pageKey == QLatin1String("workbench");
}
