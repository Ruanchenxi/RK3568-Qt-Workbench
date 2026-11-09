#ifndef SERVICESTARTUPPOLICY_H
#define SERVICESTARTUPPOLICY_H

#include <QList>

class ServiceStartupPolicy
{
public:
    enum class Action
    {
        Start,
        CleanAndStart,
        Block
    };

    enum class Reason
    {
        PortsAvailable,
        OwnedOutageDetected,
        ForeignProcessDetected
    };

    struct Observation
    {
        QList<int> occupiedPorts;
        QList<int> ownedOutagePorts;
        QList<int> foreignProcessPorts;
    };

    struct Decision
    {
        Action action = Action::Block;
        Reason reason = Reason::ForeignProcessDetected;
        QList<int> portsToClean;
    };

    /**
     * Decide whether the local service script may start.
     *
     * This policy has no process, port, configuration, or logging side effects.
     * ProcessService executes the returned action and verifies the result.
     */
    static Decision decide(const Observation &observation);
};

#endif // SERVICESTARTUPPOLICY_H