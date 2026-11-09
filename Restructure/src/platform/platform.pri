# platform module

SOURCES += \
    $$PWD/serial/QtSerialTransport.cpp \
    $$PWD/process/ServiceStartupPolicy.cpp \
    $$PWD/process/ProcessService.cpp \
    $$PWD/logging/LogService.cpp \
    $$PWD/logging/BusinessLogService.cpp \
    $$PWD/runtime/ApplicationSessionTracker.cpp

HEADERS += \
    $$PWD/serial/ISerialTransport.h \
    $$PWD/serial/QtSerialTransport.h \
    $$PWD/process/ServiceStartupPolicy.h \
    $$PWD/process/ProcessService.h \
    $$PWD/logging/LogService.h \
    $$PWD/logging/BusinessLogService.h \
    $$PWD/runtime/ApplicationSessionTracker.h
