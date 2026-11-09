#include "app/ProductProfile.h"

#include "core/ConfigManager.h"

ProductProfile ProductProfile::load()
{
    ProductProfile profile;
    ConfigManager *config = ConfigManager::instance();

    profile.m_workbenchEnabled =
            config->value(QStringLiteral("product/features/workbench"), true).toBool();
    profile.m_keyManagementEnabled =
            config->value(QStringLiteral("product/features/keyManagement"), true).toBool();
    profile.m_systemManagementEnabled =
            config->value(QStringLiteral("product/features/systemManagement"), true).toBool();
    profile.m_serviceOperationsEnabled =
            config->value(QStringLiteral("product/features/serviceOperations"), true).toBool();
    profile.m_keyboardEnabled =
            config->value(QStringLiteral("product/features/keyboard"), true).toBool();

    return profile;
}

bool ProductProfile::isEnabled(ProductFeature feature) const
{
    switch (feature) {
    case ProductFeature::Workbench:
        return m_workbenchEnabled;
    case ProductFeature::KeyManagement:
        return m_keyManagementEnabled;
    case ProductFeature::SystemManagement:
        return m_systemManagementEnabled;
    case ProductFeature::ServiceOperations:
        return m_serviceOperationsEnabled;
    case ProductFeature::Keyboard:
        return m_keyboardEnabled;
    }
    return false;
}

