/**
 * @file KeyboardModeResolver.cpp
 * @brief 根据输入目标解析键盘模式
 */

#include "features/keyboard/application/KeyboardModeResolver.h"

#include "features/keyboard/application/KeyboardTargetAdapter.h"

#include <QComboBox>
#include <QLineEdit>
#include <QString>

keyboard::KeyboardMode KeyboardModeResolver::resolve(QLineEdit *target)
{
    if (!target)
    {
        return keyboard::KeyboardMode::Normal;
    }

    QString objectName = target->objectName().trimmed().toLower();
    if (objectName.isEmpty())
    {
        if (QComboBox *combo = KeyboardTargetAdapter::comboBoxFrom(target))
        {
            objectName = combo->objectName().trimmed().toLower();
        }
    }
    if (objectName.contains(QStringLiteral("url")) || objectName.contains(QStringLiteral("api")))
    {
        return keyboard::KeyboardMode::Url;
    }
    if (objectName.contains(QStringLiteral("station"))
        || objectName.contains(QStringLiteral("port"))
        || objectName.contains(QStringLiteral("tenant")))
    {
        return keyboard::KeyboardMode::Numeric;
    }
    return keyboard::KeyboardMode::Normal;
}

bool KeyboardModeResolver::allowUrlSwitch(QLineEdit *target)
{
    return resolve(target) == keyboard::KeyboardMode::Url;
}

QString KeyboardModeResolver::displayName(QLineEdit *target)
{
    if (!target)
    {
        return QStringLiteral("未选择输入框");
    }

    QString objectName = target->objectName().trimmed().toLower();
    if (objectName.isEmpty())
    {
        if (QComboBox *combo = KeyboardTargetAdapter::comboBoxFrom(target))
        {
            objectName = combo->objectName().trimmed().toLower();
        }
    }

    if (objectName.contains(QStringLiteral("username")))
        return QStringLiteral("用户名");
    if (objectName.contains(QStringLiteral("password")))
        return QStringLiteral("密码");
    if (objectName.contains(QStringLiteral("homeurl")))
        return QStringLiteral("首页地址");
    if (objectName.contains(QStringLiteral("apiurl")))
        return QStringLiteral("接口地址");
    if (objectName.contains(QStringLiteral("station")))
        return QStringLiteral("当前站号");
    if (objectName.contains(QStringLiteral("tenant")))
        return QStringLiteral("租户编码");
    if (objectName.contains(QStringLiteral("keyserial")))
        return QStringLiteral("钥匙串口");
    if (objectName.contains(QStringLiteral("cardserial")))
        return QStringLiteral("读卡串口");

    return QStringLiteral("当前输入");
}

QString KeyboardModeResolver::modeName(keyboard::KeyboardMode mode)
{
    switch (mode)
    {
    case keyboard::KeyboardMode::Numeric:
        return QStringLiteral("数字键盘");
    case keyboard::KeyboardMode::Url:
        return QStringLiteral("URL 键盘");
    case keyboard::KeyboardMode::Symbol:
        return QStringLiteral("符号键盘");
    case keyboard::KeyboardMode::Normal:
    default:
        return QStringLiteral("全字符键盘");
    }
}
