/**
 * @file UrlKeyboard.cpp
 * @brief URL 专用键盘实现
 */

#include "features/keyboard/ui/UrlKeyboard.h"
#include "features/keyboard/ui/KeyboardTheme.h"

#include <QHBoxLayout>
#include <QPushButton>
#include <QVBoxLayout>

namespace
{
QPushButton *makeUrlButton(const QString &text, const QSize &size = QSize(0, 0))
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

UrlKeyboard::UrlKeyboard(QWidget *parent)
    : QWidget(parent)
{
    buildUi();
}

void UrlKeyboard::buildUi()
{
    QVBoxLayout *root = new QVBoxLayout(this);
    root->setSpacing(KeyboardTheme::kGap);
    root->setContentsMargins(8, 8, 8, 8);

    const QList<QStringList> rows = {
        {QStringLiteral("https://"), QStringLiteral("http://"), QStringLiteral("localhost"), QStringLiteral("/api")},
        {QStringLiteral("127.0.0.1"), QStringLiteral(":"), QStringLiteral("/"), QStringLiteral(".")},
        {QStringLiteral("-"), QStringLiteral("_"), QStringLiteral("?"), QStringLiteral("&")}
    };

    for (const QStringList &rowValues : rows)
    {
        QHBoxLayout *row = new QHBoxLayout;
        row->setSpacing(KeyboardTheme::kGap);
        for (const QString &value : rowValues)
        {
            const int width = value.length() > 4 ? 118 : 78;
            QPushButton *button = makeUrlButton(value, QSize(width, 40));
            connect(button, &QPushButton::clicked, this, [this, value]() {
                emit keyPressed(Qt::Key_unknown, value, Qt::NoModifier);
            });
            row->addWidget(button);
        }
        root->addLayout(row);
    }

    QHBoxLayout *bottom = new QHBoxLayout;
    bottom->setSpacing(KeyboardTheme::kGap);
    QPushButton *normal = makeUrlButton(QStringLiteral("ABC"), QSize(76, 42));
    QPushButton *numeric = makeUrlButton(QStringLiteral("123"), QSize(76, 42));
    QPushButton *clear = makeUrlButton(QStringLiteral("清空"), QSize(88, 42));
    QPushButton *backspace = makeUrlButton(QStringLiteral("退格"), QSize(88, 42));
    QPushButton *at = makeUrlButton(QStringLiteral("@"), QSize(60, 42));
    QPushButton *enter = makeUrlButton(QStringLiteral("完成"), QSize(88, 42));
    QPushButton *hide = makeUrlButton(QStringLiteral("收起"), QSize(88, 42));
    bottom->addWidget(normal);
    bottom->addWidget(numeric);
    bottom->addWidget(clear);
    bottom->addWidget(backspace);
    bottom->addWidget(at);
    bottom->addWidget(enter);
    bottom->addWidget(hide);
    root->addLayout(bottom);

    connect(normal, &QPushButton::clicked, this, &UrlKeyboard::switchToNormal);
    connect(numeric, &QPushButton::clicked, this, &UrlKeyboard::switchToNumeric);
    connect(clear, &QPushButton::clicked, this, [this]() {
        emit keyPressed(Qt::Key_unknown, QStringLiteral("\x01CLEAR"), Qt::NoModifier);
    });
    connect(backspace, &QPushButton::clicked, this, [this]() {
        emit keyPressed(Qt::Key_Backspace, QString(), Qt::NoModifier);
    });
    connect(at, &QPushButton::clicked, this, [this]() {
        emit keyPressed(Qt::Key_At, QStringLiteral("@"), Qt::NoModifier);
    });
    connect(enter, &QPushButton::clicked, this, [this]() {
        emit keyPressed(Qt::Key_Return, QString(), Qt::NoModifier);
    });
    connect(hide, &QPushButton::clicked, this, &UrlKeyboard::hideKeyboard);
}
