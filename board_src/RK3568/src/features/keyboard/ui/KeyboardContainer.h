/**
 * @file KeyboardContainer.h
 * @brief 普通 QWidget 键盘宿主容器
 */

#ifndef KEYBOARDCONTAINER_H
#define KEYBOARDCONTAINER_H

#include "features/keyboard/domain/KeyboardTypes.h"

#include <QPointer>
#include <QWidget>

class CandidateBarWidget;
class NormalKeyboard;
class NumKeyboard;
class SymbolKeyboard;
class UrlKeyboard;
class QLineEdit;
class QStackedLayout;
class KeyboardPinyinEngine;

class KeyboardContainer : public QWidget
{
    Q_OBJECT

public:
    explicit KeyboardContainer(QWidget *parent = nullptr);
    void setKeyboardHeight(int height);
    void showFor(QLineEdit *target, keyboard::KeyboardMode mode, bool allowUrlSwitch);
    void hideKeyboard();
    bool isKeyboardVisible() const;
    int keyboardHeight() const;

signals:
    void visibilityChanged(bool visible, int height);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void applyMode(keyboard::KeyboardMode mode, bool allowUrlSwitch);
    void updateContainerHeight();
    void updateOverlayGeometry();
    void handleKey(Qt::Key key, const QString &text, Qt::KeyboardModifiers modifiers);

    QPointer<QLineEdit> m_target;
    QStackedLayout *m_stack;
    CandidateBarWidget *m_candidateBar;
    KeyboardPinyinEngine *m_pinyinEngine;
    int m_baseKeyboardHeight;
    NormalKeyboard *m_normalKeyboard;
    NumKeyboard *m_numKeyboard;
    SymbolKeyboard *m_symbolKeyboard;
    UrlKeyboard *m_urlKeyboard;
};

#endif // KEYBOARDCONTAINER_H
