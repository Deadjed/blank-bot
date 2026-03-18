% ============================================================
%  SC2 Strategist — Knowledge Base
%  Compatible with: SWI-Prolog / embedded C++ Prolog (YAP/SWI)
% ============================================================


% ============================================================
%  SECTION 1: World State — Own Economy
% ============================================================

% resources(-Minerals, -Gas)
%   Current stockpile of each resource type.
resources(minerals, Minerals) :- query_game(minerals, Minerals).
resources(gas,      Gas)      :- query_game(gas, Gas).

% population(-Current, -Cap)
%   Current supply used vs. hard supply cap.
population(Current, Cap) :- query_game(supply_used, Current),
                             query_game(supply_cap,  Cap).

% population_free(-Free)
%   Remaining supply headroom.
population_free(Free) :- population(Current, Cap), Free is Cap - Current.

% building(+Name, -Location, -Category, -Capability)
%   Extensional facts injected from the game engine each tick.
%   Category:    economy | military | tech | defence
%   Capability:  what the building provides (atom or list)
%
%   Example facts (injected at runtime):
%     building(command_centre, loc(32,48), economy,  mineral_harvest).
%     building(barracks,       loc(34,50), military, [produce_marine, produce_reaper]).
%     building(factory,        loc(36,50), military, [produce_hellion, produce_tank]).
%     building(engineering_bay,loc(38,48), tech,     [infantry_weapons, infantry_armour]).

% buildings(-Category, -Count)
%   Count all buildings in a given category.
buildings(Category, Count) :-
    aggregate_all(count, building(_, _, Category, _), Count).

% active_harvesters(-Count)
%   Number of workers currently assigned to resource nodes.
active_harvesters(Count) :-
    aggregate_all(count, unit(worker, _, harvesting), Count).

% saturated(+Base)
%   True when the mineral line at Base is at or above optimal saturation (16 workers).
saturated(Base) :-
    aggregate_all(count, unit(worker, Base, harvesting), N),
    N >= 16.


% ============================================================
%  SECTION 2: World State — Own Army
% ============================================================

% unit(+Type, +Location, +State)
%   State: idle | attacking | defending | harvesting | retreating
%   Injected as facts each tick from the game engine.

% army_size(-N)
army_size(N) :- aggregate_all(count, unit(_, _, S), N), S \= harvesting.

% army_strength(-Score)
%   Weighted combat power estimate. Weights are tunable constants.
army_strength(Score) :-
    aggregate_all(
        sum(W),
        ( unit(Type, _, State), State \= harvesting,
          unit_combat_weight(Type, W) ),
        Score
    ).

% unit_combat_weight(+Type, -Weight)
%   Tunable — extend as new unit types are added.
unit_combat_weight(marine,       1).
unit_combat_weight(marauder,     2).
unit_combat_weight(hellion,      1.5).
unit_combat_weight(siege_tank,   5).
unit_combat_weight(medivac,      1).
unit_combat_weight(thor,         8).
unit_combat_weight(battlecruiser,15).


% ============================================================
%  SECTION 3: World State — Technology
% ============================================================

% researched(+Tech)
%   Tech upgrades that have been completed. Injected at runtime.
%   e.g. researched(stim_pack). researched(combat_shield).

% available_tech(-Tech)
%   Techs whose prerequisites are met but not yet researched.
available_tech(Tech) :-
    tech_prereq(Tech, Prereqs),
    \+ researched(Tech),
    maplist(researched, Prereqs).

% tech_prereq(+Tech, -Prereqs)
%   Declare tech trees here. Extend freely.
tech_prereq(stim_pack,        [barracks_tech_lab]).
tech_prereq(combat_shield,    [barracks_tech_lab]).
tech_prereq(concussive_shells,[barracks_tech_lab]).
tech_prereq(siege_mode,       [factory_tech_lab]).
tech_prereq(infantry_weapons_1,[engineering_bay]).
tech_prereq(infantry_armour_1, [engineering_bay]).


% ============================================================
%  SECTION 4: World State — Enemy Intelligence
% ============================================================

% enemy_base(-Location)
% enemy_unit(+Type, -Location, -Count)
% enemy_building(+Name, -Location)
%   Populated from scouting observations; facts expire on a timer
%   managed by the game engine bridge.

% enemy_army_strength(-Score)
%   Mirrors own army_strength but for the estimated enemy force.
enemy_army_strength(Score) :-
    aggregate_all(
        sum(W),
        ( enemy_unit(Type, _, Count),
          unit_combat_weight(Type, W0),
          W is W0 * Count ),
        Score
    ).

% enemy_threat(-Score)
%   Enemy units known to be near our bases.
enemy_threat(Score) :-
    aggregate_all(
        sum(W),
        ( enemy_unit(Type, ELoc, Count),
          own_base(BLoc),
          proximity(ELoc, BLoc, Dist),
          Dist < 20,                        % threat radius in game units
          unit_combat_weight(Type, W0),
          W is W0 * Count ),
        Score
    ).


% ============================================================
%  SECTION 5: Strategic Goals — Definitions
% ============================================================

% can_afford(+MineralCost, +GasCost)
can_afford(M, G) :-
    resources(minerals, Have_M), Have_M >= M,
    resources(gas,      Have_G), Have_G >= G.

% economically_beneficial(+MineralCost, +GasCost)
%   Simple ROI guard: only spend if stockpile permits a buffer.
economically_beneficial(M, G) :-
    resources(minerals, Have_M), Have_M >= M + 200,
    resources(gas,      Have_G), Have_G >= G + 100.

% immediate_threat
%   True when enemy forces are close enough to require attention.
immediate_threat :-
    enemy_threat(Score), Score > 0.

% overwhelmed
%   True when the incoming threat exceeds our defensive capacity.
overwhelmed :-
    enemy_threat(Threat),
    army_strength(Ours),
    Threat > Ours * 1.2.   % 20% margin before we consider fleeing


% ============================================================
%  SECTION 6: Strategic Actions
% ============================================================

% --- Defend ---------------------------------------------------
% Engage if we face an immediate threat we can handle;
% retreat workers if overwhelmed.

defend :-
    immediate_threat,
    \+ overwhelmed,
    army_strength(Ours),
    enemy_threat(Threat),
    Ours >= Threat,
    issue_command(engage_defensive).

defend :-
    overwhelmed,
    issue_command(retreat_workers),
    issue_command(call_for_reinforcements).

% --- Expand ---------------------------------------------------
% Build a new base or add production when economically sound.

expand :-
    can_afford(400, 0),              % command centre cost
    economically_beneficial(400, 0),
    \+ expansion_in_progress,
    issue_command(build_expansion).

expand :-
    can_afford(150, 0),              % supply depot
    population_free(Free),
    Free =< 4,                       % supply-blocked or nearly so
    issue_command(build_supply_depot).

% --- Gather Resources -----------------------------------------
% Ensure workers are assigned and new refineries are up if needed.

gather :-
    own_base(Base),
    \+ saturated(Base),
    active_harvesters(N),
    N < 24,                          % hard cap per base
    issue_command(train_worker).

gather :-
    can_afford(75, 0),
    own_base(Base),
    building(command_centre, Base, economy, _),
    \+ building(refinery, Base, economy, _),   % no refinery yet
    issue_command(build_refinery(Base)).

% --- Offence --------------------------------------------------
% Layered: scout first, build army, then attack when ready.

offence :-
    \+ enemy_base(_),
    issue_command(send_scout).

offence :-
    enemy_base(_),
    army_strength(Ours),
    enemy_army_strength(Enemy),
    Ours > Enemy * 1.5,             % significant strength advantage
    issue_command(attack_enemy_base).

offence :-
    enemy_base(_),
    can_afford(150, 100),
    population_free(Free), Free >= 2,
    issue_command(produce_military_unit).

% --- Technology -----------------------------------------------
% Research the highest-priority available upgrade.

technology :-
    available_tech(Tech),
    tech_priority(Tech, Priority),
    \+ (available_tech(Other), tech_priority(Other, P2), P2 > Priority),
    can_afford(TechCost_M, TechCost_G),
    tech_cost(Tech, TechCost_M, TechCost_G),
    issue_command(research(Tech)).

% tech_priority(+Tech, -Priority)   higher = more urgent
tech_priority(stim_pack,          10).
tech_priority(siege_mode,         9).
tech_priority(infantry_weapons_1, 8).
tech_priority(infantry_armour_1,  7).
tech_priority(combat_shield,      6).
tech_priority(concussive_shells,  5).

% tech_cost(+Tech, -Minerals, -Gas)
tech_cost(stim_pack,          100, 100).
tech_cost(siege_mode,         150, 150).
tech_cost(infantry_weapons_1, 100, 100).
tech_cost(infantry_armour_1,  100, 100).
tech_cost(combat_shield,      100, 100).
tech_cost(concussive_shells,  50,  50).


% ============================================================
%  SECTION 7: Top-Level Strategy Selector
% ============================================================

% strategy/0 — called each decision tick by the game engine bridge.
% Evaluated top-to-bottom; first applicable goal fires.

strategy :-
    defend,    !.   % safety first

strategy :-
    gather,    !.   % economy is always the base

strategy :-
    expand,    !.

strategy :-
    technology, !.

strategy :-
    offence,   !.

strategy :-
    true.           % idle / no applicable action this tick


% ============================================================
%  SECTION 8: Engine Bridge Stubs
%  (Replace with actual C++ foreign-function hooks)
% ============================================================

% query_game(+Key, -Value)   — pull state from the C++ engine
query_game(_, 0).             % stub: always returns 0

% issue_command(+Command)    — push a command to the C++ engine
issue_command(Cmd) :- format("[CMD] ~w~n", [Cmd]).

% proximity(+Loc1, +Loc2, -Dist) — Euclidean distance helper
proximity(loc(X1,Y1), loc(X2,Y2), Dist) :-
    Dist is sqrt((X2-X1)^2 + (Y2-Y1)^2).

% expansion_in_progress — true if a command centre is being built
expansion_in_progress :-
    unit(scv, _, building_command_centre).

% own_base(-Location)
own_base(loc(32,48)).   % stub — inject actual base locations at runtime
