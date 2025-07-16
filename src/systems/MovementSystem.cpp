#include "MovementSystem.h"
#include "../components/Components.h"
#include <SDL2/SDL_log.h>
#include <cmath>

MovementSystem::MovementSystem(ECSRegistry& registry)
    : SystemBase(registry)
{
}

MovementSystem::~MovementSystem() {
    Shutdown();
}

bool MovementSystem::Initialize() {
    SDL_Log("Movement system initialized");
    return true;
}

void MovementSystem::Update(float deltaTime) {
    UpdateSpacecraftMovement(deltaTime);
    UpdateProjectileMovement(deltaTime);
}

void MovementSystem::Shutdown() {
    SDL_Log("Movement system shutdown");
}

void MovementSystem::UpdateSpacecraftMovement(float deltaTime) {
    using namespace Components;
    
    // First pass: Apply separation forces to prevent ships from stacking
    ApplySeparationForces(deltaTime);
    
    // Second pass: Update spacecraft movement and rotation
    mRegistry.ForEach<Spacecraft>([&](EntityID entity, Spacecraft& spacecraft) {
        auto* position = mRegistry.GetComponent<Position>(entity);
        auto* health = mRegistry.GetComponent<Health>(entity);
        if (!position || !health || !health->isAlive) {
            return;
        }
        
        // Store previous position to calculate movement direction
        float prevX = position->posX;
        float prevY = position->posY;
        
        if (spacecraft.type == SpacecraftType::Player && spacecraft.isMoving) {
            // Player spacecraft movement
            float deltaX = spacecraft.destX - position->posX;
            float deltaY = spacecraft.destY - position->posY;
            float distance = CalculateDistance(position->posX, position->posY, spacecraft.destX, spacecraft.destY);
            
            // Check if we've arrived
            if (distance < ARRIVAL_THRESHOLD) {
                spacecraft.isMoving = false;
                return;
            }
            
            // Normalize direction
            float dirX = deltaX / distance;
            float dirY = deltaY / distance;
            
            // Update position
            position->posX += dirX * SHIP_SPEED * deltaTime;
            position->posY += dirY * SHIP_SPEED * deltaTime;
            
            // Update angle to face movement direction (convert radians to degrees, adjust for triangle pointing up)
            spacecraft.angle = (std::atan2(dirY, dirX) * 180.0F / 3.14159F) - 90.0F;
        } 
        else if (spacecraft.type == SpacecraftType::Enemy) {
            // Enemy AI: chase and attack nearest player unit
            EntityID nearestPlayer = FindNearestPlayerUnit(entity);
            if (nearestPlayer != INVALID_ENTITY) {
                auto* targetPos = mRegistry.GetComponent<Position>(nearestPlayer);
                if (targetPos) {
                    float deltaX = targetPos->posX - position->posX;
                    float deltaY = targetPos->posY - position->posY;
                    float distance = std::sqrt((deltaX * deltaX) + (deltaY * deltaY));
                    
                    constexpr float ATTACK_RANGE = 0.12F;
                    constexpr float CHASE_SPEED = SHIP_SPEED * 0.8F; // Slightly slower than player ships
                    
                    if (distance > ATTACK_RANGE) {
                        // Chase the player
                        float dirX = deltaX / distance;
                        float dirY = deltaY / distance;
                        
                        position->posX += dirX * CHASE_SPEED * deltaTime;
                        position->posY += dirY * CHASE_SPEED * deltaTime;
                        
                        // Point toward target
                        spacecraft.angle = (std::atan2(dirY, dirX) * 180.0F / 3.14159F) - 90.0F;
                    } else {
                        // In range - stop and face target for firing
                        spacecraft.angle = (std::atan2(deltaY, deltaX) * 180.0F / 3.14159F) - 90.0F;
                    }
                }
            }
        }
        
        // For all ships: if they moved this frame, update their angle to face movement direction
        float movementX = position->posX - prevX;
        float movementY = position->posY - prevY;
        float movementDistance = std::sqrt((movementX * movementX) + (movementY * movementY));
        
        if (movementDistance > 0.001F && spacecraft.type == SpacecraftType::Player && !spacecraft.isMoving) {
            // Only update angle for player ships that aren't currently moving (to handle separation forces)
            spacecraft.angle = (std::atan2(movementY, movementX) * 180.0F / 3.14159F) - 90.0F;
        }
    });
}

void MovementSystem::UpdateProjectileMovement(float deltaTime) {
    using namespace Components;
    
    // Collect entities to destroy to avoid iterator invalidation
    std::vector<EntityID> entitiesToDestroy;
    
    mRegistry.ForEach<Projectile>([&](EntityID entity, Projectile& projectile) {
        auto* position = mRegistry.GetComponent<Position>(entity);
        if (!position) {
            return;
        }
        
        if (projectile.isActive) {
            // Update position based on direction
            position->posX += projectile.directionX * projectile.speed * deltaTime;
            position->posY += projectile.directionY * projectile.speed * deltaTime;
            
            // Update lifetime
            projectile.lifetime -= deltaTime;
            if (projectile.lifetime <= 0.0F) {
                projectile.isActive = false;
                entitiesToDestroy.push_back(entity);
            }
        }
    });
    
    // Safely destroy entities after iteration
    for (EntityID entity : entitiesToDestroy) {
        mRegistry.DestroyEntity(entity);
    }
}

void MovementSystem::ApplySeparationForces(float deltaTime) {
    using namespace Components;
    
    // Create a map to store all spacecraft for efficient iteration
    std::vector<std::pair<EntityID, Position*>> spacecraftPositions;
    std::vector<std::pair<EntityID, Spacecraft*>> spacecraftData;
    
    // Collect all spacecraft and their positions
    mRegistry.ForEach<Spacecraft>([&](EntityID entity, Spacecraft& spacecraft) {
        auto* position = mRegistry.GetComponent<Position>(entity);
        if (position) {
            spacecraftPositions.push_back({entity, position});
            spacecraftData.push_back({entity, &spacecraft});
        }
    });
    
    // Apply separation forces to prevent ships from stacking
    for (size_t i = 0; i < spacecraftPositions.size(); ++i) {
        EntityID id = spacecraftPositions[i].first;
        Position* pos = spacecraftPositions[i].second;
        Spacecraft* spacecraft = spacecraftData[i].second;
        
        // Calculate separation force from nearby ships
        float separationX = 0.0F;
        float separationY = 0.0F;
        int nearbyCount = 0;
        
        constexpr float SEPARATION_RADIUS = 0.05F; // How close ships can get before pushing apart
        constexpr float SEPARATION_STRENGTH = 0.8F; // How strong the push force is
        
        for (size_t j = 0; j < spacecraftPositions.size(); ++j) {
            if (i == j) continue; // Don't separate from self
            
            EntityID otherId = spacecraftPositions[j].first;
            Position* otherPos = spacecraftPositions[j].second;
            Spacecraft* otherSpacecraft = spacecraftData[j].second;
            
            // Only apply separation between ships of the same type to avoid weird enemy behavior
            if (spacecraft->type != otherSpacecraft->type) continue;
            
            float deltaX = pos->posX - otherPos->posX;
            float deltaY = pos->posY - otherPos->posY;
            float distance = std::sqrt((deltaX * deltaX) + (deltaY * deltaY));
            
            if (distance > 0.001F && distance < SEPARATION_RADIUS) { // Avoid division by zero
                // Normalize and scale by inverse distance (closer = stronger push)
                float force = SEPARATION_STRENGTH * (SEPARATION_RADIUS - distance) / SEPARATION_RADIUS;
                separationX += (deltaX / distance) * force * deltaTime;
                separationY += (deltaY / distance) * force * deltaTime;
                nearbyCount++;
            }
        }
        
        // Apply separation force
        if (nearbyCount > 0) {
            pos->posX += separationX;
            pos->posY += separationY;
        }
    }
}

float MovementSystem::CalculateAngleTo(float fromX, float fromY, float toX, float toY) const {
    float deltaX = toX - fromX;
    float deltaY = toY - fromY;
    return std::atan2(deltaY, deltaX);
}

float MovementSystem::CalculateDistance(float startX, float startY, float endX, float endY) const {
    float deltaX = endX - startX;
    float deltaY = endY - startY;
    return std::sqrt(deltaX * deltaX + deltaY * deltaY);
}

EntityID MovementSystem::FindNearestPlayerUnit(EntityID enemyEntity) const {
    using namespace Components;
    
    auto* enemyPos = mRegistry.GetComponent<Position>(enemyEntity);
    if (!enemyPos) {
        return INVALID_ENTITY;
    }
    
    EntityID nearestPlayer = INVALID_ENTITY;
    float nearestDistance = 1000.0F; // Large initial value
    
    mRegistry.ForEach<Spacecraft>([&](EntityID entity, const Spacecraft& spacecraft) {
        if (spacecraft.type != SpacecraftType::Player || entity == enemyEntity) {
            return;
        }
        
        auto* playerPos = mRegistry.GetComponent<Position>(entity);
        auto* playerHealth = mRegistry.GetComponent<Health>(entity);
        
        if (!playerPos || !playerHealth || !playerHealth->isAlive) {
            return;
        }
        
        float distance = CalculateDistance(enemyPos->posX, enemyPos->posY, 
                                         playerPos->posX, playerPos->posY);
        
        if (distance < nearestDistance) {
            nearestDistance = distance;
            nearestPlayer = entity;
        }
    });
    
    return nearestPlayer;
}
