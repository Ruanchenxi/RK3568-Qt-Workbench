#ifndef KEYTASKPRESENTATIONSERVICE_H
#define KEYTASKPRESENTATIONSERVICE_H

#include <QHash>
#include <QList>
#include <QString>

#include "features/key/application/CaptureTicketOperationContext.h"
#include "shared/contracts/KeyTaskDisplayDto.h"
#include "shared/contracts/KeyTaskDto.h"
#include "shared/contracts/SystemTicketDto.h"

class KeyTaskPresentationService
{
public:
    // Resolves business labels from process-known ticket ownership while
    // preserving Q_TASK bytes only as diagnostic text.
    static QList<KeyTaskDisplayDto> present(
            const QList<KeyTaskDto> &tasks,
            const QList<SystemTicketDto> &systemTickets,
            const QHash<QString, CaptureTicketPhase> &capturePhases);

private:
    static QString taskIdFromRaw(const QByteArray &taskIdRaw);
    static QString ordinaryTypeText(int taskType);
    static QString ordinaryStatusText(quint8 status);
    static QString captureStatusText(CaptureTicketPhase phase);
};

#endif // KEYTASKPRESENTATIONSERVICE_H
