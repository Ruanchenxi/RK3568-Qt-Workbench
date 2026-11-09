#ifndef UILAYOUTPROFILE_H
#define UILAYOUTPROFILE_H

#include <QSize>
#include <QtGlobal>

enum class UiLayoutMode { Compact, Standard, Wide };

// Immutable presentation metrics distributed by MainWindow; it contains no
// business or device state.
struct UiLayoutProfile
{
    UiLayoutMode mode = UiLayoutMode::Compact;
    QSize availableSize;
    QSize clientSize;
    bool isTouchFirst = false;
    bool isNarrowWidth = true;
    bool isShortHeight = true;
    int contentHorizontalMargin = 12;
    int keyboardMaximumHeight = 220;
    qreal navigationFontPointSize = 14.0;
    qreal buttonFontPointSize = 12.0;
    int navigationButtonMinimumHeight = 48;
    int buttonMinimumHeight = 44;

    bool operator==(const UiLayoutProfile &other) const
    {
        // Raw window dimensions are diagnostic facts. Reapplying every widget
        // for a one-pixel resize is unnecessary when all derived metrics match.
        return mode == other.mode
                && isTouchFirst == other.isTouchFirst
                && isNarrowWidth == other.isNarrowWidth
                && isShortHeight == other.isShortHeight
                && contentHorizontalMargin == other.contentHorizontalMargin
                && keyboardMaximumHeight == other.keyboardMaximumHeight
                && qFuzzyCompare(navigationFontPointSize, other.navigationFontPointSize)
                && qFuzzyCompare(buttonFontPointSize, other.buttonFontPointSize)
                && navigationButtonMinimumHeight == other.navigationButtonMinimumHeight
                && buttonMinimumHeight == other.buttonMinimumHeight;
    }

    bool operator!=(const UiLayoutProfile &other) const { return !(*this == other); }
};

#endif // UILAYOUTPROFILE_H
