#include "app/AppComposition.h"

#include "app/MainWindowController.h"
#include "core/ConfigManager.h"
#include "features/auth/application/AuthFlowCoordinator.h"
#include "features/auth/infra/http/AuthGatewayAdapter.h"
#include "features/auth/infra/password/AuthServiceAdapter.h"
#include "features/key/application/AdminTicketTerminationService.h"
#include "features/key/application/KeyManageController.h"
#include "features/key/application/KeyMaintenanceService.h"
#include "features/key/application/KeyPortRuntimeCoordinator.h"
#include "features/key/application/KeyProvisioningService.h"
#include "features/key/application/KeySessionService.h"
#include "features/key/application/KeyTaskObservationService.h"
#include "features/key/application/KeyManageObservabilityService.h"
#include "features/key/application/KeyManageSelectionService.h"
#include "features/key/application/SerialLogManager.h"
#include "features/key/application/TicketCancelCoordinator.h"
#include "features/key/application/TicketCancelTrackingService.h"
#include "features/key/application/TicketCancelWorkflowService.h"
#include "features/key/application/TicketDeleteWorkflowService.h"
#include "features/key/application/TicketIngressService.h"
#include "features/key/application/TicketReturnUploadService.h"
#include "features/key/application/TicketReturnWorkflowService.h"
#include "features/key/application/TicketStore.h"
#include "features/key/application/TicketTransferWorkflowService.h"
#include "features/key/application/CaptureTicketWorkflowService.h"
#include "features/key/application/CaptureTicketHttpClient.h"
#include "platform/logging/LogService.h"
#include "platform/process/ProcessService.h"
#include "platform/runtime/ApplicationSessionTracker.h"
#include <QCoreApplication>

AppComposition::AppComposition(QObject *parent)
    : QObject(parent)
    , m_productProfile()
    , m_authService(nullptr)
    , m_authFlow(nullptr)
    , m_processService(nullptr)
    , m_mainWindowController(nullptr)
    , m_keyManageController(nullptr)
    , m_sessionTracker(nullptr)
    , m_started(false)
{
    ConfigManager::instance();
    LogService::instance();
    m_sessionTracker = new ApplicationSessionTracker(this);
    m_sessionTracker->start();

    m_productProfile = ProductProfile::load();
    m_authService = new AuthServiceAdapter(this);
    auto *authGateway = new AuthGatewayAdapter(m_authService, this);
    m_authFlow = new AuthFlowCoordinator(authGateway, this);
    m_processService = new ProcessService(this);
    m_mainWindowController = new MainWindowController(m_authService, this);
    if (m_productProfile.isEnabled(ProductFeature::KeyManagement)) {
        composeKeyManagement();
    }
}

// Creates every concrete key-feature service at the application boundary.
// The controller receives only the completed dependency set and owns no peers.
void AppComposition::composeKeyManagement()
{
    auto *session = new KeySessionService(this);
    auto *logManager = new SerialLogManager(this);
    auto *ticketStore = new TicketStore(this);
    auto *keyTaskObservation = new KeyTaskObservationService(this);
    auto *observability = new KeyManageObservabilityService(this);
    auto *selection = new KeyManageSelectionService(this);
    auto *transferWorkflow = new TicketTransferWorkflowService(
                ticketStore, this);
    auto *captureWorkflow = new CaptureTicketWorkflowService(this);
    auto *captureHttpClient = new CaptureTicketHttpClient(
                m_authService, this);
    // 创建 HTTP 接入服务并由 AppComposition 管理生命周期；稍后经 dependencies
    // 将同一对象交给 Controller 使用，监听在 Controller::start() 时开启。
    auto *ticketIngress = new TicketIngressService(this);
    auto *returnUpload = new TicketReturnUploadService(this);
    auto *returnWorkflow = new TicketReturnWorkflowService(
                ticketStore, this);
    auto *provisioning = new KeyProvisioningService(m_authService, this);
    auto *cancelCoordinator = new TicketCancelCoordinator(this);
    auto *cancelTracking = new TicketCancelTrackingService(
                ticketStore, cancelCoordinator, this);
    auto *cancelWorkflow = new TicketCancelWorkflowService(
                ticketStore, cancelCoordinator, cancelTracking, this);
    auto *deleteWorkflow = new TicketDeleteWorkflowService(
                ticketStore, this);
    auto *adminTermination = new AdminTicketTerminationService(
                ticketStore,
                [this]() { return m_authService->accessToken(); },
                this);

    auto *maintenance = new KeyMaintenanceService(session, this);
    auto *portRuntime = new KeyPortRuntimeCoordinator(
                session,
                []() {
                    KeyPortRuntimeConfig config;
                    ConfigManager *manager = ConfigManager::instance();
                    config.portName = manager->keySerialPort();
                    config.enabled = !ConfigManager::isSerialPortDisabled(
                                config.portName);
                    config.baudRate = manager->baudRate();
                    return config;
                },
                this);

    KeyManageDependencies dependencies;
    dependencies.session = session;
    dependencies.logManager = logManager;
    dependencies.ticketStore = ticketStore;
    dependencies.ticketTransferWorkflow = transferWorkflow;
    dependencies.ticketReturnWorkflow = returnWorkflow;
    // 传递接入服务的地址；Controller 构造时保存到 m_ticketIngress，不另建服务。
    dependencies.ticketIngress = ticketIngress;
    dependencies.returnUploadService = returnUpload;
    dependencies.keyProvisioning = provisioning;
    dependencies.ticketCancelWorkflow = cancelWorkflow;
    dependencies.ticketDeleteWorkflow = deleteWorkflow;
    dependencies.adminTerminationService = adminTermination;
    dependencies.maintenanceService = maintenance;
    dependencies.keyPortRuntime = portRuntime;
    dependencies.keyTaskObservation = keyTaskObservation;
    dependencies.observability = observability;
    dependencies.selection = selection;
    dependencies.captureWorkflow = captureWorkflow;
    dependencies.captureHttpClient = captureHttpClient;
    m_keyManageController = new KeyManageController(dependencies, this);
}

void AppComposition::markCleanExit()
{
    m_sessionTracker->markCleanExit();
}

void AppComposition::start()
{
    if (m_started) {
        return;
    }
    m_started = true;
    m_processService->startAuto();
    if (m_keyManageController) {
        m_keyManageController->start();
    }
}

const ProductProfile &AppComposition::productProfile() const
{
    return m_productProfile;
}

IAuthService *AppComposition::authService() const
{
    return m_authService;
}

AuthFlowCoordinator *AppComposition::authFlow() const
{
    return m_authFlow;
}

IProcessService *AppComposition::processService() const
{
    return m_processService;
}

MainWindowController *AppComposition::mainWindowController() const
{
    return m_mainWindowController;
}

KeyManageController *AppComposition::keyManageController() const
{
    return m_keyManageController;
}
