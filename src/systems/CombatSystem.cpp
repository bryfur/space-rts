#include "CombatSystem.h"
#include "../components/Components.h"
#include "../rendering/AudioManager.h"
#include <SDL2/SDL_log.h>
#include <cmath>
#include <algorithm>

CombatSystem::CombatSystem(ECSRegistry& registry)
    : SystemBase(registry)
    , mAIUpdateTimer(0.0F)
    , mGroupCoordinationTimer(0.0F)
    , mCurrentStrategicTarget(INVALID_ENTITY)
    , mMassAttackInProgress(false)
    , mSurroundInProgress(false)
    , mAudioManager(nullptr)
{
}

CombatSystem::~CombatSystem() {
    Shutdown();
}

bool CombatSystem::Initialize() {
    SDL_Log("Combat system initialized");
    return true;
}

void CombatSystem::Update(float deltaTime) {
    UpdateWeaponCooldowns(deltaTime);
    ProcessAutomaticFiring(deltaTime);
    ProcessEnemyAI(deltaTime);
    ProcessPlayerAutoAttack(deltaTime);
}

void CombatSystem::Shutdown() {
    SDL_Log("Combat system shutdown");
}

void CombatSystem::FireWeapon(EntityID shooter, EntityID targetEntity, float targetX, float targetY) {
    using namespace Components;
    
    auto* spacecraft = mRegistry.GetComponent<Spacecraft>(shooter);
    auto* position = mRegistry.GetComponent<Position>(shooter);
    
    if (!spacecraft || !position) {
        return;
    }
    
    // Check weapon cooldown
    if (spacecraft->lastShotTime > 0.0F) {
        return; // Still cooling down
    }
    
    // Calculate direction to target
    float dirX = 0.0F;
    float dirY = 0.0F;
    CalculateDirection(position->posX, position->posY, targetX, targetY, dirX, dirY);
    
    // Rotate ship to face target when shooting
    float deltaX = targetX - position->posX;
    float deltaY = targetY - position->posY;
    constexpr float RADIANS_TO_DEGREES = 180.0F / 3.14159F;
    constexpr float TRIANGLE_ROTATION_OFFSET = 90.0F; // Triangles point up by default
    float angleToTarget = (std::atan2(deltaY, deltaX) * RADIANS_TO_DEGREES) - TRIANGLE_ROTATION_OFFSET;
    spacecraft->angle = angleToTarget;
    
    // Create projectile
    CreateProjectile(shooter, position->posX, position->posY, dirX, dirY, 2.0F, targetEntity);
    
    // Play weapon fire sound effect
    if (mAudioManager) {
        mAudioManager->PlayPew();
    }
    
    // Set weapon cooldown
    spacecraft->lastShotTime = WEAPON_COOLDOWN;    
}

void CombatSystem::ProcessAutomaticFiring(float deltaTime) {
    (void)deltaTime; // Will be used for timing automatic weapons
    
    // For now, this is handled by manual firing and AI firing
    // Could add automatic weapons or charged shots here
}

void CombatSystem::UpdateWeaponCooldowns(float deltaTime) {
    using namespace Components;
    
    mRegistry.ForEach<Spacecraft>([&](EntityID entity, Spacecraft& spacecraft) {
        if (spacecraft.lastShotTime > 0.0F) {
            spacecraft.lastShotTime -= deltaTime;
            if (spacecraft.lastShotTime < 0.0F) {
                spacecraft.lastShotTime = 0.0F;
            }
        }
    });
}

void CombatSystem::ProcessEnemyAI(float deltaTime) {
    using namespace Components;
    
    // Handle group coordination less frequently
    mGroupCoordinationTimer += deltaTime;
    if (mGroupCoordinationTimer >= GROUP_COORDINATION_INTERVAL) {
        CoordinateGroupTactics(deltaTime);
        mGroupCoordinationTimer = 0.0F;
    }
    
    // Update AI timer
    mAIUpdateTimer -= deltaTime;
    if (mAIUpdateTimer > 0.0F) return;
    mAIUpdateTimer = AI_UPDATE_INTERVAL;
    
    // Process each enemy unit with unified state machine
    mRegistry.ForEach<Spacecraft>([&](EntityID entity, Spacecraft& spacecraft) {
        if (spacecraft.type != SpacecraftType::Enemy) return;
        
        auto* health = mRegistry.GetComponent<Health>(entity);
        auto* position = mRegistry.GetComponent<Position>(entity);
        if (!health || !health->isAlive || !position) return;
        
        // Update state timer
        spacecraft.aiStateTimer += deltaTime;
        
        // Analyze current tactical situation
        TacticalInfo tactical = AnalyzeTacticalSituation(entity);
        
        // Unified state machine logic (formation-aware)
        AIState newState = UpdateAIStateMachine(entity, spacecraft, tactical);
        
        // Change state if needed
        if (newState != spacecraft.aiState) {
            spacecraft.aiState = newState;
            spacecraft.aiStateTimer = 0.0F;
        }
        
        // Execute current state behavior (always formation-aware)
        ExecuteAIState(entity, spacecraft, tactical);
    });
}

Components::AIState CombatSystem::UpdateAIStateMachine(EntityID entity, const Components::Spacecraft& spacecraft, const TacticalInfo& tactical) {
    using namespace Components;
    
    auto* health = mRegistry.GetComponent<Health>(entity);
    if (!health) return spacecraft.aiState;
    
    float healthPercent = static_cast<float>(health->currentHP) / static_cast<float>(health->maxHP);
    
    // Get formation info for consistent target selection
    const GroupFormation* formation = GetActiveFormation(entity);
    
    // 1. RETREAT: Low health and outnumbered (highest priority)
    if (healthPercent < 0.2F && tactical.nearbyPlayerShips > tactical.nearbyEnemyShips) {
        return AIState::Retreat;
    }
    
    // 2. REGROUP: Isolated and not in immediate danger
    if (tactical.nearbyEnemyShips == 0 && tactical.nearbyPlayerShips == 0) {
        return AIState::Regroup;
    }
    
    // 3. ENGAGE: Have target in engagement range (use unified target selection)
    EntityID engageTarget = SelectBestTarget(entity, formation, true); // true = engagement range only
    if (engageTarget != INVALID_ENTITY) {
        return AIState::Engage;
    }
    
    // 4. APPROACH: Have target detectable but out of engagement range
    EntityID approachTarget = SelectBestTarget(entity, formation, false); // false = any range
    if (approachTarget != INVALID_ENTITY) {
        return AIState::Approach;
    }
    
    // 5. SEARCH: No targets found
    return AIState::Search;
}

void CombatSystem::ExecuteAIState(EntityID entity, Components::Spacecraft& spacecraft, const TacticalInfo& tactical) {
    using namespace Components;
    
    auto* position = mRegistry.GetComponent<Position>(entity);
    if (!position) return;
    
    // All states are now formation-aware by default
    switch (spacecraft.aiState) {
        case AIState::Search:
            ExecuteSearchState(entity, spacecraft, tactical);
            break;
            
        case AIState::Approach:
            ExecuteApproachState(entity, spacecraft, tactical);
            break;
            
        case AIState::Engage:
            ExecuteEngageState(entity, spacecraft, tactical);
            break;
            
        case AIState::Retreat:
            ExecuteRetreatState(entity, spacecraft, tactical);
            break;
            
        case AIState::Regroup:
            ExecuteRegroupState(entity, spacecraft, tactical);
            break;
    }
}

const CombatSystem::GroupFormation* CombatSystem::GetActiveFormation(EntityID entity) const {
    for (const auto& formation : mActiveFormations) {
        if (std::find(formation.members.begin(), formation.members.end(), entity) != formation.members.end()) {
            return &formation;
        }
    }
    return nullptr;
}

bool CombatSystem::ShouldPrioritizeFormationTarget(EntityID entity, const GroupFormation& formation) const {
    using namespace Components;
    
    // If formation has a target and this unit is close to it, prioritize formation target
    if (formation.target == INVALID_ENTITY) return false;
    
    auto* position = mRegistry.GetComponent<Position>(entity);
    auto* targetPos = mRegistry.GetComponent<Position>(formation.target);
    if (!position || !targetPos) return false;
    
    float distance = CalculateDistance(position->posX, position->posY, targetPos->posX, targetPos->posY);
    
    // Check if formation target is a planet or ship and if we're in range
    auto* planetComponent = mRegistry.GetComponent<Planet>(formation.target);
    float maxRange = planetComponent ? PLANET_ATTACK_RANGE : AI_FIRING_RANGE;
    
    return distance <= maxRange * 1.5F; // Use extended range for formation coordination
}

EntityID CombatSystem::SelectBestTarget(EntityID entity, const GroupFormation* formation, bool engagementRangeOnly) const {
    using namespace Components;
    
    auto* position = mRegistry.GetComponent<Position>(entity);
    if (!position) return INVALID_ENTITY;
    
    // Priority 1: Formation target if we should prioritize it (formation-aware range checking)
    if (formation && formation->target != INVALID_ENTITY) {
        auto* targetPos = mRegistry.GetComponent<Position>(formation->target);
        if (targetPos) {
            float distance = CalculateDistance(position->posX, position->posY, targetPos->posX, targetPos->posY);
            
            // Determine range based on target type
            auto* planetComponent = mRegistry.GetComponent<Planet>(formation->target);
            float maxRange = planetComponent ? PLANET_ATTACK_RANGE : AI_FIRING_RANGE;
            
            if (engagementRangeOnly) {
                // For engagement checks, use exact firing range
                if (distance <= maxRange) {
                    return formation->target;
                }
            } else {
                // For approach checks, formation targets are always valid if detectable
                return formation->target;
            }
        }
    }
    
    // Priority 2: Nearest player ship in appropriate range
    float shipRange = engagementRangeOnly ? AI_FIRING_RANGE : std::numeric_limits<float>::max();
    EntityID nearestShip = FindNearestTarget(entity, shipRange);
    if (nearestShip != INVALID_ENTITY) return nearestShip;
    
    // Priority 3: Nearest planet in appropriate range
    float planetRange = engagementRangeOnly ? PLANET_ATTACK_RANGE : std::numeric_limits<float>::max();
    EntityID nearestPlanet = FindNearestPlanet(entity, planetRange);
    if (nearestPlanet != INVALID_ENTITY) return nearestPlanet;
    
    return INVALID_ENTITY;
}

void CombatSystem::CalculateFormationPosition(const GroupFormation& formation, size_t unitIndex, float& posX, float& posY) const {
    float angle;
    float radius;
    
    switch (formation.type) {
        case GroupFormation::MASS_ATTACK:
            // Tight formation for concentrated assault
            angle = (static_cast<float>(unitIndex) / static_cast<float>(formation.members.size())) * 2.0F * 3.14159F;
            radius = 0.2F + (static_cast<float>(unitIndex % 2) * 0.1F); // Staggered rows
            posX = formation.formationCenterX + std::cos(angle) * radius;
            posY = formation.formationCenterY + std::sin(angle) * radius;
            break;
            
        case GroupFormation::SURROUND:
            // Circle formation around target
            angle = (static_cast<float>(unitIndex) / static_cast<float>(formation.members.size())) * 2.0F * 3.14159F;
            radius = SURROUND_RADIUS;
            posX = formation.formationCenterX + std::cos(angle) * radius;
            posY = formation.formationCenterY + std::sin(angle) * radius;
            break;
            
        default:
            // Default to formation center
            posX = formation.formationCenterX;
            posY = formation.formationCenterY;
            break;
    }
    
    // Apply screen boundaries
    ApplyScreenBoundaries(posX, posY);
}

void CombatSystem::ExecuteSearchState(EntityID entity, Components::Spacecraft& spacecraft, const TacticalInfo& tactical) {
    using namespace Components;
    
    auto* position = mRegistry.GetComponent<Position>(entity);
    if (!position) return;
    
    // Always clear invalid targets first
    if (spacecraft.aiTarget != INVALID_ENTITY) {
        auto* targetPos = mRegistry.GetComponent<Position>(spacecraft.aiTarget);
        auto* targetHealth = mRegistry.GetComponent<Health>(spacecraft.aiTarget);
        if (!targetPos || !targetHealth || !targetHealth->isAlive) {
            spacecraft.aiTarget = INVALID_ENTITY;
        }
    }
    
    // Priority 1: Find ANY player target on the map (omniscient detection)
    EntityID target = FindNearestTarget(entity, std::numeric_limits<float>::max());
    if (target != INVALID_ENTITY) {
        spacecraft.aiTarget = target;
        auto* targetPos = mRegistry.GetComponent<Position>(target);
        if (targetPos) {
            spacecraft.destX = targetPos->posX;
            spacecraft.destY = targetPos->posY;
            spacecraft.isMoving = true;
        }
        return; // State machine will transition to Approach next update
    }
    
    // Priority 2: Look for planets to attack (also unlimited range)
    EntityID planet = FindNearestPlanet(entity, std::numeric_limits<float>::max());
    if (planet != INVALID_ENTITY) {
        spacecraft.aiTarget = planet;
        auto* planetPos = mRegistry.GetComponent<Position>(planet);
        if (planetPos) {
            spacecraft.destX = planetPos->posX;
            spacecraft.destY = planetPos->posY;
            spacecraft.isMoving = true;
        }
        return;
    }
    
    // Priority 3: If no targets found, move toward center of map and keep searching
    float centerX = 0.0F;
    float centerY = 0.0F;
    
    // Try to find the center of any remaining player activity
    int playerUnitsFound = 0;
    mRegistry.ForEach<Spacecraft>([&](EntityID targetEntity, const Spacecraft& targetSpacecraft) {
        if (targetSpacecraft.type != SpacecraftType::Player) return;
        
        auto* targetPos = mRegistry.GetComponent<Position>(targetEntity);
        auto* targetHealth = mRegistry.GetComponent<Health>(targetEntity);
        if (!targetPos || !targetHealth || !targetHealth->isAlive) return;
        
        centerX += targetPos->posX;
        centerY += targetPos->posY;
        playerUnitsFound++;
    });
    
    if (playerUnitsFound > 0) {
        centerX /= static_cast<float>(playerUnitsFound);
        centerY /= static_cast<float>(playerUnitsFound);
    }
    
    ApplyScreenBoundaries(centerX, centerY);
    spacecraft.destX = centerX;
    spacecraft.destY = centerY;
    spacecraft.isMoving = true;
}

void CombatSystem::ExecuteApproachState(EntityID entity, Components::Spacecraft& spacecraft, const TacticalInfo& tactical) {
    using namespace Components;
    
    auto* position = mRegistry.GetComponent<Position>(entity);
    if (!position) return;
    
    // Get formation info (may be nullptr)
    const GroupFormation* formation = GetActiveFormation(entity);
    
    // Select best target (use unified target selection system)
    EntityID target = SelectBestTarget(entity, formation, false); // false = any range for approach
    if (target != INVALID_ENTITY) {
        spacecraft.aiTarget = target;
    }
    
    // If in formation, maintain formation position while approaching
    if (formation) {
        size_t unitIndex = 0;
        for (size_t i = 0; i < formation->members.size(); ++i) {
            if (formation->members[i] == entity) {
                unitIndex = i;
                break;
            }
        }
        
        float formationX, formationY;
        CalculateFormationPosition(*formation, unitIndex, formationX, formationY);
        
        // Move to formation position if not close enough
        float distanceToFormationPos = CalculateDistance(position->posX, position->posY, formationX, formationY);
        if (distanceToFormationPos > FORMATION_TOLERANCE) {
            spacecraft.destX = formationX;
            spacecraft.destY = formationY;
            spacecraft.isMoving = true;
            return;
        }
    }
    
    // Move toward target
    if (target != INVALID_ENTITY) {
        auto* targetPos = mRegistry.GetComponent<Position>(target);
        if (targetPos) {
            float distance = CalculateDistance(position->posX, position->posY, targetPos->posX, targetPos->posY);
            
            // Determine optimal range based on target type
            float optimalRange = AI_FIRING_RANGE * 0.8F;
            auto* planetComponent = mRegistry.GetComponent<Planet>(target);
            if (planetComponent) {
                optimalRange = PLANET_ATTACK_RANGE * 0.9F;
            }
            
            if (distance > optimalRange) {
                float deltaX = targetPos->posX - position->posX;
                float deltaY = targetPos->posY - position->posY;
                float length = std::sqrt(deltaX * deltaX + deltaY * deltaY);
                
                if (length > 0.01F) {
                    spacecraft.destX = targetPos->posX - (deltaX / length) * optimalRange;
                    spacecraft.destY = targetPos->posY - (deltaY / length) * optimalRange;
                    spacecraft.isMoving = true;
                }
            }
        }
    }
}

void CombatSystem::ExecuteEngageState(EntityID entity, Components::Spacecraft& spacecraft, const TacticalInfo& tactical) {
    using namespace Components;
    auto* position = mRegistry.GetComponent<Position>(entity);
    if (!position) return;
    
    // Get formation info (may be nullptr)
    const GroupFormation* formation = GetActiveFormation(entity);
    
    // Maintain formation position if in formation
    if (formation) {
        size_t unitIndex = 0;
        for (size_t i = 0; i < formation->members.size(); ++i) {
            if (formation->members[i] == entity) {
                unitIndex = i;
                break;
            }
        }
        
        float formationX, formationY;
        CalculateFormationPosition(*formation, unitIndex, formationX, formationY);
        
        float distanceToFormationPos = CalculateDistance(position->posX, position->posY, formationX, formationY);
        if (distanceToFormationPos > FORMATION_TOLERANCE * 2.0F) {
            spacecraft.destX = formationX;
            spacecraft.destY = formationY;
            spacecraft.isMoving = true;
        } else {
            spacecraft.isMoving = false;
        }
    }
    
    // Select best target (use unified target selection system)
    EntityID target = SelectBestTarget(entity, formation, true); // true = engagement range only
    
    if (target != INVALID_ENTITY) {
        auto* targetPos = mRegistry.GetComponent<Position>(target);
        if (targetPos) {
            float distance = CalculateDistance(position->posX, position->posY, targetPos->posX, targetPos->posY);
            
            // Check range based on target type
            float maxRange = AI_FIRING_RANGE;
            auto* planetComponent = mRegistry.GetComponent<Planet>(target);
            if (planetComponent) {
                maxRange = PLANET_ATTACK_RANGE;
            }
            
            if (distance <= maxRange) {
                spacecraft.aiTarget = target;
                
                // Rotate to face target
                float deltaX = targetPos->posX - position->posX;
                float deltaY = targetPos->posY - position->posY;
                constexpr float RADIANS_TO_DEGREES = 180.0F / 3.14159F;
                constexpr float TRIANGLE_ROTATION_OFFSET = 90.0F;
                float angleToTarget = (std::atan2(deltaY, deltaX) * RADIANS_TO_DEGREES) - TRIANGLE_ROTATION_OFFSET;
                spacecraft.angle = angleToTarget;
                
                // Fire weapon
                FireWeapon(entity, target, targetPos->posX, targetPos->posY);
                
                // If target is too close and we're outnumbered, back away slightly
                if (!formation && distance < AI_FIRING_RANGE * 0.3F && tactical.nearbyPlayerShips > tactical.nearbyEnemyShips) {
                    float retreatDistance = AI_FIRING_RANGE * 0.6F;
                    spacecraft.destX = position->posX - (deltaX / distance) * retreatDistance;
                    spacecraft.destY = position->posY - (deltaY / distance) * retreatDistance;
                    spacecraft.isMoving = true;
                }
            }
        }
    }
}

void CombatSystem::ExecuteRetreatState(EntityID entity, Components::Spacecraft& spacecraft, const TacticalInfo& tactical) {
    using namespace Components;
    
    auto* position = mRegistry.GetComponent<Position>(entity);
    if (!position) return;
    
    // Find nearest threat and move away
    EntityID threat = FindNearestTarget(entity, AI_FIRING_RANGE * 2.0F);
    if (threat != INVALID_ENTITY) {
        auto* threatPos = mRegistry.GetComponent<Position>(threat);
        if (threatPos) {
            float deltaX = position->posX - threatPos->posX;
            float deltaY = position->posY - threatPos->posY;
            float length = std::sqrt(deltaX * deltaX + deltaY * deltaY);
            
            if (length > 0.01F) {
                // Check if we can retreat behind a healthy ally
                EntityID protectiveAlly = FindHealthyAllyToHideBehind(entity, threat);
                if (protectiveAlly != INVALID_ENTITY) {
                    auto* allyPos = mRegistry.GetComponent<Position>(protectiveAlly);
                    if (allyPos) {
                        // Move to position where ally is between us and the threat
                        float allyThreatDeltaX = allyPos->posX - threatPos->posX;
                        float allyThreatDeltaY = allyPos->posY - threatPos->posY;
                        float allyThreatLength = std::sqrt(allyThreatDeltaX * allyThreatDeltaX + allyThreatDeltaY * allyThreatDeltaY);
                        
                        if (allyThreatLength > 0.01F) {
                            // Position ourselves behind the ally relative to the threat
                            float hideDistance = AI_FIRING_RANGE * 0.7F;
                            float retreatX = allyPos->posX + (allyThreatDeltaX / allyThreatLength) * hideDistance;
                            float retreatY = allyPos->posY + (allyThreatDeltaY / allyThreatLength) * hideDistance;
                            
                            // Apply screen boundaries and set destination
                            ApplyScreenBoundaries(retreatX, retreatY);
                            SetRetreatDestination(spacecraft, position->posX, position->posY, retreatX, retreatY);
                            return;
                        }
                    }
                }
                
                // Calculate retreat distance with oscillation reduction
                float currentDistance = length;
                float targetDistance = AI_FIRING_RANGE * 1.8F; // Slightly closer than 2.0F to reduce oscillation
                
                // Only move if we're significantly closer than target distance
                if (currentDistance < targetDistance - AI_FIRING_RANGE * 0.2F) {
                    float retreatDistance = targetDistance - currentDistance;
                    float retreatX = position->posX + (deltaX / length) * retreatDistance;
                    float retreatY = position->posY + (deltaY / length) * retreatDistance;
                    
                    // Apply screen boundaries and set destination
                    ApplyScreenBoundaries(retreatX, retreatY);
                    SetRetreatDestination(spacecraft, position->posX, position->posY, retreatX, retreatY);
                }
            }
        }
    } else {
        // No immediate threat, find friendly units to regroup with
        EntityID bestRegroupTarget = FindBestRegroupTarget(entity);
        if (bestRegroupTarget != INVALID_ENTITY) {
            auto* targetPos = mRegistry.GetComponent<Position>(bestRegroupTarget);
            if (targetPos) {
                float distance = CalculateDistance(position->posX, position->posY, targetPos->posX, targetPos->posY);
                float optimalDistance = AI_FIRING_RANGE * 1.2F; // Stay close for mutual support
                
                if (distance > optimalDistance + AI_FIRING_RANGE * 0.1F) { // Add hysteresis
                    spacecraft.destX = targetPos->posX;
                    spacecraft.destY = targetPos->posY;
                    spacecraft.isMoving = true;
                }
            }
        } else {
            // No allies found, move toward center of friendly units
            std::vector<EntityID> allies = GetAllEnemyUnits();
            if (!allies.empty()) {
                float centerX = 0.0F, centerY = 0.0F;
                int count = 0;
                
                for (EntityID ally : allies) {
                    if (ally == entity) continue;
                    auto* allyPos = mRegistry.GetComponent<Position>(ally);
                    if (allyPos) {
                        centerX += allyPos->posX;
                        centerY += allyPos->posY;
                        count++;
                    }
                }
                
                if (count > 0) {
                    centerX /= static_cast<float>(count);
                    centerY /= static_cast<float>(count);
                    ApplyScreenBoundaries(centerX, centerY);
                    spacecraft.destX = centerX;
                    spacecraft.destY = centerY;
                    spacecraft.isMoving = true;
                }
            }
        }
    }
}

void CombatSystem::ExecuteRegroupState(EntityID entity, Components::Spacecraft& spacecraft, const TacticalInfo& tactical) {
    using namespace Components;
    
    auto* position = mRegistry.GetComponent<Position>(entity);
    if (!position) return;
    
    // Find other friendly units and move toward them
    std::vector<EntityID> allies = GetAllEnemyUnits();
    EntityID nearestAlly = INVALID_ENTITY;
    float nearestDistance = std::numeric_limits<float>::max();
    
    for (EntityID ally : allies) {
        if (ally == entity) continue;
        
        auto* allyPos = mRegistry.GetComponent<Position>(ally);
        if (allyPos) {
            float distance = CalculateDistance(position->posX, position->posY, allyPos->posX, allyPos->posY);
            if (distance < nearestDistance) {
                nearestDistance = distance;
                nearestAlly = ally;
            }
        }
    }
    
    if (nearestAlly != INVALID_ENTITY) {
        auto* allyPos = mRegistry.GetComponent<Position>(nearestAlly);
        if (allyPos) {
            // Move toward ally but not too close
            float targetDistance = AI_FIRING_RANGE * 1.5F;
            if (nearestDistance > targetDistance) {
                float deltaX = allyPos->posX - position->posX;
                float deltaY = allyPos->posY - position->posY;
                float length = std::sqrt(deltaX * deltaX + deltaY * deltaY);
                
                if (length > 0.01F) {
                    spacecraft.destX = position->posX + (deltaX / length) * (length - targetDistance);
                    spacecraft.destY = position->posY + (deltaY / length) * (length - targetDistance);
                    spacecraft.isMoving = true;
                }
            }
        }
    } else {
        // No allies found, move to map center
        spacecraft.destX = 0.0F;
        spacecraft.destY = 0.0F;
        spacecraft.isMoving = true;
    }
}

void CombatSystem::CreateProjectile(EntityID shooter, float startX, float startY, 
                                   float directionX, float directionY, float speed, EntityID targetEntity) {
    using namespace Components;
    
    EntityID projectile = mRegistry.CreateEntity();
    
    mRegistry.AddComponent<Position>(projectile, {startX, startY});
    mRegistry.AddComponent<Projectile>(projectile, {
        directionX, directionY, speed, PROJECTILE_LIFETIME, shooter, targetEntity, true
    });
    mRegistry.AddComponent<Renderable>(projectile, {
        1.0F, 1.0F, 0.0F, 1.0F, 0.5F // Yellow projectiles
    });
    mRegistry.AddComponent<Collider>(projectile, {0.02F, false});
}

EntityID CombatSystem::FindNearestTarget(EntityID attacker, float maxRange) const {
    using namespace Components;
    
    auto* attackerPos = mRegistry.GetComponent<Position>(attacker);
    auto* attackerShip = mRegistry.GetComponent<Spacecraft>(attacker);
    
    if (!attackerPos || !attackerShip) {
        return INVALID_ENTITY;
    }
    
    EntityID nearestTarget = INVALID_ENTITY;
    float nearestDistance = maxRange;
    
    mRegistry.ForEach<Spacecraft>([&](EntityID entity, const Spacecraft& spacecraft) {
        // Don't target same type or self
        if (spacecraft.type == attackerShip->type || entity == attacker) {
            return;
        }
        
        auto* targetPos = mRegistry.GetComponent<Position>(entity);
        auto* targetHealth = mRegistry.GetComponent<Health>(entity);
        
        if (!targetPos || !targetHealth || !targetHealth->isAlive) {
            return;
        }
        
        float distance = CalculateDistance(
            attackerPos->posX, attackerPos->posY,
            targetPos->posX, targetPos->posY
        );
        
        if (distance < nearestDistance) {
            nearestDistance = distance;
            nearestTarget = entity;
        }
    });
    
    return nearestTarget;
}

EntityID CombatSystem::FindNearestPlanet(EntityID attacker, float maxRange) const {
    using namespace Components;
    
    auto* attackerPos = mRegistry.GetComponent<Position>(attacker);
    auto* attackerShip = mRegistry.GetComponent<Spacecraft>(attacker);
    
    if (!attackerPos || !attackerShip) {
        return INVALID_ENTITY;
    }
    
    EntityID nearestPlanet = INVALID_ENTITY;
    float nearestDistance = maxRange;
    
    mRegistry.ForEach<Planet>([&](EntityID entity, const Planet& planet) {
        // Only target enemy planets (if attacker is enemy, target player planets)
        if (attackerShip->type == SpacecraftType::Enemy && !planet.isPlayerOwned) {
            return; // Enemy should target player planets
        }
        if (attackerShip->type == SpacecraftType::Player && planet.isPlayerOwned) {
            return; // Player should target enemy planets
        }
        
        auto* planetPos = mRegistry.GetComponent<Position>(entity);
        auto* planetHealth = mRegistry.GetComponent<Health>(entity);
        
        if (!planetPos || !planetHealth || !planetHealth->isAlive) {
            return;
        }
        
        float distance = CalculateDistance(
            attackerPos->posX, attackerPos->posY,
            planetPos->posX, planetPos->posY
        );
        
        if (distance < nearestDistance) {
            nearestDistance = distance;
            nearestPlanet = entity;
        }
    });
    
    return nearestPlanet;
}

CombatSystem::TacticalInfo CombatSystem::AnalyzeTacticalSituation(EntityID enemy) const {
    using namespace Components;
    
    TacticalInfo tactical = {};
    
    auto* enemyPos = mRegistry.GetComponent<Position>(enemy);
    if (!enemyPos) {
        return tactical;
    }
    
    // Count nearby ships and find distances
    tactical.nearestPlayerDistance = std::numeric_limits<float>::max();
    tactical.nearestPlanetDistance = std::numeric_limits<float>::max();
    
    // Analyze nearby ships (use unlimited range for omniscient AI)
    mRegistry.ForEach<Spacecraft>([&](EntityID entity, const Spacecraft& spacecraft) {
        if (entity == enemy) return;
        
        auto* pos = mRegistry.GetComponent<Position>(entity);
        auto* health = mRegistry.GetComponent<Health>(entity);
        if (!pos || !health || !health->isAlive) return;
        
        float distance = CalculateDistance(enemyPos->posX, enemyPos->posY, pos->posX, pos->posY);
        
        // Count all units on the map for strategic decisions
        if (spacecraft.type == SpacecraftType::Player) {
            tactical.nearbyPlayerShips++;
            if (distance < tactical.nearestPlayerDistance) {
                tactical.nearestPlayerDistance = distance;
            }
        } else if (spacecraft.type == SpacecraftType::Enemy) {
            tactical.nearbyEnemyShips++;
        }
    });
    
    // Find most vulnerable planet
    tactical.vulnerablePlanet = FindMostVulnerablePlanet(enemy, PLANET_ATTACK_RANGE);
    
    if (tactical.vulnerablePlanet != INVALID_ENTITY) {
        auto* planetPos = mRegistry.GetComponent<Position>(tactical.vulnerablePlanet);
        if (planetPos) {
            tactical.nearestPlanetDistance = CalculateDistance(
                enemyPos->posX, enemyPos->posY, planetPos->posX, planetPos->posY);
        }
    }
    
    // Determine tactical conditions
    tactical.playerOverwhelmed = (tactical.nearbyEnemyShips >= tactical.nearbyPlayerShips * OVERWHELMING_RATIO);
    tactical.safeToAttackPlanet = (tactical.nearbyPlayerShips == 0 || tactical.playerOverwhelmed);
    
    return tactical;
}

EntityID CombatSystem::FindMostVulnerablePlanet(EntityID attacker, float maxRange) const {
    using namespace Components;
    
    auto* attackerPos = mRegistry.GetComponent<Position>(attacker);
    if (!attackerPos) {
        return INVALID_ENTITY;
    }
    
    EntityID mostVulnerable = INVALID_ENTITY;
    float lowestDefense = std::numeric_limits<float>::max();
    
    mRegistry.ForEach<Planet>([&](EntityID entity, const Planet& planet) {
        if (!planet.isPlayerOwned) return; // Only target player planets
        
        auto* planetPos = mRegistry.GetComponent<Position>(entity);
        auto* planetHealth = mRegistry.GetComponent<Health>(entity);
        if (!planetPos || !planetHealth || !planetHealth->isAlive) return;
        
        float planetDistance = CalculateDistance(
            attackerPos->posX, attackerPos->posY, planetPos->posX, planetPos->posY);
        
        if (planetDistance > maxRange) return;
        
        // Calculate defense level (number of nearby player ships)
        int defenseLevel = 0;
        mRegistry.ForEach<Spacecraft>([&](EntityID shipEntity, const Spacecraft& spacecraft) {
            if (spacecraft.type != SpacecraftType::Player) return;
            
            auto* shipPos = mRegistry.GetComponent<Position>(shipEntity);
            auto* shipHealth = mRegistry.GetComponent<Health>(shipEntity);
            if (!shipPos || !shipHealth || !shipHealth->isAlive) return;
            
            float shipToPlanetDistance = CalculateDistance(
                shipPos->posX, shipPos->posY, planetPos->posX, planetPos->posY);
            
            if (shipToPlanetDistance <= VULNERABLE_PLANET_RANGE) {
                defenseLevel++;
            }
        });
        
        // Factor in planet health for vulnerability assessment
        float vulnerability = static_cast<float>(defenseLevel) + 
                            (static_cast<float>(planetHealth->currentHP) / static_cast<float>(planetHealth->maxHP));
        
        if (vulnerability < lowestDefense) {
            lowestDefense = vulnerability;
            mostVulnerable = entity;
        }
    });
    
    return mostVulnerable;
}

EntityID CombatSystem::FindWeakestTarget(EntityID attacker, float maxRange) const {
    using namespace Components;
    
    auto* attackerPos = mRegistry.GetComponent<Position>(attacker);
    if (!attackerPos) {
        return INVALID_ENTITY;
    }
    
    EntityID weakestTarget = INVALID_ENTITY;
    float lowestHealthPercentage = 1.1F; // Start above 100%
    
    mRegistry.ForEach<Spacecraft>([&](EntityID entity, const Spacecraft& spacecraft) {
        if (spacecraft.type != SpacecraftType::Player || entity == attacker) return;
        
        auto* pos = mRegistry.GetComponent<Position>(entity);
        auto* health = mRegistry.GetComponent<Health>(entity);
        if (!pos || !health || !health->isAlive) return;
        
        float distance = CalculateDistance(attackerPos->posX, attackerPos->posY, pos->posX, pos->posY);
        if (distance > maxRange) return;
        
        float healthPercentage = static_cast<float>(health->currentHP) / static_cast<float>(health->maxHP);
        
        if (healthPercentage < lowestHealthPercentage) {
            lowestHealthPercentage = healthPercentage;
            weakestTarget = entity;
        }
    });
    
    return weakestTarget;
}

bool CombatSystem::ShouldRetreat(EntityID enemy, const TacticalInfo& tactical) const {
    using namespace Components;
    
    auto* health = mRegistry.GetComponent<Health>(enemy);
    if (!health) return false;
    
    float healthPercentage = static_cast<float>(health->currentHP) / static_cast<float>(health->maxHP);
    
    // Be more aggressive - only retreat if critically low health AND severely outnumbered
    return (healthPercentage < 0.30F) && // Only retreat when very close to death
           (tactical.nearbyPlayerShips > tactical.nearbyEnemyShips * 3 && tactical.nearbyPlayerShips > 3); // And severely outnumbered
}

bool CombatSystem::ShouldAggressivelyAttackPlanet(EntityID enemy, const TacticalInfo& tactical) const {
    (void)enemy; // Suppress unused parameter warning
    
    // Attack planet aggressively if we outnumber players significantly OR planet is very close and undefended
    return (tactical.playerOverwhelmed && tactical.vulnerablePlanet != INVALID_ENTITY) ||
           (tactical.nearbyPlayerShips == 0 && tactical.nearestPlanetDistance < VULNERABLE_PLANET_RANGE);
}

float CombatSystem::CalculateDistance(float startX, float startY, float endX, float endY) const {
    float deltaX = endX - startX;
    float deltaY = endY - startY;
    return std::sqrt((deltaX * deltaX) + (deltaY * deltaY));
}

void CombatSystem::CalculateDirection(float fromX, float fromY, float toX, float toY, 
                                     float& directionX, float& directionY) const {
    float deltaX = toX - fromX;
    float deltaY = toY - fromY;
    float distance = CalculateDistance(fromX, fromY, toX, toY);
    
    if (distance > 0.0F) {
        directionX = deltaX / distance;
        directionY = deltaY / distance;
    } else {
        directionX = 0.0F;
        directionY = 0.0F;
    }
}

void CombatSystem::ProcessPlayerAutoAttack(float deltaTime) {
    using namespace Components;
    (void)deltaTime; // Suppress unused parameter warning
    
    // Process all player units for auto-attack
    mRegistry.ForEach<Spacecraft>([&](EntityID entity, const Spacecraft& spacecraft) {
        if (spacecraft.type != SpacecraftType::Player) {
            return;
        }
        
        auto* health = mRegistry.GetComponent<Health>(entity);
        if (!health || !health->isAlive) {
            return;
        }
        
        auto* playerPos = mRegistry.GetComponent<Position>(entity);
        if (!playerPos) {
            return;
        }
        
        EntityID target = INVALID_ENTITY;
        
        // Priority 1: Attack the specific target if set and valid
        if (spacecraft.targetEntity != INVALID_ENTITY) {
            auto* targetPos = mRegistry.GetComponent<Position>(spacecraft.targetEntity);
            auto* targetHealth = mRegistry.GetComponent<Health>(spacecraft.targetEntity);
            auto* targetSpacecraft = mRegistry.GetComponent<Spacecraft>(spacecraft.targetEntity);
            
            // Check if target is still valid and alive and is an enemy
            if (targetPos && targetHealth && targetHealth->isAlive && 
                targetSpacecraft && targetSpacecraft->type == SpacecraftType::Enemy) {
                
                float distanceToSpecificTarget = CalculateDistance(
                    playerPos->posX, playerPos->posY, targetPos->posX, targetPos->posY);
                
                if (distanceToSpecificTarget <= AI_FIRING_RANGE) {
                    target = spacecraft.targetEntity;
                }
            }
        }
        
        // Priority 2: If no specific target in range, attack nearest enemy
        if (target == INVALID_ENTITY) {
            target = FindNearestTarget(entity, AI_FIRING_RANGE);
        }
        
        // Fire at the target if we have one
        if (target != INVALID_ENTITY) {
            auto* targetPos = mRegistry.GetComponent<Position>(target);
            auto* playerSpacecraft = mRegistry.GetComponent<Spacecraft>(entity);
            
            if (targetPos && playerSpacecraft) {
                // Calculate angle to target and update ship rotation
                float deltaX = targetPos->posX - playerPos->posX;
                float deltaY = targetPos->posY - playerPos->posY;
                constexpr float RADIANS_TO_DEGREES = 180.0F / 3.14159F;
                constexpr float TRIANGLE_ROTATION_OFFSET = 90.0F; // Triangles point up by default
                float angleToTarget = (std::atan2(deltaY, deltaX) * RADIANS_TO_DEGREES) - TRIANGLE_ROTATION_OFFSET;
                playerSpacecraft->angle = angleToTarget;
                
                // Fire the weapon
                FireWeapon(entity, target, targetPos->posX, targetPos->posY);
            }
        }
    });
}

void CombatSystem::CoordinateGroupTactics(float deltaTime) {
    using namespace Components;
    
    // Update active formations and clean up completed ones
    for (auto it = mActiveFormations.begin(); it != mActiveFormations.end();) {
        it->activationTime += deltaTime;
        
        // Update formation target position if it's moved
        if (it->target != INVALID_ENTITY) {
            Position* targetPos = mRegistry.GetComponent<Position>(it->target);
            if (targetPos) {
                it->formationCenterX = targetPos->posX;
                it->formationCenterY = targetPos->posY;
            }
        }
        
        // Remove formations after 15 seconds or if target is destroyed
        auto* targetHealth = mRegistry.GetComponent<Health>(it->target);
        if (it->activationTime > 15.0F || !targetHealth || !targetHealth->isAlive) {
            // Transition formation members back to individual AI control
            for (EntityID member : it->members) {
                auto* spacecraft = mRegistry.GetComponent<Spacecraft>(member);
                if (spacecraft) {
                    // Force transition back to search to find new objectives
                    spacecraft->aiState = AIState::Search;
                    spacecraft->aiStateTimer = 0.0F;
                    spacecraft->aiTarget = INVALID_ENTITY;
                }
            }
            it = mActiveFormations.erase(it);
        } else {
            ++it;
        }
    }
    
    // Reset formation flags if no active formations
    if (mActiveFormations.empty()) {
        mMassAttackInProgress = false;
        mSurroundInProgress = false;
    }
    
    // Count available enemy units (not already in active formations)
    std::vector<EntityID> availableEnemies;
    std::vector<EntityID> allEnemies = GetAllEnemyUnits();
    
    for (EntityID enemy : allEnemies) {
        bool inFormation = false;
        for (const auto& formation : mActiveFormations) {
            if (std::find(formation.members.begin(), formation.members.end(), enemy) != formation.members.end()) {
                inFormation = true;
                break;
            }
        }
        
        if (!inFormation) {
            auto* spacecraft = mRegistry.GetComponent<Spacecraft>(enemy);
            // Only include units not currently retreating or regrouping for formations
            if (spacecraft && spacecraft->aiState != AIState::Retreat && spacecraft->aiState != AIState::Regroup) {
                availableEnemies.push_back(enemy);
            }
        }
    }
    
    if (availableEnemies.size() < MIN_MASS_ATTACK_SIZE) {
        return; // Not enough available units for coordinated tactics
    }
    
    // Check if we should initiate a mass attack
    if (ShouldInitiateMassAttack() && !mMassAttackInProgress && availableEnemies.size() >= MIN_MASS_ATTACK_SIZE) {
        GroupFormation formation;
        formation.type = GroupFormation::MASS_ATTACK;
        formation.target = SelectStrategicTarget();
        formation.members = availableEnemies;
        formation.isActive = true;
        formation.activationTime = 0.0F;
        formation.leader = availableEnemies[0]; // First unit becomes leader
        
        Position* targetPos = mRegistry.GetComponent<Position>(formation.target);
        if (targetPos) {
            formation.formationCenterX = targetPos->posX;
            formation.formationCenterY = targetPos->posY;
        }
        
        // Transition all formation members to approach state with formation target
        for (EntityID member : formation.members) {
            auto* spacecraft = mRegistry.GetComponent<Spacecraft>(member);
            if (spacecraft) {
                spacecraft->aiState = AIState::Approach;
                spacecraft->aiTarget = formation.target;
                spacecraft->aiStateTimer = 0.0F;
            }
        }
        
        ExecuteMassAttack(formation);
        mActiveFormations.push_back(formation);
        mMassAttackInProgress = true;
        SDL_Log("Initiating mass attack with %zu units on target %u", availableEnemies.size(), formation.target);
    }
    
    // Check if we should initiate a surround maneuver
    EntityID strategicTarget = SelectStrategicTarget();
    if (ShouldInitiateSurroundManeuver(strategicTarget) && !mSurroundInProgress && availableEnemies.size() >= MIN_SURROUND_SIZE) {
        GroupFormation formation;
        formation.type = GroupFormation::SURROUND;
        formation.target = strategicTarget;
        formation.members = availableEnemies;
        formation.isActive = true;
        formation.activationTime = 0.0F;
        formation.leader = availableEnemies[0]; // First unit becomes leader
        
        Position* targetPos = mRegistry.GetComponent<Position>(formation.target);
        if (targetPos) {
            formation.formationCenterX = targetPos->posX;
            formation.formationCenterY = targetPos->posY;
        }
        
        // Transition all formation members to approach state with formation target
        for (EntityID member : formation.members) {
            auto* spacecraft = mRegistry.GetComponent<Spacecraft>(member);
            if (spacecraft) {
                spacecraft->aiState = AIState::Approach;
                spacecraft->aiTarget = formation.target;
                spacecraft->aiStateTimer = 0.0F;
            }
        }
        
        ExecuteSurroundManeuver(formation);
        mActiveFormations.push_back(formation);
        mSurroundInProgress = true;
        SDL_Log("Initiating surround maneuver with %zu units on target %u", availableEnemies.size(), formation.target);
    }
}

bool CombatSystem::ShouldInitiateMassAttack() const {
    // Initiate mass attack if we have tactical advantage
    std::vector<EntityID> enemies = GetAllEnemyUnits();
    if (enemies.empty()) {
        return false;
    }
    
    TacticalInfo tactical = AnalyzeTacticalSituation(enemies[0]);
    // More lenient conditions - attack if we have enough units and any nearby players
    return tactical.nearbyEnemyShips >= MIN_MASS_ATTACK_SIZE && tactical.nearbyPlayerShips > 0;
}

bool CombatSystem::ShouldInitiateSurroundManeuver(EntityID target) const {
    using namespace Components;
    if (target == INVALID_ENTITY) return false;
    
    // Check if target is valuable and vulnerable
    Health* health = mRegistry.GetComponent<Components::Health>(target);
    if (health && health->currentHP < 50) {
        return true; // Vulnerable target - good surround target
    }
    
    return false;
}

EntityID CombatSystem::SelectStrategicTarget() const {
    using namespace Components;
    
    std::vector<EntityID> enemies = GetAllEnemyUnits();
    if (enemies.empty()) {
        return INVALID_ENTITY;
    }
    
    // Count total player forces to inform target selection
    int totalPlayerShips = 0;
    mRegistry.ForEach<Spacecraft>([&](EntityID entity, const Spacecraft& spacecraft) {
        if (spacecraft.type == SpacecraftType::Player) {
            auto* health = mRegistry.GetComponent<Health>(entity);
            if (health && health->isAlive) {
                totalPlayerShips++;
            }
        }
    });
    
    // If player has significant forces (3+), prioritize ships for mass attack
    if (totalPlayerShips >= 3) {
        EntityID playerTarget = FindNearestTarget(enemies[0], MASS_ATTACK_RANGE);
        if (playerTarget != INVALID_ENTITY) {
            return playerTarget;
        }
    }
    
    // Otherwise, look for most vulnerable planet
    EntityID vulnerablePlanet = FindMostVulnerablePlanet(enemies[0], MASS_ATTACK_RANGE);
    if (vulnerablePlanet != INVALID_ENTITY) {
        return vulnerablePlanet;
    }
    
    // Fallback: any player target within range
    return FindNearestTarget(enemies[0], MASS_ATTACK_RANGE);
}

void CombatSystem::ExecuteMassAttack(const GroupFormation& formation) {
    using namespace Components;
    
    // Position units in an aggressive formation toward the target
    for (size_t i = 0; i < formation.members.size(); ++i) {
        Position* pos = mRegistry.GetComponent<Position>(formation.members[i]);
        Spacecraft* spacecraft = mRegistry.GetComponent<Spacecraft>(formation.members[i]);
        if (pos && spacecraft) {
            // Create attack vector towards target with slight spread
            float angle = (static_cast<float>(i) / static_cast<float>(formation.members.size())) * 2.0F * 3.14159F;
            float attackRadius = 0.2F; // Tight formation for mass attack
            
            // Move units towards formation position
            float targetX = formation.formationCenterX + std::cos(angle) * attackRadius;
            float targetY = formation.formationCenterY + std::sin(angle) * attackRadius;
            
            // Set movement destination instead of directly modifying position
            spacecraft->destX = targetX;
            spacecraft->destY = targetY;
            spacecraft->isMoving = true;
            
            // Also allow attacking the formation target
            if (formation.target != INVALID_ENTITY) {
                Position* targetPos = mRegistry.GetComponent<Position>(formation.target);
                if (targetPos) {
                    float distanceToTarget = CalculateDistance(pos->posX, pos->posY, targetPos->posX, targetPos->posY);
                    if (distanceToTarget <= AI_FIRING_RANGE) {
                        FireWeapon(formation.members[i], formation.target, targetPos->posX, targetPos->posY);
                    }
                }
            }
        }
    }
}

void CombatSystem::ExecuteSurroundManeuver(const GroupFormation& formation) {
    using namespace Components;
    
    // Position units in a circle around the target
    for (size_t i = 0; i < formation.members.size(); ++i) {
        Position* pos = mRegistry.GetComponent<Position>(formation.members[i]);
        Spacecraft* spacecraft = mRegistry.GetComponent<Spacecraft>(formation.members[i]);
        if (pos && spacecraft) {
            float angle = (static_cast<float>(i) / static_cast<float>(formation.members.size())) * 2.0F * 3.14159F;
            
            // Calculate target position in surround formation
            float targetX = formation.formationCenterX + std::cos(angle) * SURROUND_RADIUS;
            float targetY = formation.formationCenterY + std::sin(angle) * SURROUND_RADIUS;
            
            // Set movement destination instead of directly modifying position
            spacecraft->destX = targetX;
            spacecraft->destY = targetY;
            spacecraft->isMoving = true;
            
            // Allow attacking the surrounded target
            if (formation.target != INVALID_ENTITY) {
                Position* targetPos = mRegistry.GetComponent<Position>(formation.target);
                if (targetPos) {
                    float distanceToTarget = CalculateDistance(pos->posX, pos->posY, targetPos->posX, targetPos->posY);
                    if (distanceToTarget <= AI_FIRING_RANGE) {
                        FireWeapon(formation.members[i], formation.target, targetPos->posX, targetPos->posY);
                    }
                }
            }
        }
    }
}

void CombatSystem::AssignFormationPositions(GroupFormation& formation) {
    // This method can be used for more complex formation positioning
    // Currently handled in Execute methods
}

std::vector<EntityID> CombatSystem::GetAllEnemyUnits() const {
    using namespace Components;
    std::vector<EntityID> enemies;
    
    mRegistry.ForEach<Spacecraft>([&](EntityID entity, const Spacecraft& spacecraft) {
        if (spacecraft.type == SpacecraftType::Enemy) {
            enemies.push_back(entity);
        }
    });
    
    return enemies;
}

float CombatSystem::CalculateGroupCohesion() const {
    using namespace Components;
    // Calculate how well grouped the enemy units are
    std::vector<EntityID> enemies = GetAllEnemyUnits();
    if (enemies.size() < 2) {
        return 1.0F;
    }
    
    float totalDistance = 0.0F;
    int pairCount = 0;
    
    for (size_t i = 0; i < enemies.size(); ++i) {
        for (size_t j = i + 1; j < enemies.size(); ++j) {
            Components::Position* pos1 = mRegistry.GetComponent<Components::Position>(enemies[i]);
            Components::Position* pos2 = mRegistry.GetComponent<Components::Position>(enemies[j]);
            
            if (pos1 != nullptr && pos2 != nullptr) {
                float dx = pos1->posX - pos2->posX;
                float dy = pos1->posY - pos2->posY;
                totalDistance += std::sqrt((dx * dx) + (dy * dy));
                pairCount++;
            }
        }
    }
    
    if (pairCount == 0) {
        return 1.0F;
    }
    
    float averageDistance = totalDistance / static_cast<float>(pairCount);
    return 1.0F / (1.0F + averageDistance); // Higher cohesion = lower average distance
}

bool CombatSystem::HasHealthyAlliesNearby(EntityID entity, const TacticalInfo& tactical) const {
    using namespace Components;
    
    auto* position = mRegistry.GetComponent<Position>(entity);
    if (!position) return false;
    
    int healthyAlliesNearby = 0;
    const float supportRange = AI_FIRING_RANGE * 2.0F;
    
    mRegistry.ForEach<Spacecraft>([&](EntityID allyEntity, const Spacecraft& spacecraft) {
        if (allyEntity == entity || spacecraft.type != SpacecraftType::Enemy) return;
        
        auto* health = mRegistry.GetComponent<Health>(allyEntity);
        auto* allyPos = mRegistry.GetComponent<Position>(allyEntity);
        if (!health || !allyPos || !health->isAlive) return;
        
        // Check if ally is healthy (more than 60% health)
        float healthPercent = static_cast<float>(health->currentHP) / static_cast<float>(health->maxHP);
        if (healthPercent < 0.6F) return;
        
        // Check if ally is close enough to provide support
        float distance = CalculateDistance(position->posX, position->posY, allyPos->posX, allyPos->posY);
        if (distance <= supportRange) {
            healthyAlliesNearby++;
        }
    });
    
    // Consider we have support if at least 2 healthy allies are nearby
    return healthyAlliesNearby >= 2;
}

EntityID CombatSystem::FindHealthyAllyToHideBehind(EntityID entity, EntityID threat) const {
    using namespace Components;
    
    auto* entityPos = mRegistry.GetComponent<Position>(entity);
    auto* threatPos = mRegistry.GetComponent<Position>(threat);
    if (!entityPos || !threatPos) return INVALID_ENTITY;
    
    EntityID bestAlly = INVALID_ENTITY;
    float bestProtectionScore = 0.0F;
    
    mRegistry.ForEach<Spacecraft>([&](EntityID allyEntity, const Spacecraft& spacecraft) {
        if (allyEntity == entity || spacecraft.type != SpacecraftType::Enemy) return;
        
        auto* health = mRegistry.GetComponent<Health>(allyEntity);
        auto* allyPos = mRegistry.GetComponent<Position>(allyEntity);
        if (!health || !allyPos || !health->isAlive) return;
        
        // Only consider healthy allies (more than 70% health)
        float healthPercent = static_cast<float>(health->currentHP) / static_cast<float>(health->maxHP);
        if (healthPercent < 0.7F) return;
        
        // Check if ally is between us and the threat (roughly)
        float entityToAllyDist = CalculateDistance(entityPos->posX, entityPos->posY, allyPos->posX, allyPos->posY);
        float allyToThreatDist = CalculateDistance(allyPos->posX, allyPos->posY, threatPos->posX, threatPos->posY);
        float entityToThreatDist = CalculateDistance(entityPos->posX, entityPos->posY, threatPos->posX, threatPos->posY);
        
        // Good protection if ally is between us and threat, and close enough to help
        if (allyToThreatDist < entityToThreatDist && entityToAllyDist <= AI_FIRING_RANGE * 1.5F) {
            float protectionScore = healthPercent + (1.0F / (1.0F + entityToAllyDist));
            if (protectionScore > bestProtectionScore) {
                bestProtectionScore = protectionScore;
                bestAlly = allyEntity;
            }
        }
    });
    
    return bestAlly;
}

EntityID CombatSystem::FindBestRegroupTarget(EntityID entity) const {
    using namespace Components;
    
    auto* entityPos = mRegistry.GetComponent<Position>(entity);
    if (!entityPos) return INVALID_ENTITY;
    
    EntityID bestTarget = INVALID_ENTITY;
    float bestScore = 0.0F;
    
    mRegistry.ForEach<Spacecraft>([&](EntityID allyEntity, const Spacecraft& spacecraft) {
        if (allyEntity == entity || spacecraft.type != SpacecraftType::Enemy) return;
        
        auto* health = mRegistry.GetComponent<Health>(allyEntity);
        auto* allyPos = mRegistry.GetComponent<Position>(allyEntity);
        if (!health || !allyPos || !health->isAlive) return;
        
        float healthPercent = static_cast<float>(health->currentHP) / static_cast<float>(health->maxHP);
        float distance = CalculateDistance(entityPos->posX, entityPos->posY, allyPos->posX, allyPos->posY);
        
        // Score based on health and proximity - prefer healthy, nearby allies
        float score = healthPercent * (1.0F / (1.0F + distance));
        
        // Bonus for allies not currently retreating
        if (spacecraft.aiState != Components::AIState::Retreat) {
            score *= 1.5F;
        }
        
        if (score > bestScore) {
            bestScore = score;
            bestTarget = allyEntity;
        }
    });
    
    return bestTarget;
}

bool CombatSystem::CanPickOffWeakTargets(EntityID entity, const TacticalInfo& tactical) const {
    using namespace Components;
    
    auto* entityPos = mRegistry.GetComponent<Position>(entity);
    auto* entityHealth = mRegistry.GetComponent<Health>(entity);
    if (!entityPos || !entityHealth) return false;
    
    float myHealthPercent = static_cast<float>(entityHealth->currentHP) / static_cast<float>(entityHealth->maxHP);
    
    // Only consider picking off targets if we have reasonable health (>30%)
    if (myHealthPercent < 0.3F) return false;
    
    // Look for vulnerable player targets within reasonable range
    const float pickOffRange = AI_FIRING_RANGE * 3.0F;
    int vulnerableTargets = 0;
    int totalPlayerTargets = 0;
    
    mRegistry.ForEach<Spacecraft>([&](EntityID targetEntity, const Spacecraft& spacecraft) {
        if (spacecraft.type != SpacecraftType::Player) return;
        
        auto* targetPos = mRegistry.GetComponent<Position>(targetEntity);
        auto* targetHealth = mRegistry.GetComponent<Health>(targetEntity);
        if (!targetPos || !targetHealth || !targetHealth->isAlive) return;
        
        float distance = CalculateDistance(entityPos->posX, entityPos->posY, targetPos->posX, targetPos->posY);
        if (distance > pickOffRange) return;
        
        totalPlayerTargets++;
        
        // Consider target vulnerable if low health or isolated
        float targetHealthPercent = static_cast<float>(targetHealth->currentHP) / static_cast<float>(targetHealth->maxHP);
        if (targetHealthPercent < 0.4F) {
            vulnerableTargets++;
        }
        
        // Also consider isolated targets as vulnerable
        int nearbyPlayerAllies = 0;
        mRegistry.ForEach<Spacecraft>([&](EntityID allyEntity, const Spacecraft& allySpacecraft) {
            if (allyEntity == targetEntity || allySpacecraft.type != SpacecraftType::Player) return;
            
            auto* allyPos = mRegistry.GetComponent<Position>(allyEntity);
            auto* allyHealth = mRegistry.GetComponent<Health>(allyEntity);
            if (!allyPos || !allyHealth || !allyHealth->isAlive) return;
            
            float allyDistance = CalculateDistance(targetPos->posX, targetPos->posY, allyPos->posX, allyPos->posY);
            if (allyDistance <= AI_FIRING_RANGE * 1.5F) {
                nearbyPlayerAllies++;
            }
        });
        
        // Target is vulnerable if isolated (no nearby allies)
        if (nearbyPlayerAllies == 0) {
            vulnerableTargets++;
        }
    });
    
    // Pick off targets if:
    // 1. There are few total player units (≤3) and we found some vulnerable ones
    // 2. Most of the nearby player targets are vulnerable (≥50%)
    // 3. Player forces are very thin (≤2 total units nearby)
    return (totalPlayerTargets <= 3 && vulnerableTargets > 0) ||
           (totalPlayerTargets > 0 && vulnerableTargets >= totalPlayerTargets / 2) ||
           (totalPlayerTargets <= 2);
}

void CombatSystem::ApplyScreenBoundaries(float& posX, float& posY) const {
    // Clamp position to screen boundaries with small margin
    if (posX < SCREEN_BOUNDARY_MIN) {
        posX = SCREEN_BOUNDARY_MIN;
    } else if (posX > SCREEN_BOUNDARY_MAX) {
        posX = SCREEN_BOUNDARY_MAX;
    }
    
    if (posY < SCREEN_BOUNDARY_MIN) {
        posY = SCREEN_BOUNDARY_MIN;
    } else if (posY > SCREEN_BOUNDARY_MAX) {
        posY = SCREEN_BOUNDARY_MAX;
    }
}

void CombatSystem::SetRetreatDestination(Components::Spacecraft& spacecraft, float currentX, float currentY, float targetX, float targetY) {
    // Calculate distance to target destination
    float distance = CalculateDistance(currentX, currentY, targetX, targetY);
    
    // Only update destination if it's significantly different to reduce oscillation
    const float minMoveDistance = AI_FIRING_RANGE * 0.1F; // Minimum distance to trigger movement
    
    if (distance > minMoveDistance) {
        spacecraft.destX = targetX;
        spacecraft.destY = targetY;
        spacecraft.isMoving = true;
    }
}
