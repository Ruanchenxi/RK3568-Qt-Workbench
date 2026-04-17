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

    // QWebEngine/Chromium 在首次接收触摸后会通过 XI2 XIGrabDevice 抢占触摸设备，
    // 导致 Qt 后续收不到任何触摸事件（触摸"冻结"）。
    // 禁用 Qt XCB 插件的 XI2 支持后，Qt 改用核心 X11 指针事件；
    // 核心 X11 事件不受 XI2 设备 grab 影响，触摸屏在工作台标签激活时仍可正常响应。
    if (!qEnvironmentVariableIsSet("QT_XCB_NO_XI2")) {
        qputenv("QT_XCB_NO_XI2", "1");
    }

    // Rockchip Mali GPU 驱动与 Chromium GPU 进程不兼容：GPU 进程会使用无效 X11 窗口句柄，
    // 触发 XCB BadWindow 错误导致 zygote 崩溃，后续所有渲染进程启动失败（工作台白屏）。
    // 强制软件渲染规避此问题；仅在外部未覆盖时写入，以保留调试灵活性。
    if (!qEnvironmentVariableIsSet("QT_OPENGL")) {
        qputenv("QT_OPENGL", "software");
    }
    if (!qEnvironmentVariableIsSet("QTWEBENGINE_CHROMIUM_FLAGS")) {
        qputenv("QTWEBENGINE_CHROMIUM_FLAGS",
                "--no-sandbox --disable-gpu --disable-gpu-compositing "
                "--disable-es3-gl-context --ignore-gpu-blocklist "
                "--enable-accelerated-video-decode=false "
                "--single-process --no-zygote-sandbox --disable-setuid-sandbox");
    }

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
            << "AA_UseOpenGLES=" << (forceEglGles ? "true" : "false")
            << "QT_XCB_NO_XI2=" << qEnvironmentVariable("QT_XCB_NO_XI2", "<unset>");
}
#endif
}

int main(int argc, char *argv[]) // C++ 标准入口函数
{
#ifdef Q_OS_LINUX
    configureLinuxBoardGraphics();
#endif

    // QT_XCB_NO_XI2=1 已禁用 XI2，Qt 直接用核心 X11 鼠标事件，无 Qt 触摸事件产生。
    // 保留此属性以兼容未来可能切回 XI2 的场景，当前实际不产生效果。
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
