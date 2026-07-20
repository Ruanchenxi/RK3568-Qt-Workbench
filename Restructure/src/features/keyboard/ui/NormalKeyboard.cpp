/**
 * @file NormalKeyboard.cpp
 * @brief 普通字母键盘实现
 */

#include "features/keyboard/ui/NormalKeyboard.h"
#include "features/keyboard/ui/KeyboardTheme.h"

#include <QAbstractButton>
#include <QButtonGroup>
#include <QHBoxLayout>
#include <QPushButton>
#include <QVBoxLayout>

namespace
{
QPushButton *makeKeyButton(const QString &text, const QSize &size = QSize(0, 0))
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

NormalKeyboard::NormalKeyboard(QWidget *parent)
    : QWidget(parent),
      m_uppercase(false),
      m_languageButton(nullptr),
      m_urlSwitchButton(nullptr)
{
    buildUi();
    setUppercase(false);
}

void NormalKeyboard::buildUi()
{
    QVBoxLayout *root = new QVBoxLayout(this);
    root->setSpacing(KeyboardTheme::kGap);
    root->setContentsMargins(8, 8, 8, 8);

    addLetterRow(root, QStringLiteral("QWERTYUIOP"));
    addLetterRow(root, QStringLiteral("ASDFGHJKL"));

    QHBoxLayout *thirdRow = new QHBoxLayout;
    thirdRow->setSpacing(KeyboardTheme::kGap);

    QPushButton *shift = makeKeyButton(QStringLiteral("Shift"), KeyboardTheme::compactActionSize());
    QPushButton *hide = makeKeyButton(QStringLiteral("收起"), KeyboardTheme::compactActionSize());
    QPushButton *backspace = makeKeyButton(QStringLiteral("退格"), KeyboardTheme::actionSize());
    thirdRow->addWidget(shift);

    const QString lastRowLetters = QStringLiteral("ZXCVBNM");
    for (const QChar &letter : lastRowLetters)
    {
        QPushButton *button = makeKeyButton(QString(letter), KeyboardTheme::letterKeySize());
        connect(button, &QPushButton::clicked, this, [this, button]() {
            const QString text = m_uppercase ? button->text().toUpper() : button->text().toLower();
            emit keyPressed(Qt::Key_unknown, text, Qt::NoModifier);
            if (m_uppercase)
            {
                setUppercase(false);
            }
        });
        thirdRow->addWidget(button);
    }

    thirdRow->addWidget(backspace);
    thirdRow->addWidget(hide);
    root->addLayout(thirdRow);

    QHBoxLayout *bottomRow = new QHBoxLayout;
    bottomRow->setSpacing(KeyboardTheme::kGap);
    m_languageButton = makeKeyButton(QStringLiteral("中/英"), QSize(84, 44));
    QPushButton *number = makeKeyButton(QStringLiteral("123"), QSize(84, 44));
    m_urlSwitchButton = makeKeyButton(QStringLiteral("URL"), QSize(84, 44));
    QPushButton *symbol = makeKeyButton(QStringLiteral("#+="), QSize(84, 44));
    QPushButton *space = makeKeyButton(QStringLiteral("空格"), QSize(220, 44));
    QPushButton *dot = makeKeyButton(QStringLiteral("."), QSize(60, 44));
    QPushButton *enter = makeKeyButton(QStringLiteral("完成"), QSize(92, 44));
    bottomRow->addWidget(m_languageButton);
    bottomRow->addWidget(number);
    bottomRow->addWidget(m_urlSwitchButton);
    bottomRow->addWidget(symbol);
    bottomRow->addWidget(space);
    bottomRow->addWidget(dot);
    bottomRow->addWidget(enter);
    root->addLayout(bottomRow);

    connect(m_languageButton, &QPushButton::clicked, this, &NormalKeyboard::toggleLanguage);
    connect(number, &QPushButton::clicked, this, &NormalKeyboard::switchToNumeric);
    connect(m_urlSwitchButton, &QPushButton::clicked, this, &NormalKeyboard::switchToUrl);
    connect(shift, &QPushButton::clicked, this, [this]() { setUppercase(!m_uppercase); });
    connect(symbol, &QPushButton::clicked, this, &NormalKeyboard::changeSymbol);
    connect(hide, &QPushButton::clicked, this, &NormalKeyboard::hideKeyboard);
    connect(backspace, &QPushButton::clicked, this, [this]() {
        emit keyPressed(Qt::Key_Backspace, QString(), Qt::NoModifier);
    });
    connect(space, &QPushButton::clicked, this, [this]() {
        emit keyPressed(Qt::Key_Space, QStringLiteral(" "), Qt::NoModifier);
    });
    connect(dot, &QPushButton::clicked, this, [this]() {
        emit keyPressed(Qt::Key_Period, QStringLiteral("."), Qt::NoModifier);
    });
    connect(enter, &QPushButton::clicked, this, [this]() {
        emit keyPressed(Qt::Key_Return, QString(), Qt::NoModifier);
    });
}

void NormalKeyboard::setLanguageName(const QString &name)
{
    if (m_languageButton)
    {
        m_languageButton->setText(name);
    }
}

void NormalKeyboard::setUrlSwitchVisible(bool visible)
{
    if (m_urlSwitchButton)
    {
        m_urlSwitchButton->setVisible(visible);
    }
}

void NormalKeyboard::addLetterRow(QVBoxLayout *layout, const QString &letters)
{
    QHBoxLayout *row = new QHBoxLayout;
    row->setSpacing(KeyboardTheme::kGap);
    for (const QChar &letter : letters)
    {
        QPushButton *button = makeKeyButton(QString(letter), KeyboardTheme::letterKeySize());
        connect(button, &QPushButton::clicked, this, [this, button]() {
            const QString text = m_uppercase ? button->text().toUpper() : button->text().toLower();
            emit keyPressed(Qt::Key_unknown, text, Qt::NoModifier);
            if (m_uppercase)
            {
                setUppercase(false);
            }
        });
        row->addWidget(button);
    }
    layout->addLayout(row);
}

void NormalKeyboard::setUppercase(bool enabled)
{
    m_uppercase = enabled;
}
