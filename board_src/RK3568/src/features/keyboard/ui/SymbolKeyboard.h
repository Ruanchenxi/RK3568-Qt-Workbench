/**
 * @file SymbolKeyboard.h
 * @brief 符号键盘
 */

#ifndef SYMBOLKEYBOARD_H
#define SYMBOLKEYBOARD_H

#include <QWidget>

class SymbolKeyboard : public QWidget
{
    Q_OBJECT

public:
    explicit SymbolKeyboard(QWidget *parent = nullptr);

signals:
    void changeSymbol();
    void hideKeyboard();
    void keyPressed(Qt::Key key, const QString &text, Qt::KeyboardModifiers modifiers);

private:
    void buildUi();
};

#endif // SYMBOLKEYBOARD_H
