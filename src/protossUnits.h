#ifndef PROTOSS_UNITS_H
#define PROTOSS_UNITS_H

#include <sc2api/sc2_agent.h>
#include <vector>
#include <unordered_set>

class ProtossUnits {
public:
    ProtossUnits() {
        InitializeUnits();
    }

    // Methods to retrieve different unit categories
    const std::unordered_set<sc2::UNIT_TYPEID>& GetWorkers() const { return workers; }
    const std::unordered_set<sc2::UNIT_TYPEID>& GetArmy() const { return army; }
    const std::unordered_set<sc2::UNIT_TYPEID>& GetProductionBuildings() const { return productionBuildings; }
    const std::unordered_set<sc2::UNIT_TYPEID>& GetTechBuildings() const { return techBuildings; }
    const std::unordered_set<sc2::UNIT_TYPEID>& GetBaseStructures() const { return baseStructures; }
    const std::unordered_set<sc2::UNIT_TYPEID>& GetDefensiveBuildings() const { return defensiveBuildings; }

    // Check if a unit belongs to a category
    bool IsWorker(sc2::UNIT_TYPEID unit) const { return workers.count(unit) > 0; }
    bool IsArmyUnit(sc2::UNIT_TYPEID unit) const { return army.count(unit) > 0; }
    bool IsProductionBuilding(sc2::UNIT_TYPEID unit) const { return productionBuildings.count(unit) > 0; }
    bool IsTechBuilding(sc2::UNIT_TYPEID unit) const { return techBuildings.count(unit) > 0; }
    bool IsBaseStructure(sc2::UNIT_TYPEID unit) const { return baseStructures.count(unit) > 0; }
    bool IsDefensiveBuilding(sc2::UNIT_TYPEID unit) const { return defensiveBuildings.count(unit) > 0; }

private:
    std::unordered_set<sc2::UNIT_TYPEID> workers;
    std::unordered_set<sc2::UNIT_TYPEID> army;
    std::unordered_set<sc2::UNIT_TYPEID> productionBuildings;
    std::unordered_set<sc2::UNIT_TYPEID> techBuildings;
    std::unordered_set<sc2::UNIT_TYPEID> baseStructures;
    std::unordered_set<sc2::UNIT_TYPEID> defensiveBuildings;

    void InitializeUnits() {
        // Workers
        workers.insert(sc2::UNIT_TYPEID::PROTOSS_PROBE);

        // Combat Units (Army)
        army = {
            sc2::UNIT_TYPEID::PROTOSS_ZEALOT,
            sc2::UNIT_TYPEID::PROTOSS_STALKER,
            sc2::UNIT_TYPEID::PROTOSS_ADEPT,
            sc2::UNIT_TYPEID::PROTOSS_SENTRY,
            sc2::UNIT_TYPEID::PROTOSS_HIGHTEMPLAR,
            sc2::UNIT_TYPEID::PROTOSS_DARKTEMPLAR,
            sc2::UNIT_TYPEID::PROTOSS_ARCHON,
            sc2::UNIT_TYPEID::PROTOSS_IMMORTAL,
            sc2::UNIT_TYPEID::PROTOSS_COLOSSUS,
            sc2::UNIT_TYPEID::PROTOSS_DISRUPTOR,
            sc2::UNIT_TYPEID::PROTOSS_PHOENIX,
            sc2::UNIT_TYPEID::PROTOSS_VOIDRAY,
            sc2::UNIT_TYPEID::PROTOSS_ORACLE,
            sc2::UNIT_TYPEID::PROTOSS_TEMPEST,
            sc2::UNIT_TYPEID::PROTOSS_CARRIER,
            sc2::UNIT_TYPEID::PROTOSS_MOTHERSHIP
        };

        // Production Buildings
        productionBuildings = {
            sc2::UNIT_TYPEID::PROTOSS_GATEWAY,
            sc2::UNIT_TYPEID::PROTOSS_WARPGATE,
            sc2::UNIT_TYPEID::PROTOSS_ROBOTICSFACILITY,
            sc2::UNIT_TYPEID::PROTOSS_STARGATE
        };

        // Tech Buildings
        techBuildings = {
            sc2::UNIT_TYPEID::PROTOSS_CYBERNETICSCORE,
            sc2::UNIT_TYPEID::PROTOSS_TWILIGHTCOUNCIL,
            sc2::UNIT_TYPEID::PROTOSS_ROBOTICSBAY,
            sc2::UNIT_TYPEID::PROTOSS_TEMPLARARCHIVE,
            sc2::UNIT_TYPEID::PROTOSS_DARKSHRINE,
            sc2::UNIT_TYPEID::PROTOSS_FLEETBEACON
        };

        // Base & Economy Structures
        baseStructures = {
            sc2::UNIT_TYPEID::PROTOSS_NEXUS,
            sc2::UNIT_TYPEID::PROTOSS_PYLON,
            sc2::UNIT_TYPEID::PROTOSS_ASSIMILATOR
        };

        // Defensive Structures
        defensiveBuildings = {
            sc2::UNIT_TYPEID::PROTOSS_PHOTONCANNON,
            sc2::UNIT_TYPEID::PROTOSS_SHIELDBATTERY
        };
    }
};

#endif // PROTOSS_UNITS_H

