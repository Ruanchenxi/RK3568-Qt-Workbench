#include "app/mainwindow.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDebug>

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

    QApplication a(argc, argv); // 创建应用对象，负责管理应用级资源和主事件循环

    MainWindow w; // 创建主窗口
    w.show();     // 显示主窗口到屏幕上

    return a.exec(); // 进入事件循环，程序在这里等待用户操作
                     // 只有当用户关闭窗口时，这个函数才会返回
}
