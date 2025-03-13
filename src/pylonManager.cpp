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
