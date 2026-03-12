# platform module

SOURCES += \
    $$PWD/serial/QtSerialTransport.cpp \
    $$PWD/serial/ReplaySerialTransport.cpp \
    $$PWD/process/ProcessService.cpp \
    $$PWD/logging/LogService.cpp

HEADERS += \
    $$PWD/serial/ISerialTransport.h \
    $$PWD/serial/QtSerialTransport.h \
    $$PWD/serial/ReplaySerialTransport.h \
    $$PWD/process/ProcessService.h \
    $$PWD/logging/LogService.h
