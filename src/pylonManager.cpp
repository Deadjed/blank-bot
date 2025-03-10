#include "PylonManager.h"
#include <sc2api/sc2_client.cc>

bool PylonManager::IsPylonPowered(const sc2::Unit* pylon) {
    return pylon && pylon->is_powered;
}

sc2::Point2D PylonManager::FindBuildLocationNearPylon(const sc2::Unit* pylon, const sc2::ObservationInterface* observation) {
    if (!pylon) return sc2::Point2D();
    
    float search_radius = 5.0f;
    sc2::Point2D pylon_pos = pylon->pos;
    
    for (float dx = -search_radius; dx <= search_radius; dx += 1.0f) {
        for (float dy = -search_radius; dy <= search_radius; dy += 1.0f) {
            sc2::Point2D test_pos = pylon_pos + sc2::Point2D(dx, dy);
            if (observation->IsPathable(test_pos)) {
                return test_pos;
            }
        }
    }
    return pylon_pos;
}

void PylonManager::AssignIdleWorkersToVespene(sc2::ActionInterface* actions, const sc2::ObservationInterface* observation) {
    // Get all assimilators
    auto assimilators = observation->GetUnits(sc2::Unit::Alliance::Self, 
        [](const sc2::Unit& unit) { 
            return unit.unit_type == sc2::UNIT_TYPEID::PROTOSS_ASSIMILATOR && 
                   unit.build_progress == 1.0f &&    // Only completed assimilators
                   unit.assigned_harvesters < unit.ideal_harvesters; // Not fully saturated
        });
    
    if (assimilators.empty()) {
        return; // No assimilators need workers
    }
    
    // Find idle workers
    auto idleWorkers = observation->GetUnits(sc2::Unit::Alliance::Self, 
        [](const sc2::Unit& unit) { 
            return unit.unit_type == sc2::UNIT_TYPEID::PROTOSS_PROBE && 
                   unit.orders.empty(); 
        });
    
    // Assign up to 3 workers per assimilator (the ideal number)
    for (const auto& assimilator : assimilators) {
        int workersNeeded = assimilator->ideal_harvesters - assimilator->assigned_harvesters;
        
        for (int i = 0; i < workersNeeded && !idleWorkers.empty(); i++) {
            actions->UnitCommand(idleWorkers.back(), sc2::ABILITY_ID::HARVEST_GATHER, assimilator);
            idleWorkers.pop_back();
            
            if (idleWorkers.empty()) {
                break;
            }
        }
    }
}

void PylonManager::BuildAssimilator(const sc2::ObservationInterface* observation, sc2::ActionInterface* actions) {
    // Check if we have enough minerals
    if (observation->GetMinerals() < 75) {
        return;
    }
    
    // Get all vespene geysers that don't already have an assimilator on them
    auto geysers = observation->GetUnits(sc2::Unit::Alliance::Neutral, 
        [&observation](const sc2::Unit& unit) {
            // Check if it's a vespene geyser
            if (unit.unit_type != sc2::UNIT_TYPEID::NEUTRAL_VESPENEGEYSER) {
                return false;
            }
            
            // Check if there's already an assimilator being built on this geyser
            // by checking if there's any assimilator within a very small distance
            auto assimilators = observation->GetUnits(sc2::Unit::Alliance::Self,
                [&unit](const sc2::Unit& assimilator) {
                    return (assimilator.unit_type == sc2::UNIT_TYPEID::PROTOSS_ASSIMILATOR || 
                            assimilator.unit_type == sc2::UNIT_TYPEID::PROTOSS_ASSIMILATORRICH) &&
                           sc2::Distance2D(assimilator.pos, unit.pos) < 1.0f;
                });
            
            // Only return true if there's no assimilator on this geyser
            return assimilators.empty();
        });
    
    // Only try to build if we have available geysers
    if (geysers.empty()) {
        return;
    }
    
    // Find idle workers
    auto workers = observation->GetUnits(sc2::Unit::Alliance::Self, 
        [](const sc2::Unit& unit) {
            return unit.unit_type == sc2::UNIT_TYPEID::PROTOSS_PROBE && 
                   // Only use workers that aren't already building something
                   (unit.orders.empty() || 
                    (unit.orders.size() == 1 && 
                     unit.orders[0].ability_id != sc2::ABILITY_ID::BUILD_ASSIMILATOR));
        });
    
    if (workers.empty()) {
        return;
    }
    
    // Find the geyser closest to our base (nexus)
    auto bases = observation->GetUnits(sc2::Unit::Alliance::Self,
        [](const sc2::Unit& unit) {
            return unit.unit_type == sc2::UNIT_TYPEID::PROTOSS_NEXUS;
        });
    
    if (bases.empty()) {
        return;
    }
    
    const sc2::Unit* closestGeyser = nullptr;
    float minDistance = std::numeric_limits<float>::max();
    
    for (const auto& geyser : geysers) {
        float distance = sc2::Distance2D(geyser->pos, bases.front()->pos);
        if (distance < minDistance) {
            minDistance = distance;
            closestGeyser = geyser;
        }
    }
    
    if (closestGeyser) {
        // Order a worker to build an assimilator on the closest geyser
        actions->UnitCommand(workers.front(), sc2::ABILITY_ID::BUILD_ASSIMILATOR, closestGeyser);
    }
}
