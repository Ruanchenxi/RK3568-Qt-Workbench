/**
 * @file WorkbenchController.h
 * @brief 工作台页面控制器
 */
#ifndef WORKBENCHCONTROLLER_H
#define WORKBENCHCONTROLLER_H

#include <QObject>
#include <QUrl>

#include "shared/contracts/IAuthService.h"

class WorkbenchController : public QObject
{
    Q_OBJECT
public:
    explicit WorkbenchController(IAuthService *auth = nullptr, QObject *parent = nullptr);
    ~WorkbenchController() override = default;

    bool shouldLoadNow();
    bool buildTargetUrl(QUrl *url, QString *safeSummary, QString *error) const;

private:
    QString currentAuthSignature() const;

    IAuthService *m_auth;
    bool m_hasLoaded;
    QString m_lastSignature;
};

#endif // WORKBENCHCONTROLLER_H
