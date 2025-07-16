#include "GameplaySystem.h"
#include "../components/Components.h"
#include <SDL_log.h>
#include <cstdlib>
#include <cmath>

GameplaySystem::GameplaySystem(ECSRegistry& registry)
    : SystemBase(registry)
    , mSurvivalTime(0.0F)
    , mEnemySpawnTimer(INITIAL_SPAWN_INTERVAL)
    , mEnemySpawnInterval(INITIAL_SPAWN_INTERVAL)
    , mEnemyWaveCount(0)
{
}

GameplaySystem::~GameplaySystem() {
    Shutdown();
}

bool GameplaySystem::Initialize() {
    using namespace Components;
    
    // Create initial game entities
    
    // Create planets
    EntityID planet1 = mRegistry.CreateEntity();
    mRegistry.AddComponent<Position>(planet1, {-0.5F, 0.0F});
    mRegistry.AddComponent<Planet>(planet1, {0.15F, {}, true}); // Player-owned planet
    mRegistry.AddComponent<Selectable>(planet1, {false, 0.15F});
    mRegistry.AddComponent<Renderable>(planet1, {0.2F, 0.6F, 1.0F, 1.0F, 1.0F});

    EntityID planet2 = mRegistry.CreateEntity();
    mRegistry.AddComponent<Position>(planet2, {0.5F, 0.3F});
    mRegistry.AddComponent<Planet>(planet2, {0.10F, {}, false}); // Enemy planet
    mRegistry.AddComponent<Selectable>(planet2, {false, 0.10F});
    mRegistry.AddComponent<Renderable>(planet2, {0.2F, 0.6F, 1.0F, 1.0F, 1.0F});

    // Create initial player ships
    EntityID playerShip1 = mRegistry.CreateEntity();
    mRegistry.AddComponent<Position>(playerShip1, {0.0F, -0.4F});
    mRegistry.AddComponent<Spacecraft>(playerShip1, {SpacecraftType::Player, 0.0F, 0.0F, 0.0F, false, false, 0.0F});
    mRegistry.AddComponent<Health>(playerShip1, {10, 10, true});
    mRegistry.AddComponent<Selectable>(playerShip1, {false, 0.04F});
    mRegistry.AddComponent<Renderable>(playerShip1, {1.0F, 0.8F, 0.2F, 1.0F, 1.0F});

    EntityID playerShip2 = mRegistry.CreateEntity();
    mRegistry.AddComponent<Position>(playerShip2, {0.2F, 0.2F});
    mRegistry.AddComponent<Spacecraft>(playerShip2, {SpacecraftType::Player, 45.0F, 0.0F, 0.0F, false, false, 0.0F});
    mRegistry.AddComponent<Health>(playerShip2, {10, 10, true});
    mRegistry.AddComponent<Selectable>(playerShip2, {false, 0.04F});
    mRegistry.AddComponent<Renderable>(playerShip2, {1.0F, 0.8F, 0.2F, 1.0F, 1.0F});

    // Create initial enemy
    EntityID enemyShip = mRegistry.CreateEntity();
    mRegistry.AddComponent<Position>(enemyShip, {-0.3F, 0.3F});
    mRegistry.AddComponent<Spacecraft>(enemyShip, {SpacecraftType::Enemy, 0.0F, 0.0F, 0.0F, false, false, 0.0F});
    mRegistry.AddComponent<Health>(enemyShip, {10, 10, true});
    mRegistry.AddComponent<Selectable>(enemyShip, {false, 0.04F});
    mRegistry.AddComponent<Renderable>(enemyShip, {1.0F, 0.2F, 0.2F, 1.0F, 1.0F});

    SDL_Log("Gameplay manager initialized with initial entities");
    return true;
}

void GameplaySystem::Update(float deltaTime) {
    mSurvivalTime += deltaTime;
    
    // Enemy spawning system
    mEnemySpawnTimer -= deltaTime;
    if (mEnemySpawnTimer <= 0.0F) {
        SpawnEnemyWave();
        
        // Update spawn parameters for next wave
        mEnemyWaveCount++;
        mEnemySpawnInterval = std::max(MIN_SPAWN_INTERVAL, mEnemySpawnInterval * SPAWN_INTERVAL_DECREASE);
        mEnemySpawnTimer = mEnemySpawnInterval;
    }
}

void GameplaySystem::Shutdown() {
    SDL_Log("Gameplay manager shutdown");
}

void GameplaySystem::SpawnEnemyWave() {
    using namespace Components;
    
    // Spawn exponentially increasing number of enemies
    constexpr int WAVE_DIVISOR = 3;
    int enemiesToSpawn = 1 + (mEnemyWaveCount / WAVE_DIVISOR);
    
    for (int i = 0; i < enemiesToSpawn; ++i) {
        EntityID enemy = mRegistry.CreateEntity();
        
        // Spawn enemies around the edge of the map
        float angle = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX) * 2.0F * 3.14159F;
        constexpr float SPAWN_RADIUS = 0.8F;
        constexpr float SPAWN_CENTER = 0.5F;
        float spawnX = SPAWN_CENTER + SPAWN_RADIUS * std::cos(angle);
        float spawnY = SPAWN_CENTER + SPAWN_RADIUS * std::sin(angle);
        
        mRegistry.AddComponent<Position>(enemy, {spawnX, spawnY});
        mRegistry.AddComponent<Spacecraft>(enemy, {SpacecraftType::Enemy, 0.0F, 0.0F, 0.0F, false, false, 0.0F});
        mRegistry.AddComponent<Health>(enemy, {10, 10, true});
        mRegistry.AddComponent<Selectable>(enemy, {false, 0.04F});
        mRegistry.AddComponent<Renderable>(enemy, {1.0F, 0.2F, 0.2F, 1.0F, 1.0F});
    }
    
    SDL_Log("Spawned wave %d: %d enemies (next spawn in %.1fs)", 
            mEnemyWaveCount + 1, enemiesToSpawn, mEnemySpawnInterval);
}
