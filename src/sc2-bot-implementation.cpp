#include <sc2api/sc2_api.h>
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>

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

class MyBot : public Agent {
public:
    // Current state of our behavior tree
    BotState current_state = INIT;
    
    // Track our buildings, units, and enemy units
    std::vector<const Unit*> our_workers;
    std::vector<const Unit*> our_army;
    std::vector<const Unit*> enemy_units;
    std::vector<const Unit*> our_bases;
    Point2D enemy_base_location;
    Point2D main_base_location;
    bool scouting_initiated = false;
    
    // Called when the game starts
    virtual void OnGameStart() final {
        std::cout << "Game started!" << std::endl;
        
        // Print out some game info
        std::cout << "Map: " << Observation()->GetGameInfo().map_name << std::endl;
        
        // Find our starting location
        for (const Unit& unit : Observation()->GetUnits()) {
            if (unit.alliance == Unit::Alliance::Self && unit.unit_type == UNIT_TYPEID::TERRAN_COMMANDCENTER) {
                main_base_location = unit.pos;
                break;
            }
        }
        
        // Make an educated guess about enemy location (opposite corner for now)
        const GameInfo& game_info = Observation()->GetGameInfo();
        Point2D map_center = game_info.playable_max / 2;
        enemy_base_location = Point2D(
            game_info.playable_max.x - main_base_location.x,
            game_info.playable_max.y - main_base_location.y
        );
    }

    // Called on each game step
    virtual void OnStep() final {
        // Update our unit lists
        UpdateUnitLists();
        
        // Execute the current state of our behavior tree
        switch (current_state) {
            case INIT:
                HandleInitState();
                break;
            case ECONOMY:
                HandleEconomyState();
                break;
            case ARMY:
                HandleArmyState();
                break;
            case ATTACK:
                HandleAttackState();
                break;
            case DEFEND:
                HandleDefendState();
                break;
            case SCOUT:
                HandleScoutState();
                break;
        }
        
        // Transition between states based on conditions
        DetermineNextState();
    }

    // Updates our lists of units
    void UpdateUnitLists() {
        our_workers.clear();
        our_army.clear();
        enemy_units.clear();
        our_bases.clear();
        
        Units units = Observation()->GetUnits();
        for (const auto& unit : units) {
            if (unit->alliance == Unit::Alliance::Self) {
                UnitTypeID unit_type = unit->unit_type;
                
                // Workers (SCVs for Terran)
                if (unit_type == UNIT_TYPEID::TERRAN_SCV) {
                    our_workers.push_back(unit);
                }
                // Army units (very basic for now - any non-worker, non-building)
                else if (!Units::IsBuilding(unit_type) && unit_type != UNIT_TYPEID::TERRAN_SCV) {
                    our_army.push_back(unit);
                }
                // Command centers and other base structures
                else if (unit_type == UNIT_TYPEID::TERRAN_COMMANDCENTER || 
                         unit_type == UNIT_TYPEID::TERRAN_ORBITALCOMMAND || 
                         unit_type == UNIT_TYPEID::TERRAN_PLANETARYFORTRESS) {
                    our_bases.push_back(unit);
                }
            }
            else if (unit->alliance == Unit::Alliance::Enemy) {
                enemy_units.push_back(unit);
            }
        }
    }

    // Handles the initialization state
    void HandleInitState() {
        std::cout << "Initializing..." << std::endl;
        // Start with economy focus
        current_state = ECONOMY;
    }

    // Handles the economy building state
    void HandleEconomyState() {
        std::cout << "Economy state..." << std::endl;
        
        // Build more SCVs if we have fewer than 20 and we have a command center
        if (our_workers.size() < 20 && !our_bases.empty()) {
            for (const auto& base : our_bases) {
                if (base->unit_type == UNIT_TYPEID::TERRAN_COMMANDCENTER && 
                    base->orders.empty() && 
                    Observation()->GetMinerals() >= 50) {
                    Actions()->UnitCommand(base, ABILITY_ID::TRAIN_SCV);
                    break;
                }
            }
        }
        
        // Build a supply depot if we're close to supply cap
        if (Observation()->GetFoodUsed() >= Observation()->GetFoodCap() - 5 && 
            Observation()->GetFoodCap() < 200 && 
            Observation()->GetMinerals() >= 100) {
            // Find a place near our base to build the supply depot
            const Unit* builder = FindBuilder();
            if (builder) {
                Point2D build_location = FindPlacement(ABILITY_ID::BUILD_SUPPLYDEPOT, main_base_location, 15.0f);
                if (build_location.x != 0) {
                    Actions()->UnitCommand(builder, ABILITY_ID::BUILD_SUPPLYDEPOT, build_location);
                }
            }
        }
        
        // Build a barracks if we have at least 16 workers and enough minerals
        if (our_workers.size() >= 16 && 
            CountUnitType(UNIT_TYPEID::TERRAN_BARRACKS) < 2 && 
            Observation()->GetMinerals() >= 150) {
            // Find a place to build a barracks
            const Unit* builder = FindBuilder();
            if (builder) {
                Point2D build_location = FindPlacement(ABILITY_ID::BUILD_BARRACKS, main_base_location, 20.0f);
                if (build_location.x != 0) {
                    Actions()->UnitCommand(builder, ABILITY_ID::BUILD_BARRACKS, build_location);
                }
            }
        }
        
        // Ensure workers are mining
        for (const auto& worker : our_workers) {
            if (worker->orders.empty()) {
                const Unit* mineral = FindNearestMineralPatch(worker->pos);
                if (mineral) {
                    Actions()->UnitCommand(worker, ABILITY_ID::HARVEST_GATHER, mineral);
                }
            }
        }
    }

    // Handles the army building state
    void HandleArmyState() {
        std::cout << "Army state..." << std::endl;
        
        // Find all barracks
        Units barracks = Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_BARRACKS));
        
        // Train marines from barracks
        for (const auto& barrack : barracks) {
            if (barrack->orders.empty() && Observation()->GetMinerals() >= 50) {
                Actions()->UnitCommand(barrack, ABILITY_ID::TRAIN_MARINE);
            }
        }
    }

    // Handles the attack state
    void HandleAttackState() {
        std::cout << "Attack state..." << std::endl;
        
        // If we have enough army units, attack the enemy base
        if (our_army.size() >= 15) {
            for (const auto& unit : our_army) {
                Actions()->UnitCommand(unit, ABILITY_ID::ATTACK_ATTACK, enemy_base_location);
            }
        }
        
        // If we see enemy units, attack them
        if (!enemy_units.empty() && !our_army.empty()) {
            const Unit* target = enemy_units.front(); // Just pick the first enemy unit for now
            for (const auto& unit : our_army) {
                Actions()->UnitCommand(unit, ABILITY_ID::ATTACK_ATTACK, target);
            }
        }
    }

    // Handles the defense state
    void HandleDefendState() {
        std::cout << "Defend state..." << std::endl;
        
        // If there are enemies near our base, defend
        Point2D defense_point = main_base_location;
        for (const auto& unit : our_army) {
            Actions()->UnitCommand(unit, ABILITY_ID::ATTACK_ATTACK, defense_point);
        }
    }

    // Handles the scouting state
    void HandleScoutState() {
        std::cout << "Scout state..." << std::endl;
        
        // Send a worker to scout if we haven't already
        if (!scouting_initiated && our_workers.size() > 10) {
            const Unit* scout = our_workers.back(); // Use the last worker
            Actions()->UnitCommand(scout, ABILITY_ID::MOVE, enemy_base_location);
            scouting_initiated = true;
        }
    }

    // Determines what state to transition to next
    void DetermineNextState() {
        // Check if we're under attack
        bool under_attack = false;
        for (const auto& enemy : enemy_units) {
            float distance_to_base = Distance2D(enemy->pos, main_base_location);
            if (distance_to_base < 30.0f) {
                under_attack = true;
                break;
            }
        }
        
        // If we're under attack, switch to defense
        if (under_attack) {
            current_state = DEFEND;
            return;
        }
        
        // If we have a decent army size, go attack
        if (our_army.size() >= 20) {
            current_state = ATTACK;
            return;
        }
        
        // If we need more army, focus on that
        if (our_army.size() < 20 && our_workers.size() >= 16) {
            current_state = ARMY;
            return;
        }
        
        // If we need to scout and have enough workers
        if (!scouting_initiated && our_workers.size() > 10) {
            current_state = SCOUT;
            return;
        }
        
        // Default to economy focus
        current_state = ECONOMY;
    }

    // Helper functions
    const Unit* FindBuilder() {
        if (our_workers.empty()) {
            return nullptr;
        }
        
        // Just return the first idle worker for now
        for (const auto& worker : our_workers) {
            if (worker->orders.empty()) {
                return worker;
            }
        }
        
        // If no idle workers, just return the first one
        return our_workers.front();
    }

    const Unit* FindNearestMineralPatch(const Point2D& start) {
        Units units = Observation()->GetUnits(Unit::Alliance::Neutral);
        float distance = std::numeric_limits<float>::max();
        const Unit* target = nullptr;
        
        for (const auto& u : units) {
            if (u->unit_type == UNIT_TYPEID::NEUTRAL_MINERALFIELD) {
                float d = Distance2D(u->pos, start);
                if (d < distance) {
                    distance = d;
                    target = u;
                }
            }
        }
        
        return target;
    }

    Point2D FindPlacement(AbilityID ability_type_for_structure, Point2D near_to, float max_distance) {
        Point2D result = Point2D(0, 0);
        float distance = max_distance;
        
        // Try up to 10 different locations at decreasing distances
        for (int i = 0; i < 10; ++i) {
            Point2D try_location = GetRandomPointInCircle(near_to, distance);
            
            // Query if this location is valid for this building type
            if (Query()->Placement(ability_type_for_structure, try_location)) {
                return try_location;
            }
            
            // Decrease distance to try closer to the original point
            distance -= distance / 10.0f;
        }
        
        return result; // Return 0,0 if no placement found
    }

    Point2D GetRandomPointInCircle(const Point2D& center, float radius) {
        float angle = GetRandomScalar() * 3.14159f * 2.0f;
        float distance = sqrt(GetRandomScalar()) * radius;
        
        return Point2D(center.x + cos(angle) * distance, center.y + sin(angle) * distance);
    }

    float GetRandomScalar() {
        return static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
    }

    int CountUnitType(UNIT_TYPEID unit_type) {
        int count = 0;
        Units units = Observation()->GetUnits(Unit::Alliance::Self);
        
        for (const auto& unit : units) {
            if (unit->unit_type == unit_type) {
                ++count;
            }
        }
        
        return count;
    }

    // This function is called when the bot's game ends
    virtual void OnGameEnd() final {
        std::cout << "Game ended!" << std::endl;
    }
};

int main(int argc, char* argv[]) {
    Coordinator coordinator;
    coordinator.LoadSettings(argc, argv);
    
    MyBot bot;
    coordinator.SetParticipants({
        CreateParticipant(Race::Terran, &bot),
        CreateComputer(Race::Terran, Difficulty::Medium)
    });
    
    coordinator.LaunchStarcraft();
    coordinator.StartGame(sc2::kMapBelShirVestigeLE);
    
    while (coordinator.Update()) {
        // Game runs until it's over
    }
    
    return 0;
}
