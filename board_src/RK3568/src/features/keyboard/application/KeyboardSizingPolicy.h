/**
 * @file KeyboardSizingPolicy.h
 * @brief 键盘页面级尺寸策略
 */

#ifndef KEYBOARDSIZINGPOLICY_H
#define KEYBOARDSIZINGPOLICY_H

#include <QString>

class KeyboardSizingPolicy
{
public:
    static int keyboardHeightForPage(const QString &pageKey);
};

#endif // KEYBOARDSIZINGPOLICY_H
