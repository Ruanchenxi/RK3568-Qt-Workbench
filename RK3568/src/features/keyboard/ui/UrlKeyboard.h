/**
 * @file UrlKeyboard.h
 * @brief URL 专用键盘
 */

#ifndef URLKEYBOARD_H
#define URLKEYBOARD_H

#include <QWidget>

class UrlKeyboard : public QWidget
{
    Q_OBJECT

public:
    explicit UrlKeyboard(QWidget *parent = nullptr);

signals:
    void switchToNormal();
    void switchToNumeric();
    void hideKeyboard();
    void keyPressed(Qt::Key key, const QString &text, Qt::KeyboardModifiers modifiers);

private:
    void buildUi();
};

#endif // URLKEYBOARD_H
