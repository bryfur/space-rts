#pragma once

#include "../core/ECSRegistry.h"
#include "../core/SystemBase.h"

// Forward declaration
class GameStateManager;

/**
 * @brief Gameplay system
 */
class GameplaySystem : public SystemBase {
public:
    explicit GameplaySystem(ECSRegistry& registry);
    ~GameplaySystem() override;
    
    // Set the game state manager
    void SetGameStateManager(GameStateManager* gameStateManager);
    
    // Game state reset for new games
    void ResetGameState();

    // SystemBase interface
    bool Initialize() override;
    void Update(float deltaTime) override;
    void Shutdown() override;

private:
    // Constants
    static constexpr float INITIAL_SPAWN_INTERVAL = 15.0F; // seconds (increased from 10.0F)
    static constexpr float MIN_SPAWN_INTERVAL = 4.0F; // increased from 2.0F
    static constexpr float SPAWN_INTERVAL_DECREASE = 0.9F; // slower decrease (was 0.85F)
    
    // Private methods
    void SpawnEnemyWave();
    void UpdatePlanetStates();
    void CheckGameOverCondition();
    
    // Game state
    float mSurvivalTime;
    float mEnemySpawnTimer;
    float mEnemySpawnInterval;
    int mEnemyWaveCount;
    bool mGameOverTriggered;
    
    // Game state manager
    GameStateManager* mGameStateManager;
};
