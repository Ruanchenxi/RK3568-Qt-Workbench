/**
 * @file NumKeyboard.cpp
 * @brief 数字键盘实现
 */

#include "features/keyboard/ui/NumKeyboard.h"
#include "features/keyboard/ui/KeyboardTheme.h"

#include <QHBoxLayout>
#include <QPushButton>
#include <QVBoxLayout>

namespace
{
QPushButton *makeNumButton(const QString &text, const QSize &size = QSize(0, 0))
{
    QPushButton *button = new QPushButton(text);
    if (size.isValid() && size.width() > 0 && size.height() > 0)
    {
        button->setMinimumSize(size);
    }
    button->setFocusPolicy(Qt::NoFocus);
    return button;
}
}

NumKeyboard::NumKeyboard(QWidget *parent)
    : QWidget(parent),
      m_urlSwitchButton(nullptr)
{
    buildUi();
}

void NumKeyboard::buildUi()
{
    QVBoxLayout *root = new QVBoxLayout(this);
    root->setSpacing(KeyboardTheme::kGap);
    root->setContentsMargins(8, 8, 8, 8);

    const QStringList rows = {
        QStringLiteral("1 2 3"),
        QStringLiteral("4 5 6"),
        QStringLiteral("7 8 9")
    };

    for (const QString &rowText : rows)
    {
        QHBoxLayout *row = new QHBoxLayout;
        row->setSpacing(KeyboardTheme::kGap);
        const QStringList keys = rowText.split(' ', Qt::SkipEmptyParts);
        for (const QString &text : keys)
        {
            QPushButton *button = makeNumButton(text, QSize(104, 42));
            connect(button, &QPushButton::clicked, this, [this, text]() {
                emit keyPressed(Qt::Key_unknown, text, Qt::NoModifier);
            });
            row->addWidget(button);
        }
        root->addLayout(row);
    }

    QHBoxLayout *bottom = new QHBoxLayout;
    bottom->setSpacing(KeyboardTheme::kGap);
    QPushButton *normal = makeNumButton(QStringLiteral("ABC"), QSize(104, 42));
    m_urlSwitchButton = makeNumButton(QStringLiteral("URL"), QSize(104, 42));
    QPushButton *dot = makeNumButton(QStringLiteral("."), QSize(104, 42));
    QPushButton *zero = makeNumButton(QStringLiteral("0"), QSize(104, 42));
    QPushButton *backspace = makeNumButton(QStringLiteral("退格"), QSize(104, 42));
    QPushButton *hide = makeNumButton(QStringLiteral("收起"), QSize(104, 42));
    bottom->addWidget(normal);
    bottom->addWidget(m_urlSwitchButton);
    bottom->addWidget(dot);
    bottom->addWidget(zero);
    bottom->addWidget(backspace);
    bottom->addWidget(hide);
    root->addLayout(bottom);

    connect(normal, &QPushButton::clicked, this, &NumKeyboard::switchToNormal);
    connect(m_urlSwitchButton, &QPushButton::clicked, this, &NumKeyboard::switchToUrl);
    connect(dot, &QPushButton::clicked, this, [this]() {
        emit keyPressed(Qt::Key_Period, QStringLiteral("."), Qt::NoModifier);
    });
    connect(zero, &QPushButton::clicked, this, [this]() {
        emit keyPressed(Qt::Key_unknown, QStringLiteral("0"), Qt::NoModifier);
    });
    connect(backspace, &QPushButton::clicked, this, [this]() {
        emit keyPressed(Qt::Key_Backspace, QString(), Qt::NoModifier);
    });
    connect(hide, &QPushButton::clicked, this, &NumKeyboard::hideKeyboard);
}

void NumKeyboard::setUrlSwitchVisible(bool visible)
{
    if (m_urlSwitchButton)
    {
        m_urlSwitchButton->setVisible(visible);
    }
}
