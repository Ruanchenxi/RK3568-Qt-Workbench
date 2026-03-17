/**
 * @file QtVirtualKeyboardDock.h
 * @brief Qt Virtual Keyboard 应用内底部宿主
 */

#ifndef QTVIRTUALKEYBOARDDOCK_H
#define QTVIRTUALKEYBOARDDOCK_H

#include <QWidget>

class QQuickWidget;

class QtVirtualKeyboardDock : public QWidget
{
    Q_OBJECT

public:
    explicit QtVirtualKeyboardDock(QWidget *parent = nullptr);
    void setManualEnabled(bool enabled);
    bool isPanelVisible() const;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void syncPanelHeight();

private:
    void updateOverlayGeometry();

    QQuickWidget *m_quickWidget;
};

#endif // QTVIRTUALKEYBOARDDOCK_H
