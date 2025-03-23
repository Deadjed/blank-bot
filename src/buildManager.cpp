#include "buildManager.h"
#include "probeManager.h"
#include <iostream>
#include <vector>

void BuildManager::BuildAssimilator(const sc2::ObservationInterface* observation, sc2::ActionInterface* actions) {
    // Check if we have enough minerals
    if (observation->GetMinerals() < 75) {
        std::cout << "Not enough minerals to build assimilator!" << std::endl;
        return;
    }

    // Find all our bases (Nexuses)
    auto bases = observation->GetUnits(sc2::Unit::Alliance::Self,
        [](const sc2::Unit& unit) {
            return unit.unit_type == sc2::UNIT_TYPEID::PROTOSS_NEXUS;
        });

    if (bases.empty()) {
        std::cout << "No bases, can't assign geysers effectively!" << std::endl;
        return;
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

void BuildManager::Build(const sc2::ObservationInterface* observation, sc2::ActionInterface* actions, sc2::QueryInterface* query, sc2::ABILITY_ID building, std::vector<const sc2::Unit*> our_workers)
{
    ProbeManager probeManager;

    // TODO: Get mineral/gas cost of building to ensure we can build it
    //sc2::Abilities abilityData = client->Observation()->GetAbilityData();
    //abilityData
    //if (client->Observation()->GetMinerals() < client->Observation().) {
    //    std::cout << "Not enough minerals to build Cybernetics Core!" << std::endl;
    //    return;
    //}

    // Get main base location
	// Find our base
	sc2::Point2D base_location;
	const sc2::Units units = observation->GetUnits(sc2::Unit::Alliance::Self);
	for (const auto& unit : units) {
		if (unit->alliance == sc2::Unit::Alliance::Self && unit->unit_type == sc2::UNIT_TYPEID::PROTOSS_NEXUS) {
			base_location = unit->pos;
			break;
		}
	}
    //sc2::Point2D main_base_location = observation->GetUnits(sc2::Unit::Alliance::Self);

	// Find a place to build
	const sc2::Unit* builder = probeManager.FindBuilder(our_workers);
	if (builder) {
		sc2::Point2D build_location = FindPlacement(query, building, base_location, 20.0f);
		if (build_location.x != 0) {
			actions->UnitCommand(builder, building, build_location);
		}
	}
}

sc2::Point2D BuildManager::FindPlacement(sc2::QueryInterface* query, sc2::AbilityID ability_type_for_structure, sc2::Point2D near_to, float max_distance) {
	sc2::Point2D result = sc2::Point2D(0, 0);
	float distance = max_distance;
	
	// Try up to 10 different locations at decreasing distances
	for (int i = 0; i < 10; ++i) {
		sc2::Point2D try_location = GetRandomPointInCircle(near_to, distance);
		
		// Query if this location is valid for this building type
		if (query->Placement(ability_type_for_structure, try_location)) {
			return try_location;
		}
		
		// Decrease distance to try closer to the original point
		distance -= distance / 10.0f;
	}
	
	return result; // Return 0,0 if no placement found
}

sc2::Point2D BuildManager::GetRandomPointInCircle(const sc2::Point2D& center, float radius) {
	float angle = GetRandomScalar() * 3.14159f * 2.0f;
	float distance = sqrt(GetRandomScalar()) * radius;
	
	return sc2::Point2D(center.x + cos(angle) * distance, center.y + sin(angle) * distance);
}

float BuildManager::GetRandomScalar() {
	return static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
}
