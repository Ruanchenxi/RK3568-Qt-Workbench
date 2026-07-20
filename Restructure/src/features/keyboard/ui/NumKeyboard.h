/**
 * @file NumKeyboard.h
 * @brief 数字键盘
 */

#ifndef NUMKEYBOARD_H
#define NUMKEYBOARD_H

#include <QWidget>

class QPushButton;

class NumKeyboard : public QWidget
{
    Q_OBJECT

public:
    explicit NumKeyboard(QWidget *parent = nullptr);
    void setUrlSwitchVisible(bool visible);

signals:
    void switchToUrl();
    void switchToNormal();
    void hideKeyboard();
    void keyPressed(Qt::Key key, const QString &text, Qt::KeyboardModifiers modifiers);

private:
    QPushButton *m_urlSwitchButton;
    void buildUi();
};

#endif // NUMKEYBOARD_H
