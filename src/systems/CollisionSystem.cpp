#include "CollisionSystem.h"
#include "../components/Components.h"
#include "../rendering/AudioManager.h"
#include <SDL2/SDL_log.h>
#include <cmath>
#include <vector>
#include <utility>

CollisionSystem::CollisionSystem(ECSRegistry& registry)
    : SystemBase(registry)
    , mAudioManager(nullptr)
{
}

CollisionSystem::~CollisionSystem() {
    Shutdown();
}

bool CollisionSystem::Initialize() {
    SDL_Log("Collision system initialized");
    return true;
}

void CollisionSystem::Update(float deltaTime) {
    (void)deltaTime; // Suppress unused parameter warning
    
    CheckProjectileCollisions();
    CheckProjectilePlanetCollisions();
    // Disabled: CheckShipCollisions();
    // Disabled: CheckPlanetCollisions();
}

void CollisionSystem::Shutdown() {
    SDL_Log("Collision system shutdown");
}

void CollisionSystem::CheckProjectileCollisions() {
    using namespace Components;
    
    // Collect projectiles to destroy to avoid iterator invalidation
    std::vector<EntityID> projectilesToDestroy;
    std::vector<std::pair<EntityID, EntityID>> collisions; // projectile, target
    
    // Check projectiles against ships
    mRegistry.ForEach<Projectile>([&](EntityID projectileEntity, const Projectile& projectile) {
        if (!projectile.isActive) {
            return;
        }
        
        auto* projectilePos = mRegistry.GetComponent<Position>(projectileEntity);
        if (!projectilePos) {
            return;
        }
        
        mRegistry.ForEach<Spacecraft>([&](EntityID shipEntity, const Spacecraft& spacecraft) {
            auto* shipPos = mRegistry.GetComponent<Position>(shipEntity);
            if (!shipPos) {
                return;
            }
            
            // Skip dead ships
            auto* shipHealth = mRegistry.GetComponent<Health>(shipEntity);
            if (!shipHealth || !shipHealth->isAlive) {
                return;
            }
            
            // Don't let projectiles hit their owner
            if (projectile.ownerId == shipEntity) {
                return;
            }
            
            // Let projectiles pass through friendly ships - only check collision with enemies
            auto* ownerSpacecraft = mRegistry.GetComponent<Spacecraft>(projectile.ownerId);
            if (ownerSpacecraft && ownerSpacecraft->type == spacecraft.type) {
                return; // Same team, projectile passes through
            }
            
            // Check collision only with enemy ships
            if (CheckCircleCollision(
                projectilePos->posX, projectilePos->posY, PROJECTILE_COLLISION_RADIUS,
                shipPos->posX, shipPos->posY, SHIP_COLLISION_RADIUS)) {
                
                collisions.emplace_back(projectileEntity, shipEntity);
            }
        });
    });
    
    // Process all collisions after iteration is complete
    for (const auto& [projectileEntity, shipEntity] : collisions) {
        HandleProjectileHit(projectileEntity, shipEntity);
    }
}

void CollisionSystem::CheckProjectilePlanetCollisions() {
    using namespace Components;
    
    // Collect projectiles to destroy to avoid iterator invalidation
    std::vector<EntityID> projectilesToDestroy;
    std::vector<std::pair<EntityID, EntityID>> collisions; // projectile, planet
    
    // Check projectiles against planets
    mRegistry.ForEach<Projectile>([&](EntityID projectileEntity, const Projectile& projectile) {
        if (!projectile.isActive) {
            return;
        }
        
        auto* projectilePos = mRegistry.GetComponent<Position>(projectileEntity);
        if (!projectilePos) {
            return;
        }
        
        mRegistry.ForEach<Planet>([&](EntityID planetEntity, const Planet& planet) {
            auto* planetPos = mRegistry.GetComponent<Position>(planetEntity);
            if (!planetPos) {
                return;
            }
            
            // Skip dead planets
            auto* planetHealth = mRegistry.GetComponent<Health>(planetEntity);
            if (!planetHealth || !planetHealth->isAlive) {
                return;
            }
            
            // Don't let projectiles hit their owner's planet
            auto* ownerSpacecraft = mRegistry.GetComponent<Spacecraft>(projectile.ownerId);
            if (ownerSpacecraft && planet.isPlayerOwned == (ownerSpacecraft->type == SpacecraftType::Player)) {
                return; // Same team, projectile passes through
            }
            
            // Only collide with planets if they are the specific target
            // If no specific target (INVALID_ENTITY), projectiles pass through planets
            if (projectile.targetId != planetEntity) {
                return; // Not targeting this planet, projectile passes through
            }
            
            // Check collision with planet
            if (CheckCircleCollision(
                projectilePos->posX, projectilePos->posY, PROJECTILE_COLLISION_RADIUS,
                planetPos->posX, planetPos->posY, planet.radius)) {
                
                collisions.emplace_back(projectileEntity, planetEntity);
            }
        });
    });
    
    // Process all collisions after iteration is complete
    for (const auto& [projectileEntity, planetEntity] : collisions) {
        HandleProjectileHit(projectileEntity, planetEntity);
    }
}

void CollisionSystem::CheckShipCollisions() {
    using namespace Components;
    
    // Get all ships for collision checking
    std::vector<EntityID> ships;
    mRegistry.ForEach<Spacecraft>([&](EntityID entity, const Spacecraft& /*spacecraft*/) {
        ships.push_back(entity);
    });
    
    // Check each ship against every other ship
    for (size_t i = 0; i < ships.size(); ++i) {
        for (size_t j = i + 1; j < ships.size(); ++j) {
            EntityID ship1 = ships[i];
            EntityID ship2 = ships[j];
            
            auto* pos1 = mRegistry.GetComponent<Position>(ship1);
            auto* pos2 = mRegistry.GetComponent<Position>(ship2);
            
            if (!pos1 || !pos2) {
                continue;
            }
            
            if (CheckCircleCollision(
                pos1->posX, pos1->posY, SHIP_COLLISION_RADIUS,
                pos2->posX, pos2->posY, SHIP_COLLISION_RADIUS)) {
                
                HandleShipCollision(ship1, ship2);
            }
        }
    }
}

void CollisionSystem::CheckPlanetCollisions() {
    using namespace Components;
    
    // Check ships against planets
    mRegistry.ForEach<Spacecraft>([&](EntityID shipEntity, const Spacecraft& /*spacecraft*/) {
        auto* shipPos = mRegistry.GetComponent<Position>(shipEntity);
        if (!shipPos) {
            return;
        }
        
        mRegistry.ForEach<Planet>([&](EntityID planetEntity, const Planet& planet) {
            auto* planetPos = mRegistry.GetComponent<Position>(planetEntity);
            if (!planetPos) {
                return;
            }
            
            if (CheckCircleCollision(
                shipPos->posX, shipPos->posY, SHIP_COLLISION_RADIUS,
                planetPos->posX, planetPos->posY, planet.radius)) {
                
                // Handle planet collision (could be landing/docking)
                SDL_Log("Ship collision with planet detected");
            }
        });
    });
}

bool CollisionSystem::CheckCircleCollision(float centerX1, float centerY1, float radius1,
                                         float centerX2, float centerY2, float radius2) const {
    float deltaX = centerX2 - centerX1;
    float deltaY = centerY2 - centerY1;
    float distance = std::sqrt((deltaX * deltaX) + (deltaY * deltaY));
    return distance <= (radius1 + radius2);
}

void CollisionSystem::HandleProjectileHit(EntityID projectile, EntityID target) {
    using namespace Components;
    
    // Damage the target
    auto* health = mRegistry.GetComponent<Health>(target);
    if (health && health->isAlive) {
        health->currentHP -= 1;
        if (health->currentHP <= 0) {
            health->isAlive = false;
            SDL_Log("Entity destroyed by projectile");
            
            // Play explosion sound effect
            if (mAudioManager) {
                mAudioManager->PlayBoom();
            }
        }
    }
    
    // Remove the projectile
    mRegistry.DestroyEntity(projectile);
}

void CollisionSystem::HandleShipCollision(EntityID ship1, EntityID ship2) {
    using namespace Components;
    
    // Simple collision response - damage both ships
    auto* health1 = mRegistry.GetComponent<Health>(ship1);
    auto* health2 = mRegistry.GetComponent<Health>(ship2);
    
    if (health1 && health1->isAlive) {
        health1->currentHP -= 1;
        if (health1->currentHP <= 0) {
            health1->isAlive = false;
        }
    }
    
    if (health2 && health2->isAlive) {
        health2->currentHP -= 1;
        if (health2->currentHP <= 0) {
            health2->isAlive = false;
        }
    }
    
    SDL_Log("Ship collision detected - both ships damaged");
}
