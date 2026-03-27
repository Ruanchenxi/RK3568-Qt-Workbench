/**
 * @file KeyboardContainer.h
 * @brief 普通 QWidget 键盘宿主容器
 */

#ifndef KEYBOARDCONTAINER_H
#define KEYBOARDCONTAINER_H

#include "features/keyboard/domain/KeyboardTypes.h"

#include <functional>
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
    void showStandalone(keyboard::KeyboardMode mode, bool allowUrlSwitch);
    void hideKeyboard();
    bool isKeyboardVisible() const;
    int keyboardHeight() const;
    void setActionHandlers(std::function<void(const QString &)> insertTextHandler,
                           std::function<void()> backspaceHandler,
                           std::function<void()> commitHandler,
                           std::function<void()> clearHandler = {});
    void clearActionHandlers();

signals:
    void visibilityChanged(bool visible, int height);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void applyMode(keyboard::KeyboardMode mode, bool allowUrlSwitch);
    void dispatchInsertText(const QString &text);
    void dispatchBackspace();
    void dispatchCommit();
    void dispatchClear();
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
    std::function<void(const QString &)> m_insertTextHandler;
    std::function<void()> m_backspaceHandler;
    std::function<void()> m_commitHandler;
    std::function<void()> m_clearHandler;
};

#endif // KEYBOARDCONTAINER_H
