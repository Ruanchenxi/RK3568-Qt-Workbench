/**
 * @file KeyboardController.cpp
 * @brief 键盘控制器实现
 */

#include "features/keyboard/application/KeyboardController.h"

#include "features/keyboard/application/KeyboardModeResolver.h"
#include "features/keyboard/application/KeyboardTargetAdapter.h"
#include "features/keyboard/ui/KeyboardContainer.h"

#include <QComboBox>
#include <QEvent>
#include <QLineEdit>
#include <QWidget>

KeyboardController::KeyboardController(QObject *parent)
    : QObject(parent),
      m_currentTarget(nullptr),
      m_container(nullptr)
{
}

void KeyboardController::attachContainer(KeyboardContainer *container)
{
    m_container = container;
    if (m_container)
    {
        connect(m_container, &KeyboardContainer::visibilityChanged,
                this, &KeyboardController::visibilityChanged);
    }
}

void KeyboardController::registerInputScope(QWidget *scope)
{
    if (!scope)
    {
        return;
    }

    const QList<QLineEdit *> lineEdits = scope->findChildren<QLineEdit *>();
    for (QLineEdit *edit : lineEdits)
    {
        if (edit)
        {
            edit->installEventFilter(this);
        }
    }

    const QList<QComboBox *> combos = scope->findChildren<QComboBox *>();
    for (QComboBox *combo : combos)
    {
        if (!combo || !combo->isEditable())
        {
            continue;
        }

        combo->installEventFilter(this);
        if (combo->lineEdit())
        {
            combo->lineEdit()->installEventFilter(this);
        }
    }
}

bool KeyboardController::hasTargetOnPage(QWidget *page) const
{
    return m_currentTarget && page && page->isAncestorOf(m_currentTarget);
}

bool KeyboardController::isVisible() const
{
    return m_container && m_container->isKeyboardVisible();
}

void KeyboardController::toggleOnPage(QWidget *page)
{
    if (!m_container || !hasTargetOnPage(page))
    {
        return;
    }

    if (m_container->isKeyboardVisible())
    {
        m_container->hideKeyboard();
        return;
    }

    m_container->showFor(m_currentTarget,
                         KeyboardModeResolver::resolve(m_currentTarget),
                         KeyboardModeResolver::allowUrlSwitch(m_currentTarget));
}

void KeyboardController::hide()
{
    if (m_container)
    {
        m_container->hideKeyboard();
    }
}

void KeyboardController::clearCurrentTarget()
{
    if (m_currentTarget)
    {
        m_currentTarget.clear();
        emit targetChanged(nullptr);
    }
}

bool KeyboardController::eventFilter(QObject *watched, QEvent *event)
{
    if (event && event->type() == QEvent::FocusIn)
    {
        if (QComboBox *combo = qobject_cast<QComboBox *>(watched))
        {
            if (combo->isEditable() && combo->lineEdit())
            {
                m_currentTarget = combo->lineEdit();
                emit targetChanged(m_currentTarget);
                syncVisibleMode();
            }
        }
        if (QLineEdit *lineEdit = qobject_cast<QLineEdit *>(watched))
        {
            m_currentTarget = lineEdit;
            emit targetChanged(lineEdit);
            syncVisibleMode();
        }
    }
    else if (event && event->type() == QEvent::MouseButtonRelease)
    {
        if (QComboBox *combo = qobject_cast<QComboBox *>(watched))
        {
            if (combo->isEditable() && combo->lineEdit())
            {
                m_currentTarget = combo->lineEdit();
                emit targetChanged(m_currentTarget);
                syncVisibleMode();
            }
        }
    }

    return QObject::eventFilter(watched, event);
}

void KeyboardController::syncVisibleMode()
{
    if (!m_container || !m_currentTarget || !m_container->isKeyboardVisible())
    {
        return;
    }

    m_container->showFor(m_currentTarget,
                         KeyboardModeResolver::resolve(m_currentTarget),
                         KeyboardModeResolver::allowUrlSwitch(m_currentTarget));
}
