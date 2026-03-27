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
win32:QMAKE_CXXFLAGS += /utf-8


# ========== 模块化文件清单（按目录收口） ==========
include(src/app/app.pri)
include(src/core/core.pri)
include(src/features/features.pri)
include(src/platform/platform.pri)
include(src/shared/shared.pri)

# ========== 界面文件 ==========
# 当前界面清单仅保留正式主线页面。
# 历史 dialog 键盘 `ui/keyboarddialog.ui` 已正式删除，不再参与工程构建。
FORMS += \
        ui/mainwindow.ui \
        ui/loginpage.ui \
        ui/workbenchpage.ui \
        ui/keymanagepage.ui \
        ui/systempage.ui \
        ui/logpage.ui

# ========== 包含路径 ==========
INCLUDEPATH += src \
               src/app \
               src/core \
               src/features \
               src/platform \
               src/platform/serial \
               src/features/key/protocol

RESOURCES += \
        resources/branding.qrc \
        resources/keyboard_dict.qrc

