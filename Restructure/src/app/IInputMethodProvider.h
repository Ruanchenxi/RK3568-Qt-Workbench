/**
 * @file IInputMethodProvider.h
 * @brief 输入法 Provider 抽象接口
 */

#ifndef IINPUTMETHODPROVIDER_H
#define IINPUTMETHODPROVIDER_H

#include <QString>

class QSettings;
class QWidget;

class IInputMethodProvider
{
public:
    virtual ~IInputMethodProvider() = default;

    virtual QString name() const = 0;
    virtual void applyStartupEnvironment(const QSettings &settings) = 0;
    virtual QWidget *createHostWidget(QWidget *parent) = 0;
    virtual void show() = 0;
    virtual void hide() = 0;
    virtual bool isVisible() const = 0;
    virtual void blockWidgetTree(QWidget *root) = 0;
};

#endif // IINPUTMETHODPROVIDER_H
