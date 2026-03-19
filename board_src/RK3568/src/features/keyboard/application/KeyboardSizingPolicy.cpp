/**
 * @file KeyboardSizingPolicy.cpp
 * @brief 键盘页面级尺寸策略实现
 */

#include "features/keyboard/application/KeyboardSizingPolicy.h"

int KeyboardSizingPolicy::keyboardHeightForPage(const QString &pageKey)
{
    if (pageKey == QLatin1String("login"))
    {
        return 252;
    }
    if (pageKey == QLatin1String("system"))
    {
        return 260;
    }
    return 256;
}
