/**
 * @file KeyboardContainer.cpp
 * @brief 普通 QWidget 键盘宿主容器实现
 */

#include "features/keyboard/ui/KeyboardContainer.h"

#include "features/keyboard/application/KeyboardPinyinEngine.h"
#include "features/keyboard/application/KeyboardTargetAdapter.h"
#include "features/keyboard/domain/KeyboardTypes.h"
#include "features/keyboard/ui/CandidateBarWidget.h"
#include "features/keyboard/ui/NumKeyboard.h"
#include "features/keyboard/ui/NormalKeyboard.h"
#include "features/keyboard/ui/SymbolKeyboard.h"
#include "features/keyboard/ui/KeyboardTheme.h"
#include "features/keyboard/ui/UrlKeyboard.h"

#include <QEvent>
#include <QGraphicsDropShadowEffect>
#include <QLineEdit>
#include <QPushButton>
#include <QStackedLayout>
#include <QVBoxLayout>

namespace
{
constexpr auto kClearSentinel = "\x01CLEAR";

void applyKeyShadow(QWidget *root)
{
    const QList<QPushButton *> buttons = root->findChildren<QPushButton *>();
    for (QPushButton *button : buttons)
    {
        if (!button)
        {
            continue;
        }

        auto *effect = new QGraphicsDropShadowEffect(button);
        effect->setBlurRadius(KeyboardTheme::kBlur);
        effect->setOffset(0.0, 0.0);
        effect->setColor(KeyboardTheme::shadowColor());
        button->setGraphicsEffect(effect);
    }
}
}

KeyboardContainer::KeyboardContainer(QWidget *parent)
    : QWidget(parent),
      m_target(nullptr),
      m_stack(nullptr),
      m_candidateBar(nullptr),
      m_pinyinEngine(new KeyboardPinyinEngine(this)),
      m_baseKeyboardHeight(230),
      m_normalKeyboard(new NormalKeyboard(this)),
      m_numKeyboard(new NumKeyboard(this)),
      m_symbolKeyboard(new SymbolKeyboard(this)),
      m_urlKeyboard(new UrlKeyboard(this))
{
    setObjectName(QStringLiteral("customKeyboardContainer"));
    setVisible(false);
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet(QStringLiteral(
        "#customKeyboardContainer { background-color: #0C1015; border-top: 1px solid #222D36; border-top-left-radius: 20px; border-top-right-radius: 20px; }"
        "#keyboardHeader { background-color: #0C1015; border-top-left-radius: 20px; border-top-right-radius: 20px; }"
        "#keyboardCandidateBar { background-color: #11161C; border-bottom: 1px solid #1F2730; }"
        "#customKeyboardContainer QPushButton { background-color: #2F3338; color: #F4F6F8; border: 1px solid #39424C; border-radius: 16px; padding: 4px; font-size: 15px; }"
        "#customKeyboardContainer QPushButton:hover { background-color: #3A4148; }"
        "#customKeyboardContainer QPushButton:pressed { background-color: #1F6B45; border-color: #2A875A; }"));

    setFixedHeight(m_baseKeyboardHeight);

    QVBoxLayout *root = new QVBoxLayout(this);
    root->setSpacing(0);
    root->setContentsMargins(0, 0, 0, 0);

    m_candidateBar = new CandidateBarWidget(this);
    m_candidateBar->setObjectName(QStringLiteral("keyboardCandidateBar"));
    m_candidateBar->setFixedHeight(38);
    root->addWidget(m_candidateBar);

    QWidget *content = new QWidget(this);
    m_stack = new QStackedLayout(content);
    m_stack->setContentsMargins(0, 0, 0, 0);
    m_stack->addWidget(m_normalKeyboard);
    m_stack->addWidget(m_numKeyboard);
    m_stack->addWidget(m_symbolKeyboard);
    m_stack->addWidget(m_urlKeyboard);
    m_stack->setCurrentWidget(m_normalKeyboard);
    root->addWidget(content);
    applyKeyShadow(this);

    connect(m_normalKeyboard, &NormalKeyboard::changeSymbol, this, [this]() {
        m_pinyinEngine->reset();
        applyMode(keyboard::KeyboardMode::Symbol, false);
    });
    connect(m_normalKeyboard, &NormalKeyboard::switchToNumeric, this, [this]() {
        m_pinyinEngine->reset();
        applyMode(keyboard::KeyboardMode::Numeric, false);
    });
    connect(m_normalKeyboard, &NormalKeyboard::switchToUrl, this, [this]() {
        m_pinyinEngine->reset();
        applyMode(keyboard::KeyboardMode::Url, true);
    });
    connect(m_normalKeyboard, &NormalKeyboard::toggleLanguage, this, [this]() {
        m_pinyinEngine->toggleLanguage();
    });
    connect(m_symbolKeyboard, &SymbolKeyboard::changeSymbol, this, [this]() {
        applyMode(keyboard::KeyboardMode::Normal, false);
    });
    connect(m_numKeyboard, &NumKeyboard::switchToNormal, this, [this]() {
        applyMode(keyboard::KeyboardMode::Normal, false);
    });
    connect(m_numKeyboard, &NumKeyboard::switchToUrl, this, [this]() {
        applyMode(keyboard::KeyboardMode::Url, true);
    });
    connect(m_urlKeyboard, &UrlKeyboard::switchToNormal, this, [this]() {
        applyMode(keyboard::KeyboardMode::Normal, true);
    });
    connect(m_urlKeyboard, &UrlKeyboard::switchToNumeric, this, [this]() {
        applyMode(keyboard::KeyboardMode::Numeric, true);
    });
    connect(m_numKeyboard, &NumKeyboard::hideKeyboard, this, &KeyboardContainer::hideKeyboard);
    connect(m_urlKeyboard, &UrlKeyboard::hideKeyboard, this, &KeyboardContainer::hideKeyboard);
    connect(m_normalKeyboard, &NormalKeyboard::hideKeyboard, this, &KeyboardContainer::hideKeyboard);
    connect(m_symbolKeyboard, &SymbolKeyboard::hideKeyboard, this, &KeyboardContainer::hideKeyboard);
    connect(m_normalKeyboard, &NormalKeyboard::keyPressed, this, &KeyboardContainer::handleKey);
    connect(m_numKeyboard, &NumKeyboard::keyPressed, this, &KeyboardContainer::handleKey);
    connect(m_symbolKeyboard, &SymbolKeyboard::keyPressed, this, &KeyboardContainer::handleKey);
    connect(m_urlKeyboard, &UrlKeyboard::keyPressed, this, &KeyboardContainer::handleKey);
    connect(m_candidateBar, &CandidateBarWidget::candidateChosen, this, [this](int index) {
        m_pinyinEngine->chooseCandidate(index);
    });
    connect(m_pinyinEngine, &KeyboardPinyinEngine::candidatesChanged, this, [this](const QStringList &candidates) {
        if (candidates.isEmpty())
        {
            m_candidateBar->clearCandidates();
        }
        else
        {
            m_candidateBar->setCandidates(candidates);
        }
    });
    connect(m_pinyinEngine, &KeyboardPinyinEngine::committed, this, [this](const QString &text) {
        dispatchInsertText(text);
    });
    connect(m_pinyinEngine, &KeyboardPinyinEngine::preeditChanged, this, [this](const QString &text) {
        Q_UNUSED(text);
    });
    connect(m_pinyinEngine, &KeyboardPinyinEngine::languageChanged, this, [this](const QString &name) {
        m_normalKeyboard->setLanguageName(name);
    });
    m_normalKeyboard->setLanguageName(m_pinyinEngine->languageName());

    if (parent)
    {
        parent->installEventFilter(this);
    }
}

void KeyboardContainer::setKeyboardHeight(int height)
{
    if (height <= 0)
    {
        return;
    }

    m_baseKeyboardHeight = height;
    setFixedHeight(m_baseKeyboardHeight);
    if (isVisible())
    {
        updateOverlayGeometry();
        emit visibilityChanged(true, keyboardHeight());
    }
}

void KeyboardContainer::showFor(QLineEdit *target, keyboard::KeyboardMode mode, bool allowUrlSwitch)
{
    if (!target)
    {
        return;
    }

    m_target = target;
    clearActionHandlers();
    m_pinyinEngine->reset();
    applyMode(mode, allowUrlSwitch);
    updateOverlayGeometry();
    show();
    raise();
    emit visibilityChanged(true, keyboardHeight());
}

void KeyboardContainer::showStandalone(keyboard::KeyboardMode mode, bool allowUrlSwitch)
{
    m_target = nullptr;
    m_pinyinEngine->reset();
    applyMode(mode, allowUrlSwitch);
    updateOverlayGeometry();
    show();
    raise();
    emit visibilityChanged(true, keyboardHeight());
}

void KeyboardContainer::hideKeyboard()
{
    m_target = nullptr;
    clearActionHandlers();
    hide();
    emit visibilityChanged(false, 0);
}

bool KeyboardContainer::isKeyboardVisible() const
{
    return isVisible();
}

int KeyboardContainer::keyboardHeight() const
{
    return height();
}

void KeyboardContainer::setActionHandlers(std::function<void(const QString &)> insertTextHandler,
                                          std::function<void()> backspaceHandler,
                                          std::function<void()> commitHandler,
                                          std::function<void()> clearHandler)
{
    m_insertTextHandler = insertTextHandler;
    m_backspaceHandler = backspaceHandler;
    m_commitHandler = commitHandler;
    m_clearHandler = clearHandler;
}

void KeyboardContainer::clearActionHandlers()
{
    m_insertTextHandler = {};
    m_backspaceHandler = {};
    m_commitHandler = {};
    m_clearHandler = {};
}

bool KeyboardContainer::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == parentWidget() && event && event->type() == QEvent::Resize)
    {
        updateOverlayGeometry();
    }
    return QWidget::eventFilter(watched, event);
}

void KeyboardContainer::applyMode(keyboard::KeyboardMode mode, bool allowUrlSwitch)
{
    if (!m_stack)
    {
        return;
    }

    m_normalKeyboard->setUrlSwitchVisible(allowUrlSwitch);
    m_numKeyboard->setUrlSwitchVisible(allowUrlSwitch);
    if (mode != keyboard::KeyboardMode::Normal || !m_pinyinEngine->isChineseMode())
    {
        m_candidateBar->clearCandidates();
    }

    switch (mode)
    {
    case keyboard::KeyboardMode::Numeric:
        m_stack->setCurrentWidget(m_numKeyboard);
        break;
    case keyboard::KeyboardMode::Url:
        m_stack->setCurrentWidget(m_urlKeyboard);
        break;
    case keyboard::KeyboardMode::Symbol:
        m_stack->setCurrentWidget(m_symbolKeyboard);
        break;
    case keyboard::KeyboardMode::Normal:
    default:
        m_stack->setCurrentWidget(m_normalKeyboard);
        break;
    }
}

void KeyboardContainer::dispatchInsertText(const QString &text)
{
    if (m_target)
    {
        KeyboardTargetAdapter::insertText(m_target, text);
        return;
    }
    if (m_insertTextHandler)
    {
        m_insertTextHandler(text);
    }
}

void KeyboardContainer::dispatchBackspace()
{
    if (m_target)
    {
        KeyboardTargetAdapter::backspace(m_target);
        return;
    }
    if (m_backspaceHandler)
    {
        m_backspaceHandler();
    }
}

void KeyboardContainer::dispatchCommit()
{
    if (m_target)
    {
        KeyboardTargetAdapter::commit(m_target);
        return;
    }
    if (m_commitHandler)
    {
        m_commitHandler();
    }
}

void KeyboardContainer::dispatchClear()
{
    if (m_target)
    {
        m_target->clear();
        return;
    }
    if (m_clearHandler)
    {
        m_clearHandler();
    }
}

void KeyboardContainer::updateContainerHeight()
{
    setFixedHeight(m_baseKeyboardHeight);
}

void KeyboardContainer::updateOverlayGeometry()
{
    if (!parentWidget())
    {
        return;
    }

    const int width = parentWidget()->width();
    const int height = this->height();
    const int y = parentWidget()->height() - height;
    setGeometry(0, y, width, height);
}

void KeyboardContainer::handleKey(Qt::Key key, const QString &text, Qt::KeyboardModifiers modifiers)
{
    Q_UNUSED(modifiers);

    if (!m_target && !m_insertTextHandler && !m_backspaceHandler && !m_commitHandler && !m_clearHandler)
    {
        return;
    }

    switch (key)
    {
    case Qt::Key_Backspace:
        if (!m_pinyinEngine->handleKey(key, text, modifiers))
        {
            dispatchBackspace();
        }
        break;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        if (!m_pinyinEngine->handleKey(key, text, modifiers))
        {
            dispatchCommit();
        }
        break;
    default:
        if (text == QString::fromLatin1(kClearSentinel))
        {
            m_pinyinEngine->reset();
            dispatchClear();
        }
        else if (!m_pinyinEngine->handleKey(key, text, modifiers))
        {
            dispatchInsertText(text);
        }
        break;
    }
}
