#ifndef KEYTASKDISPLAYDTO_H
#define KEYTASKDISPLAYDTO_H

#include <QString>

// Carries display-only key-task semantics from L2 to the key-management page.
// Raw protocol values remain diagnostic data and never drive UI behavior.
struct KeyTaskDisplayDto
{
    QString taskIdText;
    QString typeText;
    QString sourceText;
    QString statusText;
    QString diagnosticText;
    bool unregistered = false;
};

#endif // KEYTASKDISPLAYDTO_H
