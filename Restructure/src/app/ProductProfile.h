#ifndef PRODUCTPROFILE_H
#define PRODUCTPROFILE_H

enum class ProductFeature
{
    Workbench,
    KeyManagement,
    SystemManagement,
    ServiceOperations,
    Keyboard
};

class ProductProfile
{
public:
    static ProductProfile load();

    bool isEnabled(ProductFeature feature) const;

private:
    bool m_workbenchEnabled = true;
    bool m_keyManagementEnabled = true;
    bool m_systemManagementEnabled = true;
    bool m_serviceOperationsEnabled = true;
    bool m_keyboardEnabled = true;
};

#endif // PRODUCTPROFILE_H

