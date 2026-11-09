#ifndef UILAYOUTPROFILERESOLVER_H
#define UILAYOUTPROFILERESOLVER_H

#include "shared/ui/UiLayoutProfile.h"
#include <QApplication>
#include <QFontMetrics>
#include <QScreen>

class UiLayoutProfileResolver
{
public:
    // Converts screen facts into presentation-only metrics; feature views never
    // inspect platform geometry independently.
    static UiLayoutProfile resolve(const QScreen *screen, const QSize &clientSize)
    {
        UiLayoutProfile profile;
        profile.availableSize = screen ? screen->availableGeometry().size() : clientSize;
        profile.clientSize = clientSize.isValid() ? clientSize : profile.availableSize;
        const QSize basis(qMin(profile.availableSize.width(), profile.clientSize.width()),
                          qMin(profile.availableSize.height(), profile.clientSize.height()));
        profile.isNarrowWidth = basis.width() < 1200;
        profile.isShortHeight = basis.height() < 760;
        if (profile.isNarrowWidth || profile.isShortHeight)
            profile.mode = UiLayoutMode::Compact;
        else if (basis.width() >= 1920 && basis.height() >= 1000)
            profile.mode = UiLayoutMode::Wide;
        else
            profile.mode = UiLayoutMode::Standard;
#ifdef Q_OS_LINUX
        profile.isTouchFirst = true;
#endif
        profile.contentHorizontalMargin = profile.isNarrowWidth ? 12 : 24;
        profile.keyboardMaximumHeight = qBound(180, profile.clientSize.height() - 390, 260);
#ifdef Q_OS_WIN
        qreal systemPointSize = QApplication::font().pointSizeF();
        if (systemPointSize <= 0.0)
            systemPointSize = 9.0;

        profile.navigationFontPointSize = qMax(14.0, systemPointSize + 3.0);
        profile.buttonFontPointSize = qMax(12.0, systemPointSize + 1.5);

        QFont navigationFont = QApplication::font();
        navigationFont.setPointSizeF(profile.navigationFontPointSize);
        profile.navigationButtonMinimumHeight = qMax(
                    48, QFontMetrics(navigationFont).height() + 20);

        QFont buttonFont = QApplication::font();
        buttonFont.setPointSizeF(profile.buttonFontPointSize);
        profile.buttonMinimumHeight = qMax(
                    44, QFontMetrics(buttonFont).height() + 18);
#endif
        return profile;
    }
};

#endif // UILAYOUTPROFILERESOLVER_H
