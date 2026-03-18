/**
 * @file KeyboardPagePolicy.h
 * @brief 键盘页面支持策略
 */

#ifndef KEYBOARDPAGEPOLICY_H
#define KEYBOARDPAGEPOLICY_H

#include <QString>

class KeyboardPagePolicy
{
public:
    static bool supportsCustomKeyboard(const QString &pageKey);
};

#endif // KEYBOARDPAGEPOLICY_H
