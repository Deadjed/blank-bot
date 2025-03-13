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
    // 1. Get all our units
    Units units = observation->GetUnits();
    
    // Filter our workers
    std::vector<const Unit*> workers;
    std::vector<const Unit*> mineral_fields;
    std::vector<const Unit*> assimilators;
    
    for (const auto& unit : units) {
        if (unit->unit_type == UNIT_TYPEID::PROTOSS_PROBE && unit->alliance == Unit::Alliance::Self) {
            workers.push_back(unit);
        }
        else if (IsMineralField(unit->unit_type) && unit->alliance == Unit::Alliance::Neutral) {
            mineral_fields.push_back(unit);
        }
        else if (unit->unit_type == UNIT_TYPEID::PROTOSS_ASSIMILATOR && 
                unit->alliance == Unit::Alliance::Self && 
                unit->build_progress == 1.0f) {
            assimilators.push_back(unit);
        }
    }
    
    // 2. Classify all workers by current assignment
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
    
    // 3. Calculate optimal distributions
    int total_available_workers = idle_workers.size() + mineral_workers.size() + gas_workers.size();
    int total_mineral_patches = mineral_fields.size();
    int total_gas_geysers = assimilators.size();
    
    // Calculate ideal distribution
    int ideal_workers_per_gas = 3;
    int ideal_gas_workers = total_gas_geysers * ideal_workers_per_gas;
    int ideal_mineral_workers = total_available_workers - ideal_gas_workers;
    
    // Make sure we're not trying to assign negative workers to minerals
    if (ideal_mineral_workers < 0) {
        // We don't have enough workers for both tasks, prioritize some minimum mineral income
        int min_mineral_workers = std::min(8, total_available_workers);
        ideal_mineral_workers = min_mineral_workers;
        ideal_gas_workers = total_available_workers - min_mineral_workers;
    }
    
    // 4. Reassign workers to achieve optimal distribution
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
    
    // 5. Assign any remaining idle workers to minerals
    for (const auto& worker : idle_workers) {
        AssignWorkerToNearestMineralPatch(worker, mineral_fields, actions);
    }
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

void PylonManager::BuildAssimilator(const sc2::ObservationInterface* observation, sc2::ActionInterface* actions) {
    std::cout << "\nWe are trying to build an Assimilator" << std::endl;
    // Check if we have enough minerals
    if (observation->GetMinerals() < 75) {
        std::cout << "Not enough minerals!" << std::endl;
        return;
    }

    // Find all our bases (Nexuses)
    auto bases = observation->GetUnits(sc2::Unit::Alliance::Self,
        [](const sc2::Unit& unit) {
            return unit.unit_type == sc2::UNIT_TYPEID::PROTOSS_NEXUS;
        });

    if (bases.empty()) {
        std::cout << "No bases!" << std::endl;
        return; // No bases, can't assign geysers effectively
    }

    // For each base, find the closest available geysers
    for (const auto& base : bases) {
        float maxDistanceToBase = 15.0f;

        // Find all nearby vespene geysers without assimilators
		auto nearbyGeysers = observation->GetUnits(sc2::Unit::Alliance::Neutral,
		[&observation, &base, maxDistanceToBase](const sc2::Unit& unit) {
			// Check if it's a vespene geyser
			if (unit.unit_type != sc2::UNIT_TYPEID::NEUTRAL_VESPENEGEYSER &&
				unit.unit_type != sc2::UNIT_TYPEID::NEUTRAL_RICHVESPENEGEYSER &&
				unit.unit_type != sc2::UNIT_TYPEID::NEUTRAL_PURIFIERVESPENEGEYSER &&
				unit.unit_type != sc2::UNIT_TYPEID::NEUTRAL_SHAKURASVESPENEGEYSER &&
				unit.unit_type != sc2::UNIT_TYPEID::NEUTRAL_SPACEPLATFORMGEYSER &&
				unit.unit_type != sc2::UNIT_TYPEID::NEUTRAL_PROTOSSVESPENEGEYSER) {
				return false;
			}

			// Check if the geyser is close enough to this base
			float distance = sc2::Distance2D(unit.pos, base->pos);
			if (distance > maxDistanceToBase) {
				return false;
			}

			// Debug the distance
			std::cout << "Found geyser at distance: " << distance << " from base" << std::endl;

			// Check if there's already an assimilator on this geyser
			auto assimilators = observation->GetUnits(sc2::Unit::Alliance::Self,
				[&unit](const sc2::Unit& assimilator) {
					return (assimilator.unit_type == sc2::UNIT_TYPEID::PROTOSS_ASSIMILATOR ||
							assimilator.unit_type == sc2::UNIT_TYPEID::PROTOSS_ASSIMILATORRICH) &&
							sc2::Distance2D(assimilator.pos, unit.pos) < 1.0f;
				});

			// Only return true if there's no assimilator on this geyser
			bool isAvailable = assimilators.empty();
			if (!isAvailable) {
				std::cout << "Geyser already has an assimilator" << std::endl;
			}
			return isAvailable;
		});
        // If there are no available geysers near this base, continue to the next base
        if (nearbyGeysers.empty()) {
            std::cout << "No geysers found here!" << std::endl;
            continue;
        }
        std::cout << "Geysers found:" << nearbyGeysers.size() << std::endl;

        // Count existing assimilators near this base
        int assimilatorsNearBase = 0;
        auto existingAssimilators = observation->GetUnits(sc2::Unit::Alliance::Self,
            [&base, maxDistanceToBase](const sc2::Unit& unit) {
                return (unit.unit_type == sc2::UNIT_TYPEID::PROTOSS_ASSIMILATOR ||
                    unit.unit_type == sc2::UNIT_TYPEID::PROTOSS_ASSIMILATORRICH) &&
                    sc2::Distance2D(unit.pos, base->pos) <= maxDistanceToBase;
            });
        assimilatorsNearBase = existingAssimilators.size();

        // Don't build more than 2 assimilators per base (typical number of geysers)
        if (assimilatorsNearBase >= 2) {
            std::cout << "Already have enough assimilators!" << std::endl;
            continue;
        }

        // Sort geysers by distance to the base
        std::sort(nearbyGeysers.begin(), nearbyGeysers.end(),
            [&base](const sc2::Unit* a, const sc2::Unit* b) {
                return sc2::Distance2D(a->pos, base->pos) < sc2::Distance2D(b->pos, base->pos);
            });

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
            std::cout << "no Workers found!!" << std::endl;
            return;
        }

        // Order a worker to build an assimilator on the closest geyser to this base
		std::cout << "Building Assimilator" << std::endl;
        actions->UnitCommand(workers.front(), sc2::ABILITY_ID::BUILD_ASSIMILATOR, nearbyGeysers.front());

        // Only build one assimilator per step
        return;
    }
}
