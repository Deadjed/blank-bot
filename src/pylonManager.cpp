#include "PylonManager.h"
#include <sc2api/sc2_client.cc>

using namespace sc2;

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

void PylonManager::ManageWorkerAssignments(sc2::ActionInterface* actions, const sc2::ObservationInterface* observation) {

    // TODO: We already categorised all units at start of step, pass that information through here?
    // Get all workers
    Units units = observation->GetUnits();
    
    std::vector<const Unit*> workers;
    std::vector<const Unit*> mineral_fields;
    std::vector<const Unit*> assimilators;
    
    for (const auto& unit : units) {
        if (unit->unit_type == UNIT_TYPEID::PROTOSS_PROBE && unit->alliance == Unit::Alliance::Self) {
            workers.push_back(unit);
        }
        else if (IsMineralField(unit->unit_type) && unit->alliance == Unit::Alliance::Neutral) {
            // TODO: check if within range of a nexus
            mineral_fields.push_back(unit);
        }
        else if (unit->unit_type == UNIT_TYPEID::PROTOSS_ASSIMILATOR && 
                unit->alliance == Unit::Alliance::Self && 
                unit->build_progress == 1.0f) {
            assimilators.push_back(unit);
        }
    }
    
    // Classify all workers by current assignment
    std::vector<const Unit*> idle_workers;
    std::vector<const Unit*> mineral_workers;
    std::vector<const Unit*> gas_workers;
    std::vector<const Unit*> other_workers; // Building, scouting, etc.
    
    for (const auto& worker : workers) {
        if (worker->orders.empty()) {
            idle_workers.push_back(worker);
        } 
        else {
            auto& order = worker->orders.front();
            if (order.ability_id == ABILITY_ID::HARVEST_GATHER) {
                // Find the target unit
                const Unit* target = nullptr;
                for (const auto& unit : units) {
                    if (unit->tag == order.target_unit_tag) {
                        target = unit;
                        break;
                    }
                }
                
                if (target) {
                    if (target->unit_type == UNIT_TYPEID::PROTOSS_ASSIMILATOR) {
                        gas_workers.push_back(worker);
                    } 
                    else if (IsMineralField(target->unit_type)) {
                        mineral_workers.push_back(worker);
                    }
                }
            } 
            else {
                other_workers.push_back(worker);
            }
        }
    }
    
    // Calculate optimal distributions
    int total_available_workers = idle_workers.size() + mineral_workers.size() + gas_workers.size();
    int total_mineral_patches = mineral_fields.size();
    int total_gas_geysers = assimilators.size();
    
    // Calculate ideal distribution
    int ideal_workers_per_gas = 2;
    int ideal_gas_workers = total_gas_geysers * ideal_workers_per_gas;
    int ideal_mineral_workers = total_mineral_patches * 2;
    
    // Make sure we're not trying to assign negative workers to minerals
    if (ideal_mineral_workers < 0) {
        // We don't have enough workers for both tasks, prioritize some minimum mineral income
        int min_mineral_workers = std::min(8, total_available_workers);
        ideal_mineral_workers = min_mineral_workers;
        ideal_gas_workers = total_available_workers - min_mineral_workers;
    }
    
    // Reassign workers to achieve optimal distribution
    // First, make sure gas has exactly the right number of workers
    int gas_workers_delta = ideal_gas_workers - gas_workers.size();
    
    if (gas_workers_delta > 0) {
        // Need to add workers to gas
        int to_move = std::min(gas_workers_delta, (int)idle_workers.size());
        
        // First use idle workers
        for (int i = 0; i < to_move; i++) {
            AssignWorkerToNearestAssimilator(idle_workers[i], assimilators, actions);
        }
        
        // Remove assigned workers from idle list
        if (to_move > 0) {
            idle_workers.erase(idle_workers.begin(), idle_workers.begin() + to_move);
        }
        
        // If we still need more gas workers, pull from minerals
        gas_workers_delta -= to_move;
        to_move = std::min(gas_workers_delta, (int)mineral_workers.size());
        
        for (int i = 0; i < to_move; i++) {
            AssignWorkerToNearestAssimilator(mineral_workers[i], assimilators, actions);
        }
    } 
    else if (gas_workers_delta < 0) {
        // Too many workers on gas, move some to minerals
        int to_move = std::min(-gas_workers_delta, (int)gas_workers.size());
        
        for (int i = 0; i < to_move; i++) {
            AssignWorkerToNearestMineralPatch(gas_workers[i], mineral_fields, actions);
        }
    }
    
    // Assign any remaining idle workers to minerals
    for (const auto& worker : idle_workers) {
        AssignWorkerToNearestMineralPatch(worker, mineral_fields, actions);
    }

    // TODO: Build workers if needed
}

// Helper methods
bool PylonManager::IsMineralField(UNIT_TYPEID unit_type) {
    // Check if it's a mineral field type
    return unit_type == UNIT_TYPEID::NEUTRAL_MINERALFIELD || 
           unit_type == UNIT_TYPEID::NEUTRAL_MINERALFIELD750 ||
           unit_type == UNIT_TYPEID::NEUTRAL_RICHMINERALFIELD ||
           unit_type == UNIT_TYPEID::NEUTRAL_RICHMINERALFIELD750;
}

void PylonManager::AssignWorkerToNearestAssimilator(const Unit* worker, 
                                                   const std::vector<const Unit*>& assimilators,
                                                   ActionInterface* actions) {
    if (assimilators.empty()) return;
    
    // Find assimilator with fewest assigned workers
    const Unit* best_assimilator = nullptr;
    int lowest_workers = 999;
    
    for (const auto& assimilator : assimilators) {
        if (assimilator->assigned_harvesters < lowest_workers) {
            lowest_workers = assimilator->assigned_harvesters;
            best_assimilator = assimilator;
        }
    }
    
    if (best_assimilator) {
        actions->UnitCommand(worker, ABILITY_ID::HARVEST_GATHER, best_assimilator);
    }
}

void PylonManager::AssignWorkerToNearestMineralPatch(const Unit* worker,
                                                    const std::vector<const Unit*>& minerals,
                                                    ActionInterface* actions) {
    if (minerals.empty()) return;
    
    // Find nearest mineral patch
    float closest_dist = std::numeric_limits<float>::max();
    const Unit* closest_mineral = nullptr;
    
    for (const auto& mineral : minerals) {
        float dist = Distance2D(worker->pos, mineral->pos);
        if (dist < closest_dist) {
            closest_dist = dist;
            closest_mineral = mineral;
        }
    }
    
    if (closest_mineral) {
        actions->UnitCommand(worker, ABILITY_ID::HARVEST_GATHER, closest_mineral);
    }
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
        return;
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
