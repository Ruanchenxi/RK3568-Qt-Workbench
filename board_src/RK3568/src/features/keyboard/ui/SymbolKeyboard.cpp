/**
 * @file SymbolKeyboard.cpp
 * @brief 符号键盘实现
 */

#include "features/keyboard/ui/SymbolKeyboard.h"
#include "features/keyboard/ui/KeyboardTheme.h"

#include <QHBoxLayout>
#include <QPushButton>
#include <QVBoxLayout>

namespace
{
QPushButton *makeSymbolButton(const QString &text, const QSize &size = QSize(0, 0))
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

SymbolKeyboard::SymbolKeyboard(QWidget *parent)
    : QWidget(parent)
{
    buildUi();
}

void SymbolKeyboard::buildUi()
{
    QVBoxLayout *root = new QVBoxLayout(this);
    root->setSpacing(KeyboardTheme::kGap);
    root->setContentsMargins(8, 8, 8, 8);

    const QStringList rows = {
        QStringLiteral("1 2 3 4 5 6 7 8 9 0"),
        QStringLiteral("@ # % & * - + ( )"),
        QStringLiteral("! ? / . : ; _ =")
    };

    for (const QString &rowText : rows)
    {
        QHBoxLayout *row = new QHBoxLayout;
        row->setSpacing(KeyboardTheme::kGap);
        const QStringList keys = rowText.split(' ', Qt::SkipEmptyParts);
        for (const QString &text : keys)
        {
            QPushButton *button = makeSymbolButton(text, QSize(60, 40));
            connect(button, &QPushButton::clicked, this, [this, text]() {
                emit keyPressed(Qt::Key_unknown, text, Qt::NoModifier);
            });
            row->addWidget(button);
        }
        root->addLayout(row);
    }

    QHBoxLayout *bottomRow = new QHBoxLayout;
    bottomRow->setSpacing(KeyboardTheme::kGap);
    QPushButton *normal = makeSymbolButton(QStringLiteral("ABC"), QSize(88, 42));
    QPushButton *space = makeSymbolButton(QStringLiteral("空格"), QSize(280, 42));
    QPushButton *backspace = makeSymbolButton(QStringLiteral("退格"), QSize(96, 42));
    QPushButton *hide = makeSymbolButton(QStringLiteral("收起"), QSize(84, 42));
    bottomRow->addWidget(normal);
    bottomRow->addWidget(space);
    bottomRow->addWidget(backspace);
    bottomRow->addWidget(hide);
    root->addLayout(bottomRow);

    connect(normal, &QPushButton::clicked, this, &SymbolKeyboard::changeSymbol);
    connect(hide, &QPushButton::clicked, this, &SymbolKeyboard::hideKeyboard);
    connect(space, &QPushButton::clicked, this, [this]() {
        emit keyPressed(Qt::Key_Space, QStringLiteral(" "), Qt::NoModifier);
    });
    connect(backspace, &QPushButton::clicked, this, [this]() {
        emit keyPressed(Qt::Key_Backspace, QString(), Qt::NoModifier);
    });
}
