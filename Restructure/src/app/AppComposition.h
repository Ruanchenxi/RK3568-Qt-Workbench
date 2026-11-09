#ifndef APPCOMPOSITION_H
#define APPCOMPOSITION_H

#include <QObject>

#include "app/ProductProfile.h"

class AuthFlowCoordinator;
class IAuthService;
class IProcessService;
class MainWindowController;
class KeyManageController;
class ApplicationSessionTracker;

class AppComposition : public QObject
{
public:
    explicit AppComposition(QObject *parent = nullptr);

    void start();
    // Marks a clean application exit only after local outage shutdown completes.
    void markCleanExit();

    const ProductProfile &productProfile() const;
    IAuthService *authService() const;
    AuthFlowCoordinator *authFlow() const;
    IProcessService *processService() const;
    MainWindowController *mainWindowController() const;
    KeyManageController *keyManageController() const;

private:
    // Assembles the optional key feature once; feature internals never create peers.
    void composeKeyManagement();

    ProductProfile m_productProfile;
    IAuthService *m_authService;
    AuthFlowCoordinator *m_authFlow;
    IProcessService *m_processService;
    MainWindowController *m_mainWindowController;
    KeyManageController *m_keyManageController;
    ApplicationSessionTracker *m_sessionTracker;
    bool m_started;
};

#endif // APPCOMPOSITION_H

