#pragma once

#include "../core/ECSRegistry.h"
#include "../core/SystemBase.h"

// Forward declarations
class AudioManager;

/**
 * @brief System for handling combat mechanicsinclude "../core/ECSRegistry.h"
#include "../core/SystemBase.h"

// Forward declarations
class AudioManager;a once

#include "../core/ECSRegistry.h"

// Forward declaration
class AudioManager;

/**
 * @brief System for handling combat mechanics, shooting, and weapon systems
 */
class CombatSystem : public SystemBase {
public:
    explicit CombatSystem(ECSRegistry& registry);
    ~CombatSystem() override;

    // Set audio manager for sound effects
    void SetAudioManager(AudioManager* audioManager) { mAudioManager = audioManager; }

    // SystemBase interface
    bool Initialize() override;
    void Update(float deltaTime) override;
    void Shutdown() override;

    // Combat actions
    void FireWeapon(EntityID shooter, float targetX, float targetY);
    void ProcessAutomaticFiring(float deltaTime);

private:
    // Combat logic
    void UpdateWeaponCooldowns(float deltaTime);
    void ProcessEnemyAI(float deltaTime);
    void ProcessPlayerAutoAttack(float deltaTime);
    void CreateProjectile(EntityID shooter, float startX, float startY, 
                         float dirX, float dirY, float speed = 2.0F);
    
    // AI helpers
    EntityID FindNearestTarget(EntityID attacker, float maxRange) const;
    float CalculateDistance(float x1, float y1, float x2, float y2) const;
    void CalculateDirection(float fromX, float fromY, float toX, float toY, 
                           float& dirX, float& dirY) const;

    // Combat constants
    static constexpr float WEAPON_COOLDOWN = 1.0F; // seconds between shots
    static constexpr float PROJECTILE_SPEED = 1.0F;
    static constexpr float PROJECTILE_LIFETIME = 1.5F; // Reduced for shorter range
    static constexpr float AI_FIRING_RANGE = 0.5F; // Reduced to match projectile range
    static constexpr float AI_UPDATE_INTERVAL = 0.1F; // AI decision making frequency
    
    // Combat state
    float mAIUpdateTimer;
    
    // Audio integration
    AudioManager* mAudioManager;
};
