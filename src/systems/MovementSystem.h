#pragma once

#include "../core/ECSRegistry.h"
#include "../core/SystemBase.h"

/**
 * @brief System for handling entity movement and navigation
 */
class MovementSystem : public SystemBase {
public:
    explicit MovementSystem(ECSRegistry& registry);
    ~MovementSystem() override;

    // SystemBase interface
    bool Initialize() override;
    void Update(float deltaTime) override;
    void Shutdown() override;

private:
    // Movement calculations
    void UpdateSpacecraftMovement(float deltaTime);
    void UpdateProjectileMovement(float deltaTime);
    void ApplySeparationForces(float deltaTime);
    
    // Navigation helpers
    float CalculateAngleTo(float fromX, float fromY, float toX, float toY) const;
    float CalculateDistance(float x1, float y1, float x2, float y2) const;

    // Movement constants
    static constexpr float SHIP_SPEED = 0.5F;
    static constexpr float SHIP_ROTATION_SPEED = 3.0F;
    static constexpr float PROJECTILE_SPEED = 2.0F;
    static constexpr float ARRIVAL_THRESHOLD = 0.05F;
};
