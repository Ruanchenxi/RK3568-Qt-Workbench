#include "platform/process/ServiceStartupPolicy.h"

ServiceStartupPolicy::Decision ServiceStartupPolicy::decide(
    const Observation &observation)
{
    Decision decision;

    if (observation.occupiedPorts.isEmpty())
    {
        decision.action = Action::Start;
        decision.reason = Reason::PortsAvailable;
        return decision;
    }

    if (!observation.foreignProcessPorts.isEmpty())
    {
        decision.action = Action::Block;
        decision.reason = Reason::ForeignProcessDetected;
        return decision;
    }

    if (!observation.ownedOutagePorts.isEmpty())
    {
        decision.action = Action::CleanAndStart;
        decision.reason = Reason::OwnedOutageDetected;
        decision.portsToClean = observation.ownedOutagePorts;
        return decision;
    }

    // Occupancy without positive ownership evidence must never authorize a kill.
    decision.action = Action::Block;
    decision.reason = Reason::ForeignProcessDetected;
    return decision;
}