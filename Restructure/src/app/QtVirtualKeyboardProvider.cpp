/**
 * @file QtVirtualKeyboardProvider.cpp
 * @brief Qt Virtual Keyboard 过渡 Provider 实现
 */

#include "app/QtVirtualKeyboardProvider.h"
#include "app/QtVirtualKeyboardDock.h"

#include <QDebug>
#include <QGuiApplication>
#include <QInputMethod>
#include <QSettings>
#include <QWidget>

QString QtVirtualKeyboardProvider::name() const
{
    return QStringLiteral("qtvirtualkeyboard");
}

void QtVirtualKeyboardProvider::applyStartupEnvironment(const QSettings &settings)
{
    if (!qEnvironmentVariableIsSet("QT_IM_MODULE"))
    {
        qputenv("QT_IM_MODULE", QByteArrayLiteral("qtvirtualkeyboard"));
    }
    if (!qEnvironmentVariableIsSet("QT_VIRTUALKEYBOARD_DESKTOP_DISABLE"))
    {
        qputenv("QT_VIRTUALKEYBOARD_DESKTOP_DISABLE", QByteArrayLiteral("1"));
    }

    const QString configuredStyle =
        settings.value(QStringLiteral("input/qtVirtualKeyboardStyle")).toString().trimmed();
    if (!configuredStyle.isEmpty() && !qEnvironmentVariableIsSet("QT_VIRTUALKEYBOARD_STYLE"))
    {
        qputenv("QT_VIRTUALKEYBOARD_STYLE", configuredStyle.toUtf8());
    }
}

QWidget *QtVirtualKeyboardProvider::createHostWidget(QWidget *parent)
{
    m_host = new QtVirtualKeyboardDock(parent);
    return m_host;
}

void QtVirtualKeyboardProvider::show()
{
    qInfo() << "[qtvk-provider] show requested";
    if (m_host)
    {
        m_host->setManualEnabled(true);
    }
    if (QGuiApplication::instance() && QGuiApplication::inputMethod())
    {
        QGuiApplication::inputMethod()->show();
    }
}

void QtVirtualKeyboardProvider::hide()
{
    qInfo() << "[qtvk-provider] hide requested";
    if (m_host)
    {
        m_host->setManualEnabled(false);
    }
    if (QGuiApplication::instance() && QGuiApplication::inputMethod())
    {
        QGuiApplication::inputMethod()->hide();
    }
}

bool QtVirtualKeyboardProvider::isVisible() const
{
    return m_host && m_host->isPanelVisible();
}

void QtVirtualKeyboardProvider::blockWidgetTree(QWidget *root)
{
    if (!root)
    {
        return;
    }

    root->setAttribute(Qt::WA_InputMethodEnabled, false);
    const QList<QWidget *> children = root->findChildren<QWidget *>();
    for (QWidget *child : children)
    {
        if (child)
        {
            child->setAttribute(Qt::WA_InputMethodEnabled, false);
        }
    }
}
