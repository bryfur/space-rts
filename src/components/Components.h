#pragma once

#include <cstdint>
#include <vector>
#include "../core/ECSRegistry.h"

namespace Components {

/**
 * @brief Component for entity position in world space
 */
struct Position {
    float posX = 0.0F;
    float posY = 0.0F;
};

/**
 * @brief Component for entity velocity
 */
struct Velocity {
    float velX = 0.0F;
    float velY = 0.0F;
};

/**
 * @brief Health component for entities that can take damage
 */
struct Health {
    std::int32_t currentHP = 10;
    std::int32_t maxHP = 10;
    bool isAlive = true;
};

/**
 * @brief Spacecraft type enumeration
 */
enum class SpacecraftType : std::uint8_t {
    Player,
    Enemy
};

/**
 * @brief AI State enumeration for enemy spacecraft
 */
enum class AIState : std::uint8_t {
    Search,      // Looking for targets or moving to strategic positions
    Approach,    // Moving toward a target
    Engage,      // In combat range, firing at targets
    Retreat,     // Withdrawing due to low health or being outnumbered
    Regroup      // Moving to rally point to join other units
};

/**
 * @brief Component for spacecraft entities
 */
struct Spacecraft {
    SpacecraftType type = SpacecraftType::Player;
    float angle = 0.0F;
    float destX = 0.0F;
    float destY = 0.0F;
    bool isMoving = false;
    bool isAttacking = false;
    float lastShotTime = 0.0F;
    EntityID targetEntity = INVALID_ENTITY; // Target entity to pursue and attack
    
    // AI state machine for enemy units
    AIState aiState = AIState::Search;
    float aiStateTimer = 0.0F; // Time spent in current state
    EntityID aiTarget = INVALID_ENTITY; // Current AI target
};

/**
 * @brief Unit types that can be built
 */
enum class BuildableUnit : std::uint8_t {
    Spacecraft
};

/**
 * @brief Build queue entry
 */
struct BuildQueueEntry {
    BuildableUnit unitType = BuildableUnit::Spacecraft;
    float timeRemaining = 0.0F;
    float totalBuildTime = 0.0F;
};

/**
 * @brief Component for planet entities
 */
struct Planet {
    float radius = 0.1F;
    std::vector<BuildQueueEntry> buildQueue;
    bool isPlayerOwned = false;
    static constexpr float SPACECRAFT_BUILD_TIME = 5.0F;
};

/**
 * @brief Component for projectile entities
 */
struct Projectile {
    float directionX = 0.0F;
    float directionY = 0.0F;
    float speed = 2.0F;
    float lifetime = 3.0F;
    std::uint32_t ownerId = 0;
    std::uint32_t targetId = INVALID_ENTITY; // Specific target entity ID
    bool isActive = true;
};

/**
 * @brief Component for entities that can be selected
 */
struct Selectable {
    bool isSelected = false;
    float selectionRadius = 0.03F; // Visual selection circle
};

/**
 * @brief Component for visual representation
 */
struct Renderable {
    float red = 1.0F;
    float green = 1.0F;
    float blue = 1.0F;
    float alpha = 1.0F;
    float scale = 1.0F;
};

/**
 * @brief Component for collision detection
 */
struct Collider {
    float radius = 0.02F;
    bool isTrigger = false;
};

} // namespace Components
