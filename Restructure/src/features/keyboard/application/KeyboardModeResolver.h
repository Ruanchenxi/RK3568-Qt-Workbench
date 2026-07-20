/**
 * @file KeyboardModeResolver.h
 * @brief 根据输入目标解析键盘模式
 */

#ifndef KEYBOARDMODERESOLVER_H
#define KEYBOARDMODERESOLVER_H

#include "features/keyboard/domain/KeyboardTypes.h"

class QLineEdit;
class QString;

class KeyboardModeResolver
{
public:
    static keyboard::KeyboardMode resolve(QLineEdit *target);
    static bool allowUrlSwitch(QLineEdit *target);
    static QString displayName(QLineEdit *target);
    static QString modeName(keyboard::KeyboardMode mode);
};

#endif // KEYBOARDMODERESOLVER_H
