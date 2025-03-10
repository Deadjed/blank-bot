#ifndef PYLON_MANAGER_H
#define PYLON_MANAGER_H

#include "sc2api/sc2_api.h"

class PylonManager {
public:
    static bool IsPylonPowered(const sc2::Unit* pylon);
    static sc2::Point2D FindBuildLocationNearPylon(const sc2::Unit* pylon, const sc2::ObservationInterface* observation);
    static void AssignIdleWorkersToVespene(sc2::ActionInterface* actions, const sc2::ObservationInterface* observation);
    void BuildAssimilator(const sc2::ObservationInterface* observation, sc2::ActionInterface* actions);
};

#endif // PYLON_MANAGER_H
