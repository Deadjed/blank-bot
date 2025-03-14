#pragma once

#include <sc2api/sc2_api.h>
#include <vector>
#include "protossUnits.h"

using namespace sc2;

// Simple state enum for our behavior tree
enum BotState {
    INIT,
    ECONOMY,
    ARMY,
    ATTACK,
    DEFEND,
    SCOUT
};

// Custom filter for unit types
struct IsUnitType {
    UNIT_TYPEID type_;
    IsUnitType(UNIT_TYPEID type) : type_(type) {}
    bool operator()(const Unit& unit) { return unit.unit_type == type_; }
};

class DecisionTreeBot : public Agent {
public:
    Race race;
    ProtossUnits protoss;

	// Current state of our behavior tree
	BotState current_state = INIT;

	// Track our buildings, units, and enemy units
	std::vector<const Unit*> our_workers;
	std::vector<const Unit*> our_army;
	std::vector<const Unit*> our_production_buildings;
	std::vector<const Unit*> our_tech_buildings;
	std::vector<const Unit*> our_base_buildings;
	std::vector<const Unit*> our_defensive_buildings;
	std::vector<const Unit*> enemy_units;
	Point2D enemy_base_location;
	Point2D main_base_location;
	bool scouting_initiated = false;

    
    // Called when the game starts
    virtual void OnGameStart() final;

    // Called on each game step
    virtual void OnStep() final;

    // Updates our lists of units
    void UpdateUnitLists();

    // Handles the initialization state
    void HandleInitState();

    // Handles the economy building state
    void HandleEconomyState();

    // Handles the army building state
    void HandleArmyState();

    // Handles the attack state
    void HandleAttackState();

    // Handles the defense state
    void HandleDefendState();

    // Handles the scouting state
    void HandleScoutState();

    // Determines what state to transition to next
    void DetermineNextState();

    // Helper functions
    const Unit* FindBuilder();

    const Unit* FindNearestMineralPatch(const Point2D& start);

    Point2D FindPlacement(AbilityID ability_type_for_structure, Point2D near_to, float max_distance);

    Point2D GetRandomPointInCircle(const Point2D& center, float radius);

    float GetRandomScalar();

    int CountUnitType(UNIT_TYPEID unit_type);

    // This function is called when the bot's game ends
    virtual void OnGameEnd() final;
};
