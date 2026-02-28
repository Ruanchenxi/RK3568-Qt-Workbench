#-------------------------------------------------
#
# Project created by QtCreator 2026-01-28T10:33:04
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

# 串口通讯支持（如果没有此模块，SerialService 会使用模拟模式）
QT += serialport

# 网络模块支持（用于端口检查）
QT += network

# WebEngine 模块支持（用于内嵌网页）
QT += webenginewidgets

TARGET = RK3568       # 生成的可执行文件名
TEMPLATE = app             # 项目类型：应用程序

# The following define makes your compiler emit warnings if you use
# any feature of Qt which has been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

# MSVC 编译器选项：支持 UTF-8 编码源文件
QMAKE_CXXFLAGS += /utf-8


# ========== 源代码文件 ==========
SOURCES += \
        src/main.cpp \
        src/mainwindow.cpp \
        src/loginpage.cpp \
        src/workbenchpage.cpp \
        src/keymanagepage.cpp \
        src/systempage.cpp \
        src/logpage.cpp \
        src/core/AppContext.cpp \
        src/core/ConfigManager.cpp \
        src/core/authservice.cpp \
        src/services/LogService.cpp \
        src/services/SerialService.cpp \
        src/protocol/KeyCabinetProtocol.cpp

# 临时：UI 对齐阶段先屏蔽 KeySerialClient.cpp 编译，避免串口链路历史不一致导致构建中断
# 后续恢复串口功能时请加回：
#       src/protocol/KeySerialClient.cpp

# ========== 头文件 ==========
HEADERS += \
        src/mainwindow.h \
        src/loginpage.h \
        src/workbenchpage.h \
        src/keymanagepage.h \
        src/systempage.h \
        src/logpage.h \
        src/core/AppContext.h \
        src/core/ConfigManager.h \
        src/core/authservice.h \
        src/services/LogService.h \
        src/services/SerialService.h \
        src/protocol/ProtocolBase.h \
        src/protocol/KeyCabinetProtocol.h \
        src/protocol/KeyProtocolDefs.h \
        src/protocol/LogItem.h \
        src/protocol/KeyCrc16.h

# 临时：UI 对齐阶段先屏蔽 KeySerialClient 的 moc 生成
# 后续恢复串口功能时请加回：
#       src/protocol/KeySerialClient.h

# ========== 界面文件 ==========
FORMS += \
        ui/mainwindow.ui \
        ui/loginpage.ui \
        ui/workbenchpage.ui \
        ui/keymanagepage.ui \
        ui/systempage.ui \
        ui/logpage.ui \
        ui/keyboarddialog.ui

# ========== 资源文件 ==========
RESOURCES += \
        resources/picture.qrc

# ========== 包含路径 ==========
INCLUDEPATH += src \
               src/core \
               src/services \
               src/protocol
