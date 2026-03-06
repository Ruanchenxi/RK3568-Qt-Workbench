QT += core network widgets
# QT -= gui  # Step2: 需要 GUI

CONFIG += c++11
# CONFIG -= app_bundle  # Step2: 需要窗口

TARGET = http_ticket_server
TEMPLATE = app

SOURCES += main.cpp \
           TicketHttpServer.cpp \
           LogWindow.cpp \
           TicketFrameBuilder.cpp \
           TicketPayloadEncoder.cpp \
           ReplayRunner.cpp \
           ReplayPanel.cpp \
           GoldenPanel.cpp

HEADERS += TicketHttpServer.h \
           LogWindow.h \
           TicketFrameBuilder.h \
           TicketPayloadEncoder.h \
           ReplayRunner.h \
           ReplayPanel.h \
           GoldenPanel.h
