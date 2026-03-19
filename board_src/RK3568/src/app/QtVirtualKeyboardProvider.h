/**
 * @file QtVirtualKeyboardProvider.h
 * @brief Qt Virtual Keyboard 过渡 Provider
 */

#ifndef QTVIRTUALKEYBOARDPROVIDER_H
#define QTVIRTUALKEYBOARDPROVIDER_H

#include "app/IInputMethodProvider.h"

class QtVirtualKeyboardDock;

class QtVirtualKeyboardProvider : public IInputMethodProvider
{
public:
    QString name() const override;
    void applyStartupEnvironment(const QSettings &settings) override;
    QWidget *createHostWidget(QWidget *parent) override;
    void show() override;
    void hide() override;
    bool isVisible() const override;
    void blockWidgetTree(QWidget *root) override;

private:
    QtVirtualKeyboardDock *m_host = nullptr;
};

#endif // QTVIRTUALKEYBOARDPROVIDER_H
