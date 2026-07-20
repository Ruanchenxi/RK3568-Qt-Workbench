/**
 * @file NormalKeyboard.h
 * @brief 普通字母键盘
 */

#ifndef NORMALKEYBOARD_H
#define NORMALKEYBOARD_H

#include <QString>
#include <QWidget>

class QPushButton;
class QVBoxLayout;

class NormalKeyboard : public QWidget
{
    Q_OBJECT

public:
    explicit NormalKeyboard(QWidget *parent = nullptr);
    void setLanguageName(const QString &name);
    void setUrlSwitchVisible(bool visible);

signals:
    void toggleLanguage();
    void switchToNumeric();
    void switchToUrl();
    void changeSymbol();
    void hideKeyboard();
    void keyPressed(Qt::Key key, const QString &text, Qt::KeyboardModifiers modifiers);

private:
    bool m_uppercase;
    QPushButton *m_languageButton;
    QPushButton *m_urlSwitchButton;

    void buildUi();
    void addLetterRow(QVBoxLayout *layout, const QString &letters);
    void setUppercase(bool enabled);
};

#endif // NORMALKEYBOARD_H
