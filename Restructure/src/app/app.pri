# app module

# 旧 Qt Virtual Keyboard 相关代码仅保留为历史兼容壳，不纳入当前主构建。
# 当前主线只装配主窗口与登录态控制，不再把旧输入法协调器、provider、dock 或 qml 资源并回工程。

SOURCES += \
    $$PWD/AppComposition.cpp \
    $$PWD/main.cpp \
    $$PWD/mainwindow.cpp \
    $$PWD/MainWindowController.cpp \
    $$PWD/ProductProfile.cpp

HEADERS += \
    $$PWD/AppComposition.h \
    $$PWD/UiLayoutProfileResolver.h \
    $$PWD/mainwindow.h \
    $$PWD/MainWindowController.h \
    $$PWD/ProductProfile.h
