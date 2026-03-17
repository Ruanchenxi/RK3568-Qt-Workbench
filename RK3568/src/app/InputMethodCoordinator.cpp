/**
 * @file InputMethodCoordinator.cpp
 * @brief 输入法协调层实现
 */

#include "app/InputMethodCoordinator.h"
#include "app/IInputMethodProvider.h"
#include "app/QtVirtualKeyboardProvider.h"

#include <QEvent>
#include <QGuiApplication>
#include <QInputMethod>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QSettings>
#include <QStringList>
#include <QTextEdit>
#include <QTimer>

namespace
{
QString normalizedProviderName(const QString &raw)
{
    const QString value = raw.trimmed().toLower();
    if (value == QLatin1String("qtvk") || value == QLatin1String("qtvirtualkeyboard"))
    {
        return QStringLiteral("qtvirtualkeyboard");
    }
    if (value == QLatin1String("none")
        || value == QLatin1String("off")
        || value == QLatin1String("disabled"))
    {
        return QStringLiteral("none");
    }
    return value;
}

class SoftKeyboardAutoShowFilter : public QObject
{
public:
    explicit SoftKeyboardAutoShowFilter(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (!watched || !event)
        {
            return QObject::eventFilter(watched, event);
        }

        const bool supportedInput =
            qobject_cast<QLineEdit *>(watched)
            || qobject_cast<QTextEdit *>(watched)
            || qobject_cast<QPlainTextEdit *>(watched);
        if (!supportedInput)
        {
            return QObject::eventFilter(watched, event);
        }

        switch (event->type())
        {
        case QEvent::FocusIn:
        case QEvent::MouseButtonRelease:
        case QEvent::TouchEnd:
            QTimer::singleShot(0, []() { InputMethodCoordinator::showInputMethod(); });
            break;
        default:
            break;
        }

        return QObject::eventFilter(watched, event);
    }
};

class NoopInputMethodProvider : public IInputMethodProvider
{
public:
    QString name() const override
    {
        return QStringLiteral("none");
    }

    void applyStartupEnvironment(const QSettings &) override
    {
    }

    QWidget *createHostWidget(QWidget *) override
    {
        return nullptr;
    }

    void show() override
    {
    }

    void hide() override
    {
    }

    bool isVisible() const override
    {
        return false;
    }

    void blockWidgetTree(QWidget *) override
    {
    }
};

IInputMethodProvider *providerForName(const QString &name)
{
    static NoopInputMethodProvider noopProvider;
    static QtVirtualKeyboardProvider qtVirtualKeyboardProvider;

    if (name == QLatin1String("qtvirtualkeyboard"))
    {
        return &qtVirtualKeyboardProvider;
    }
    return &noopProvider;
}

SoftKeyboardAutoShowFilter *sharedAutoShowFilter()
{
    static SoftKeyboardAutoShowFilter filter;
    return &filter;
}
}

void InputMethodCoordinator::applyStartupEnvironment()
{
    QSettings settings(QStringLiteral("RK3568"), QStringLiteral("DigitalPowerSystem"));
    providerForName(resolveProviderName())->applyStartupEnvironment(settings);
}

QString InputMethodCoordinator::configuredProviderName()
{
    return providerForName(resolveProviderName())->name();
}

bool InputMethodCoordinator::isQtVirtualKeyboardEnabled()
{
    return configuredProviderName() == QLatin1String("qtvirtualkeyboard");
}

QWidget *InputMethodCoordinator::createEmbeddedHost(QWidget *parent)
{
    return providerForName(resolveProviderName())->createHostWidget(parent);
}

bool InputMethodCoordinator::pageAllowsVirtualKeyboard(const QString &pageKey)
{
    const QStringList enabledPages =
        readSettingString(QStringLiteral("input/softKeyboardEnabledPages"),
                          QStringLiteral("login,system"))
            .split(',', Qt::SkipEmptyParts);

    for (const QString &item : enabledPages)
    {
        if (item.trimmed().compare(pageKey, Qt::CaseInsensitive) == 0)
        {
            return true;
        }
    }
    return false;
}

void InputMethodCoordinator::installAutoShowHooks(QWidget *root)
{
    if (!root || !isQtVirtualKeyboardEnabled())
    {
        return;
    }

    const QList<QLineEdit *> lineEdits = root->findChildren<QLineEdit *>();
    for (QLineEdit *edit : lineEdits)
    {
        if (edit)
        {
            edit->installEventFilter(sharedAutoShowFilter());
        }
    }

    const QList<QTextEdit *> textEdits = root->findChildren<QTextEdit *>();
    for (QTextEdit *edit : textEdits)
    {
        if (edit)
        {
            edit->installEventFilter(sharedAutoShowFilter());
        }
    }

    const QList<QPlainTextEdit *> plainTextEdits = root->findChildren<QPlainTextEdit *>();
    for (QPlainTextEdit *edit : plainTextEdits)
    {
        if (edit)
        {
            edit->installEventFilter(sharedAutoShowFilter());
        }
    }
}

bool InputMethodCoordinator::isInputMethodVisible()
{
    return providerForName(resolveProviderName())->isVisible();
}

void InputMethodCoordinator::showInputMethod()
{
    if (!isQtVirtualKeyboardEnabled())
    {
        return;
    }
    providerForName(resolveProviderName())->show();
}

void InputMethodCoordinator::hideInputMethod()
{
    providerForName(resolveProviderName())->hide();
}

void InputMethodCoordinator::blockSoftKeyboardForWidgetTree(QWidget *root)
{
    providerForName(resolveProviderName())->blockWidgetTree(root);
}

QString InputMethodCoordinator::resolveProviderName()
{
    const QString envProvider =
        normalizedProviderName(qEnvironmentVariable("RK3568_SOFT_KEYBOARD_PROVIDER"));
    if (!envProvider.isEmpty())
    {
        return envProvider;
    }

    QSettings settings(QStringLiteral("RK3568"), QStringLiteral("DigitalPowerSystem"));
    if (!settings.contains(QStringLiteral("input/softKeyboardProviderMigratedV1EnableQtVk")))
    {
        const QString current =
            normalizedProviderName(settings.value(QStringLiteral("input/softKeyboardProvider"))
                                       .toString());
        if (current.isEmpty() || current == QLatin1String("none"))
        {
            settings.setValue(QStringLiteral("input/softKeyboardProvider"),
                              QStringLiteral("qtvirtualkeyboard"));
        }
        settings.setValue(QStringLiteral("input/softKeyboardProviderMigratedV1EnableQtVk"),
                          true);
    }

    return normalizedProviderName(
        settings.value(QStringLiteral("input/softKeyboardProvider"),
#ifdef Q_OS_LINUX
                          QStringLiteral("qtvirtualkeyboard")
#else
                          QStringLiteral("qtvirtualkeyboard")
#endif
                          )
            .toString());
}

QString InputMethodCoordinator::readSettingString(const QString &key, const QString &defaultValue)
{
    QSettings settings(QStringLiteral("RK3568"), QStringLiteral("DigitalPowerSystem"));
    return settings.value(key, defaultValue).toString().trimmed();
}
