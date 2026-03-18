/**
 * @file KeyboardTheme.h
 * @brief 键盘视觉主题常量
 */

#ifndef KEYBOARDTHEME_H
#define KEYBOARDTHEME_H

#include <QColor>
#include <QSize>

namespace KeyboardTheme
{
static constexpr int kPanelOuterRadius = 20;
static constexpr int kKeyInnerRadius = 16;
static constexpr int kKeyPadding = 4;
static constexpr int kGap = 8;
static constexpr int kBlur = 16;

static inline QColor shadowColor()
{
    return QColor(0x9C, 0xA4, 0xAC, 77);
}

static inline QSize letterKeySize()
{
    return QSize(56, 42);
}

static inline QSize compactActionSize()
{
    return QSize(78, 42);
}

static inline QSize actionSize()
{
    return QSize(92, 44);
}
}

#endif // KEYBOARDTHEME_H
