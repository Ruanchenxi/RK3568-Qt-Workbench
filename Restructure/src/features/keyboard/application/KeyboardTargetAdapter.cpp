/**
 * @file KeyboardTargetAdapter.cpp
 * @brief 键盘目标适配器实现
 */

#include "features/keyboard/application/KeyboardTargetAdapter.h"

#include <QComboBox>
#include <QLineEdit>
#include <QWidget>

QLineEdit *KeyboardTargetAdapter::lineEditFrom(QWidget *widget)
{
    return qobject_cast<QLineEdit *>(widget);
}

QComboBox *KeyboardTargetAdapter::comboBoxFrom(QWidget *widget)
{
    if (!widget)
    {
        return nullptr;
    }

    if (QComboBox *combo = qobject_cast<QComboBox *>(widget))
    {
        return combo;
    }

    return widget->parentWidget() ? qobject_cast<QComboBox *>(widget->parentWidget()) : nullptr;
}

void KeyboardTargetAdapter::insertText(QLineEdit *target, const QString &text)
{
    if (!target || text.isEmpty())
    {
        return;
    }

    target->insert(text);
}

void KeyboardTargetAdapter::backspace(QLineEdit *target)
{
    if (!target)
    {
        return;
    }
    target->backspace();
}

void KeyboardTargetAdapter::commit(QLineEdit *target)
{
    if (!target)
    {
        return;
    }

    target->editingFinished();
}
