/**
 * @file KeyboardController.h
 * @brief 键盘控制器：页面与键盘宿主之间的协调层
 */

#ifndef KEYBOARDCONTROLLER_H
#define KEYBOARDCONTROLLER_H

#include <QObject>
#include <QPointer>

class QLineEdit;
class QWidget;
class KeyboardContainer;

class KeyboardController : public QObject
{
    Q_OBJECT

public:
    explicit KeyboardController(QObject *parent = nullptr);

    void attachContainer(KeyboardContainer *container);
    void registerInputScope(QWidget *scope);
    bool hasTargetOnPage(QWidget *page) const;
    bool isVisible() const;
    void toggleOnPage(QWidget *page);
    void hide();
    void clearCurrentTarget();

signals:
    void targetChanged(QWidget *target);
    void visibilityChanged(bool visible, int height);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void syncVisibleMode();

    QPointer<QLineEdit> m_currentTarget;
    KeyboardContainer *m_container;
};

#endif // KEYBOARDCONTROLLER_H
