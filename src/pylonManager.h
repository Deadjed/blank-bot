#ifndef PYLON_MANAGER_H
#define PYLON_MANAGER_H

#include "sc2api/sc2_api.h"

using namespace sc2;

class PylonManager {
public:
    static bool IsPylonPowered(const sc2::Unit* pylon);
    static sc2::Point2D FindBuildLocationNearPylon(const sc2::Unit* pylon, const sc2::ObservationInterface* observation);
    static void AssignIdleWorkersToVespene(sc2::ActionInterface* actions, const sc2::ObservationInterface* observation);
    void BuildAssimilator(const sc2::ObservationInterface* observation, sc2::ActionInterface* actions);
    void ManageWorkerAssignments(sc2::ActionInterface* actions, const sc2::ObservationInterface* observation);
    
private:
    bool IsMineralField(UNIT_TYPEID unit_type);
    void AssignWorkerToNearestAssimilator(const Unit* worker, 
                                         const std::vector<const Unit*>& assimilators,
                                         sc2::ActionInterface* actions);
    void AssignWorkerToNearestMineralPatch(const Unit* worker,
                                          const std::vector<const Unit*>& minerals,
                                          sc2::ActionInterface* actions);
};

#endif // PYLON_MANAGER_H
