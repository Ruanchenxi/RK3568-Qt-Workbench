# features module

SOURCES += \
    $$PWD/auth/ui/loginpage.cpp \
    $$PWD/auth/application/LoginController.cpp \
    $$PWD/auth/application/AuthFlowCoordinator.cpp \
    $$PWD/auth/infra/password/authservice.cpp \
    $$PWD/auth/infra/password/AuthServiceAdapter.cpp \
    $$PWD/auth/infra/http/AuthGatewayAdapter.cpp \
    $$PWD/auth/infra/device/MockCredentialSource.cpp \
    $$PWD/auth/infra/device/CardSerialSource.cpp \
    $$PWD/auth/infra/device/FingerprintSource.cpp \
    $$PWD/workbench/ui/workbenchpage.cpp \
    $$PWD/workbench/application/WorkbenchController.cpp \
    $$PWD/key/ui/keymanagepage.cpp \
    $$PWD/key/application/KeySessionService.cpp \
    $$PWD/key/application/KeyManageController.cpp \
    $$PWD/key/application/SerialLogManager.cpp \
    $$PWD/key/application/TicketStore.cpp \
    $$PWD/key/application/TicketIngressService.cpp \
    $$PWD/key/protocol/TicketPayloadEncoder.cpp \
    $$PWD/key/protocol/TicketFrameBuilder.cpp \
    $$PWD/key/protocol/KeySerialClient.cpp \
    $$PWD/system/ui/systempage.cpp \
    $$PWD/system/application/SystemController.cpp \
    $$PWD/log/ui/logpage.cpp \
    $$PWD/log/application/LogController.cpp

HEADERS += \
    $$PWD/auth/ui/loginpage.h \
    $$PWD/auth/application/LoginController.h \
    $$PWD/auth/application/AuthFlowCoordinator.h \
    $$PWD/auth/domain/AuthTypes.h \
    $$PWD/auth/domain/ports/IAuthGateway.h \
    $$PWD/auth/domain/ports/ICredentialSource.h \
    $$PWD/auth/infra/password/authservice.h \
    $$PWD/auth/infra/password/AuthServiceAdapter.h \
    $$PWD/auth/infra/http/AuthGatewayAdapter.h \
    $$PWD/auth/infra/device/MockCredentialSource.h \
    $$PWD/auth/infra/device/CardSerialSource.h \
    $$PWD/auth/infra/device/FingerprintSource.h \
    $$PWD/workbench/ui/workbenchpage.h \
    $$PWD/workbench/application/WorkbenchController.h \
    $$PWD/key/ui/keymanagepage.h \
    $$PWD/key/application/KeySessionService.h \
    $$PWD/key/application/KeyManageController.h \
    $$PWD/key/application/SerialLogManager.h \
    $$PWD/key/application/TicketStore.h \
    $$PWD/key/application/TicketIngressService.h \
    $$PWD/key/protocol/TicketPayloadEncoder.h \
    $$PWD/key/protocol/TicketFrameBuilder.h \
    $$PWD/key/protocol/KeySerialClient.h \
    $$PWD/key/protocol/KeyProtocolDefs.h \
    $$PWD/key/protocol/LogItem.h \
    $$PWD/key/protocol/KeyCrc16.h \
    $$PWD/system/ui/systempage.h \
    $$PWD/system/application/SystemController.h \
    $$PWD/log/ui/logpage.h \
    $$PWD/log/application/LogController.h
