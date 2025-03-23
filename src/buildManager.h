#pragma once
#ifndef BUILD_MANAGER_H
#define BUILD_MANAGER_H

#include "sc2api/sc2_api.h"

class BuildManager {
public:
    void BuildAssimilator(const sc2::ObservationInterface* observation, sc2::ActionInterface* actions);
    void Build(const sc2::ObservationInterface* observation, sc2::ActionInterface* actions, sc2::QueryInterface* query, sc2::ABILITY_ID building, std::vector<const sc2::Unit*> our_workers);

private:
    sc2::Point2D FindPlacement(sc2::QueryInterface* query, sc2::AbilityID ability_type_for_structure, sc2::Point2D near_to, float max_distance);
    sc2::Point2D GetRandomPointInCircle(const sc2::Point2D& center, float radius);
    float GetRandomScalar();
};

#endif
