#include "buildManager.h"
#include <iostream>

void BuildManager::BuildAssimilator(const sc2::ObservationInterface* observation, sc2::ActionInterface* actions) {
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
