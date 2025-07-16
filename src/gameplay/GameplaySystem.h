#pragma once

#include "../core/ECSRegistry.h"
#include "../core/SystemBase.h"

/**
 * @brief Gameplay system
 */
class GameplaySystem : public SystemBase {
public:
    explicit GameplaySystem(ECSRegistry& registry);
    ~GameplaySystem() override;

    // SystemBase interface
    bool Initialize() override;
    void Update(float deltaTime) override;
    void Shutdown() override;

private:
    // Constants
    static constexpr float INITIAL_SPAWN_INTERVAL = 10.0F; // seconds
    static constexpr float MIN_SPAWN_INTERVAL = 2.0F;
    static constexpr float SPAWN_INTERVAL_DECREASE = 0.85F;
    
    // Private methods
    void SpawnEnemyWave();
    
    // Game state
    float mSurvivalTime;
    float mEnemySpawnTimer;
    float mEnemySpawnInterval;
    int mEnemyWaveCount;
};
