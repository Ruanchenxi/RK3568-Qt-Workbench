QT += core gui widgets serialport

CONFIG += c++11

TARGET   = serial_test
TEMPLATE = app

# MSVC: treat source files as UTF-8 (same as -finput-charset=utf-8 on GCC)
msvc:QMAKE_CXXFLAGS += /utf-8

SOURCES += \
    main.cpp \
    SerialTestWidget.cpp

HEADERS += \
    SerialTestWidget.h
