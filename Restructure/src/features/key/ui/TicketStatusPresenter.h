#ifndef TICKETSTATUSPRESENTER_H
#define TICKETSTATUSPRESENTER_H

#include <QString>

#include "shared/contracts/SystemTicketDto.h"

class TicketStatusPresenter
{
public:
    static QString tableText(const QString &state);
    static QString tableText(const SystemTicketDto &ticket);
    static QString detailText(const SystemTicketDto &ticket);
    static QString statusColor(const SystemTicketDto &ticket);
    static QString returnHint(const SystemTicketDto &ticket,
                              quint8 keyTaskStatus,
                              bool canManualReturn,
                              const QString &blockedReason);
};

#endif // TICKETSTATUSPRESENTER_H
