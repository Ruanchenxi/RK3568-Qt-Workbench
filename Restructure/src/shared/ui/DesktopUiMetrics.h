#ifndef DESKTOPUIMETRICS_H
#define DESKTOPUIMETRICS_H

#include <QPushButton>
#include <QVariant>

namespace DesktopUiMetrics
{
// Applies a derived desktop metric without losing the base style owned by .ui.
inline void applyButton(QPushButton *button, qreal pointSize, int minimumHeight)
{
    if (!button)
        return;

    static const char kBaseStyleProperty[] = "desktopMetricsBaseStyle";
    static const char kBaseMinimumHeightProperty[] = "desktopMetricsBaseMinimumHeight";
    static const char kBaseMaximumHeightProperty[] = "desktopMetricsBaseMaximumHeight";

    if (!button->property(kBaseStyleProperty).isValid()) {
        button->setProperty(kBaseStyleProperty, button->styleSheet());
        button->setProperty(kBaseMinimumHeightProperty, button->minimumHeight());
        button->setProperty(kBaseMaximumHeightProperty, button->maximumHeight());
    }

    const QString baseStyle = button->property(kBaseStyleProperty).toString();
    const QString metricsStyle = QStringLiteral(
        "\nQPushButton#%1 { font-size: %2pt; }\n")
            .arg(button->objectName(), QString::number(pointSize, 'f', 1));
    button->setStyleSheet(baseStyle + metricsStyle);

    const int baseMinimumHeight = button->property(kBaseMinimumHeightProperty).toInt();
    const int baseMaximumHeight = button->property(kBaseMaximumHeightProperty).toInt();
    button->setMinimumHeight(qMax(baseMinimumHeight, minimumHeight));
    button->setMaximumHeight(baseMaximumHeight < minimumHeight
                                 ? minimumHeight : baseMaximumHeight);
}
}

#endif // DESKTOPUIMETRICS_H
