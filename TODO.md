# High Level Decision
**Decision Hierarchy**
1. Are we under attack?
   - Defend if we can
   - Otherwise flee
3. Do we have resources?
   - Gather nearby minerals if available
   - otherwise expand base near more resources
5. Do we need to build to increase strategic capability?
   - Build to advance resources
   - Build to advance military development capabilities
   - Build to advance technological capability
7. Are we ready to defend ourselves?
   - Build defensive capability
   - Build army if required
   - Otherwise launch an attack
9. Can we advance our technology?
    - Advance resource collection if possible
    - Otherwise develop defensive capabilities
    - Otherwise advance military capabilities

**Required Information**
- Unit/building under attack status
- resources
  - minerals
  - vespene gas
- population vs population cap
- amount of each building type
- amount of each military type
- amount of each other unit type
- technology advancements?

**output**
Which subtree to currently control the AI

# Subtrees   
## Economy   
**Goal:** To gather resources, build capability, and advance technology   
**Input**
- Current buildings
- Current research?   

**Output**
- Resources to mine
- Buildings to build
- Research to develop

## Army
**Goal:** To build an army effective against opponents capabilities    
**Input**
- Amount of Melee/Range/Air units   

**Output**
- Units to build

## Attack
**Goal:** To advance military units towards enemy base    
**Input**
- Enemy locations (and their type Melee/Range/Air)
- Our army unit locations (and their type Melee/Range/Air)   

**Output**
- Delegate unit attacks
- Unit movement
- Unit abilities

## Defend
**Goal:** To control military units to defend our base, or if viable relocate our base to hide from the enemy   
**Input**
- Enemy locations (and their type Melee/Range/Air)
- Our army unit locations (and their type Melee/Range/Air)
- Our defensive buildings   

**Output**
- Delegate unit/building attacks
- Unit movement
- Unit/building abilities
