#include "CombatSystem.h"
#include "../components/Components.h"
#include "../rendering/AudioManager.h"
#include <SDL2/SDL_log.h>
#include <cmath>
#include <algorithm>

CombatSystem::CombatSystem(ECSRegistry& registry)
    : SystemBase(registry)
    , mAIUpdateTimer(0.0F)
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

void CombatSystem::FireWeapon(EntityID shooter, float targetX, float targetY) {
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
    
    // Create projectile
    CreateProjectile(shooter, position->posX, position->posY, dirX, dirY);
    
    // Play weapon fire sound effect
    if (mAudioManager) {
        mAudioManager->PlayPew();
    }
    
    // Set weapon cooldown
    spacecraft->lastShotTime = WEAPON_COOLDOWN;
    
    SDL_Log("Weapon fired by entity %u", shooter);
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
    
    mAIUpdateTimer -= deltaTime;
    if (mAIUpdateTimer > 0.0F) {
        return; // Not time for AI update yet
    }
    
    mAIUpdateTimer = AI_UPDATE_INTERVAL;
    
    // Process enemy AI
    mRegistry.ForEach<Spacecraft>([&](EntityID entity, const Spacecraft& spacecraft) {
        if (spacecraft.type != SpacecraftType::Enemy) {
            return;
        }
        
        auto* health = mRegistry.GetComponent<Health>(entity);
        if (!health || !health->isAlive) {
            return;
        }
        
        // Find nearest player target
        EntityID target = FindNearestTarget(entity, AI_FIRING_RANGE);
        if (target != INVALID_ENTITY) {
            auto* enemyPos = mRegistry.GetComponent<Position>(entity);
            auto* targetPos = mRegistry.GetComponent<Position>(target);
            
            if (enemyPos && targetPos) {
                // Fire at the target
                FireWeapon(entity, targetPos->posX, targetPos->posY);
            }
        }
    });
}

void CombatSystem::CreateProjectile(EntityID shooter, float startX, float startY, 
                                   float directionX, float directionY, float speed) {
    using namespace Components;
    
    EntityID projectile = mRegistry.CreateEntity();
    
    mRegistry.AddComponent<Position>(projectile, {startX, startY});
    mRegistry.AddComponent<Projectile>(projectile, {
        directionX, directionY, speed, PROJECTILE_LIFETIME, shooter, true
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
    
    // Process player units that are set to attack
    mRegistry.ForEach<Spacecraft>([&](EntityID entity, const Spacecraft& spacecraft) {
        if (spacecraft.type != SpacecraftType::Player) {
            return;
        }
        
        auto* health = mRegistry.GetComponent<Health>(entity);
        if (!health || !health->isAlive) {
            return;
        }
        
        // Only auto-attack if unit is set to attacking mode or not moving
        if (!spacecraft.isAttacking && spacecraft.isMoving) {
            return;
        }
        
        // Find nearest enemy target within range
        EntityID target = FindNearestTarget(entity, AI_FIRING_RANGE);
        if (target != INVALID_ENTITY) {
            auto* playerPos = mRegistry.GetComponent<Position>(entity);
            auto* targetPos = mRegistry.GetComponent<Position>(target);
            
            if (playerPos && targetPos) {
                // Check if enemy is actually an enemy type
                auto* enemySpacecraft = mRegistry.GetComponent<Spacecraft>(target);
                if (enemySpacecraft && enemySpacecraft->type == SpacecraftType::Enemy) {
                    // Fire at the target
                    FireWeapon(entity, targetPos->posX, targetPos->posY);
                }
            }
        }
    });
}
