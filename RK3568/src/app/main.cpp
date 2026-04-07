#include "app/mainwindow.h"

#include <QApplication>
#include <QCoreApplication>
#include <QIcon>

namespace
{
#ifdef Q_OS_LINUX
QString linuxGraphicsMode()
{
    return qEnvironmentVariable("RK3568_LINUX_GL_MODE", QStringLiteral("auto"))
            .trimmed()
            .toLower();
}

void configureLinuxBoardGraphics()
{
    qputenv("QT_AUTO_SCREEN_SCALE_FACTOR", "0");
    qputenv("QT_SCALE_FACTOR", "1");
    qputenv("QT_ENABLE_HIGHDPI_SCALING", "0");

    const QString graphicsMode = linuxGraphicsMode();
    const bool forceEglGles = (graphicsMode == QLatin1String("egl")
                               || graphicsMode == QLatin1String("gles")
                               || graphicsMode == QLatin1String("xcb_egl_gles"));
    const bool forceSoftware = (graphicsMode == QLatin1String("software"));

    QApplication::setAttribute(Qt::AA_DisableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_Use96Dpi);

    if (forceEglGles) {
        if (!qEnvironmentVariableIsSet("QT_XCB_GL_INTEGRATION")) {
            qputenv("QT_XCB_GL_INTEGRATION", "xcb_egl");
        }
        QCoreApplication::setAttribute(Qt::AA_UseOpenGLES);
    }

    if (forceSoftware && !qEnvironmentVariableIsSet("QT_OPENGL")) {
        qputenv("QT_OPENGL", "software");
    }

    qInfo().noquote()
            << "[linux-graphics]"
            << "RK3568_LINUX_GL_MODE=" << graphicsMode
            << "QT_QPA_PLATFORM=" << qEnvironmentVariable("QT_QPA_PLATFORM", "<unset>")
            << "QT_XCB_GL_INTEGRATION=" << qEnvironmentVariable("QT_XCB_GL_INTEGRATION", "<unset>")
            << "QT_OPENGL=" << qEnvironmentVariable("QT_OPENGL", "<unset>")
            << "QTWEBENGINE_CHROMIUM_FLAGS=" << qEnvironmentVariable("QTWEBENGINE_CHROMIUM_FLAGS", "<unset>")
            << "AA_UseOpenGLES=" << (forceEglGles ? "true" : "false");
}
#endif
}

int main(int argc, char *argv[]) // C++ 标准入口函数
{
#ifdef Q_OS_LINUX
    configureLinuxBoardGraphics();
#endif

    // 板端实体屏主要依赖 direct touch；显式开启“未处理触摸合成为鼠标”
    // 可以让普通 QWidget/QPushButton 在 xcb/X11 下继续按点击语义工作。
    QCoreApplication::setAttribute(Qt::AA_SynthesizeMouseForUnhandledTouchEvents, true);
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);

    QApplication a(argc, argv); // 创建应用对象，负责管理应用级资源和主事件循环
    const QIcon appIcon(QStringLiteral(":/branding/window_icon.png"));
    if (!appIcon.isNull())
    {
        a.setWindowIcon(appIcon);
    }

    qInfo().noquote()
            << "[input-touch]"
            << "AA_SynthesizeMouseForUnhandledTouchEvents=true";

    QObject::connect(&a, &QGuiApplication::lastWindowClosed, []() {
        qInfo() << "[app-lifecycle] lastWindowClosed emitted";
    });
    QObject::connect(&a, &QCoreApplication::aboutToQuit, []() {
        qInfo() << "[app-lifecycle] aboutToQuit emitted";
    });

    MainWindow w; // 创建主窗口
    if (!appIcon.isNull())
    {
        w.setWindowIcon(appIcon);
    }
    w.show();     // 显示主窗口到屏幕上

    return a.exec(); // 进入事件循环，程序在这里等待用户操作
                     // 只有当用户关闭窗口时，这个函数才会返回
}
