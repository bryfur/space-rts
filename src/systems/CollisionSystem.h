#pragma once

#include "../core/ECSRegistry.h"
#include "../core/SystemBase.h"

// Forward declaration
class AudioManager;

/**
 * @brief System for handling collision detection and response
 */
class CollisionSystem : public SystemBase {
public:
    explicit CollisionSystem(ECSRegistry& registry);
    ~CollisionSystem() override;

    // Set audio manager for sound effects
    void SetAudioManager(AudioManager* audioManager) { mAudioManager = audioManager; }

    // SystemBase interface
    bool Initialize() override;
    void Update(float deltaTime) override;
    void Shutdown() override;

private:
    // Collision detection
    void CheckProjectileCollisions();
    void CheckShipCollisions();
    void CheckPlanetCollisions();
    
    // Helper functions
    bool CheckCircleCollision(float x1, float y1, float radius1, 
                             float x2, float y2, float radius2) const;
    
    // Handle collision responses
    void HandleProjectileHit(EntityID projectile, EntityID target);
    void HandleShipCollision(EntityID ship1, EntityID ship2);
    
    // Constants
    static constexpr float SHIP_COLLISION_RADIUS = 0.04F;
    static constexpr float PROJECTILE_COLLISION_RADIUS = 0.02F;
    static constexpr float PLANET_COLLISION_RADIUS = 0.15F;
    
    // Audio integration
    AudioManager* mAudioManager;

};
