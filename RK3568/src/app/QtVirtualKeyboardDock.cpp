/**
 * @file QtVirtualKeyboardDock.cpp
 * @brief Qt Virtual Keyboard 应用内底部宿主实现
 */

#include "app/QtVirtualKeyboardDock.h"

#include <QColor>
#include <QEvent>
#include <QDebug>
#include <QQuickItem>
#include <QQuickWidget>
#include <QQmlError>
#include <QVBoxLayout>

QtVirtualKeyboardDock::QtVirtualKeyboardDock(QWidget *parent)
    : QWidget(parent),
      m_quickWidget(new QQuickWidget(this))
{
    setObjectName(QStringLiteral("qtVirtualKeyboardDock"));
    setVisible(false);
    setAttribute(Qt::WA_StyledBackground, true);
    setFixedHeight(0);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setSpacing(0);
    layout->setContentsMargins(0, 0, 0, 0);

    m_quickWidget->setResizeMode(QQuickWidget::SizeRootObjectToView);
    m_quickWidget->setClearColor(QColor(0, 0, 0, 0));
    m_quickWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_quickWidget->setFocusPolicy(Qt::NoFocus);
    m_quickWidget->setFixedHeight(0);
    connect(m_quickWidget, &QQuickWidget::statusChanged, this, [this](QQuickWidget::Status status) {
        if (status == QQuickWidget::Error)
        {
            const QList<QQmlError> errors = m_quickWidget->errors();
            for (const QQmlError &error : errors)
            {
                qWarning().noquote() << "[qtvk-dock]" << error.toString();
            }
        }
        else
        {
            qInfo() << "[qtvk-dock] statusChanged:" << status;
        }
    });
    m_quickWidget->setSource(QUrl(QStringLiteral("qrc:/qtvk/InputPanelHost.qml")));
    layout->addWidget(m_quickWidget);

    if (parent)
    {
        parent->installEventFilter(this);
    }

    if (QQuickItem *root = m_quickWidget->rootObject())
    {
        connect(root, SIGNAL(panelHeightChanged()), this, SLOT(syncPanelHeight()));
        connect(root, SIGNAL(keyboardVisibleChanged()), this, SLOT(syncPanelHeight()));
        connect(root, SIGNAL(manualEnabledChanged()), this, SLOT(syncPanelHeight()));
        syncPanelHeight();
    }
    else
    {
        qWarning() << "[qtvk-dock] rootObject is null";
    }
}

bool QtVirtualKeyboardDock::isPanelVisible() const
{
    if (QQuickItem *root = m_quickWidget->rootObject())
    {
        return root->property("keyboardVisible").toBool();
    }
    return false;
}

void QtVirtualKeyboardDock::setManualEnabled(bool enabled)
{
    if (QQuickItem *root = m_quickWidget->rootObject())
    {
        qInfo() << "[qtvk-dock] setManualEnabled =" << enabled;
        root->setProperty("manualEnabled", enabled);
        syncPanelHeight();
    }
}

bool QtVirtualKeyboardDock::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == parentWidget() && event && event->type() == QEvent::Resize)
    {
        updateOverlayGeometry();
    }
    return QWidget::eventFilter(watched, event);
}

void QtVirtualKeyboardDock::syncPanelHeight()
{
    int height = 0;
    bool keyboardVisible = false;
    if (QQuickItem *root = m_quickWidget->rootObject())
    {
        height = root->property("panelHeight").toInt();
        keyboardVisible = root->property("keyboardVisible").toBool();
    }

    qInfo() << "[qtvk-dock] syncPanelHeight"
            << "visible=" << keyboardVisible
            << "height=" << height;

    setFixedHeight(height);
    m_quickWidget->setFixedHeight(height);
    setVisible(keyboardVisible && height > 0);
    updateOverlayGeometry();
}

void QtVirtualKeyboardDock::updateOverlayGeometry()
{
    if (!parentWidget())
    {
        return;
    }

    const int height = this->height();
    const int width = parentWidget()->width();
    const int y = parentWidget()->height() - height;
    setGeometry(0, y, width, height);
    raise();
}
