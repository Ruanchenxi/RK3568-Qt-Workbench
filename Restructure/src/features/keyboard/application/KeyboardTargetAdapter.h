/**
 * @file KeyboardTargetAdapter.h
 * @brief 键盘目标适配器：第一期仅支持 QLineEdit
 */

#ifndef KEYBOARDTARGETADAPTER_H
#define KEYBOARDTARGETADAPTER_H

#include <QString>

class QComboBox;
class QLineEdit;
class QWidget;

class KeyboardTargetAdapter
{
public:
    static QLineEdit *lineEditFrom(QWidget *widget);
    static QComboBox *comboBoxFrom(QWidget *widget);
    static void insertText(QLineEdit *target, const QString &text);
    static void backspace(QLineEdit *target);
    static void commit(QLineEdit *target);
};

#endif // KEYBOARDTARGETADAPTER_H
