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
            if (spacecraft.targetEntity != INVALID_ENTITY) {
                // Pursuing a specific target entity
                auto* targetPos = mRegistry.GetComponent<Position>(spacecraft.targetEntity);
                auto* targetHealth = mRegistry.GetComponent<Health>(spacecraft.targetEntity);
                
                // Check if target is still valid and alive
                if (!targetPos || !targetHealth || !targetHealth->isAlive) {
                    // Target is dead or invalid, stop pursuing
                    spacecraft.isMoving = false;
                    spacecraft.targetEntity = INVALID_ENTITY;
                    return;
                }
                
                // Calculate distance to target
                float distanceToTarget = CalculateDistance(position->posX, position->posY, targetPos->posX, targetPos->posY);
                constexpr float PURSUIT_RANGE = 0.3F;
                
                if (distanceToTarget <= PURSUIT_RANGE) {
                    // Stop moving, we're close enough to the target
                    spacecraft.isMoving = false;
                    // Face the target
                    float deltaX = targetPos->posX - position->posX;
                    float deltaY = targetPos->posY - position->posY;
                    spacecraft.angle = (std::atan2(deltaY, deltaX) * 180.0F / 3.14159F) - 90.0F;
                    return;
                } else {
                    // Continue pursuing the target
                    float deltaX = targetPos->posX - position->posX;
                    float deltaY = targetPos->posY - position->posY;
                    float distance = distanceToTarget;
                    
                    // Normalize direction
                    float dirX = deltaX / distance;
                    float dirY = deltaY / distance;
                    
                    // Update position
                    position->posX += dirX * SHIP_SPEED * deltaTime;
                    position->posY += dirY * SHIP_SPEED * deltaTime;
                    
                    // Update angle to face movement direction
                    spacecraft.angle = (std::atan2(dirY, dirX) * 180.0F / 3.14159F) - 90.0F;
                }
            } else {
                // Regular movement to a position
                float deltaX = spacecraft.destX - position->posX;
                float deltaY = spacecraft.destY - position->posY;
                float distance = CalculateDistance(position->posX, position->posY, spacecraft.destX, spacecraft.destY);
                
                // Check if we've arrived at destination
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
                
                // Update angle to face movement direction
                spacecraft.angle = (std::atan2(dirY, dirX) * 180.0F / 3.14159F) - 90.0F;
            }
        } 
        else if (spacecraft.type == SpacecraftType::Enemy && spacecraft.isMoving) {
            // AI-controlled movement to specific destination (set by CombatSystem)
            float deltaX = spacecraft.destX - position->posX;
            float deltaY = spacecraft.destY - position->posY;
            float distance = CalculateDistance(position->posX, position->posY, spacecraft.destX, spacecraft.destY);
            
            // Check if we've arrived at destination
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
            
            // Update angle to face movement direction
            spacecraft.angle = (std::atan2(dirY, dirX) * 180.0F / 3.14159F) - 90.0F;
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
