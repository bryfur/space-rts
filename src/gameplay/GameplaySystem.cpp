#include "GameplaySystem.h"
#include "../components/Components.h"
#include "../core/GameStateManager.h"
#include <SDL_log.h>
#include <cstdlib>
#include <cmath>

GameplaySystem::GameplaySystem(ECSRegistry& registry)
    : SystemBase(registry)
    , mSurvivalTime(0.0F)
    , mEnemySpawnTimer(INITIAL_SPAWN_INTERVAL)
    , mEnemySpawnInterval(INITIAL_SPAWN_INTERVAL)
    , mEnemyWaveCount(0)
    , mGameOverTriggered(false)
    , mGameStateManager(nullptr)
{
}

GameplaySystem::~GameplaySystem() {
    Shutdown();
}

void GameplaySystem::SetGameStateManager(GameStateManager* gameStateManager) {
    mGameStateManager = gameStateManager;
}

void GameplaySystem::ResetGameState() {
    mGameOverTriggered = false;
    SDL_Log("GameplaySystem: Game state reset for new game");
}

bool GameplaySystem::Initialize() {
    using namespace Components;
    
    // Create initial game entities
    
    // Create planets
    EntityID planet1 = mRegistry.CreateEntity();
    mRegistry.AddComponent<Position>(planet1, {-0.5F, 0.0F});
    mRegistry.AddComponent<Planet>(planet1, {0.15F, {}, true}); // Player-owned planet
    mRegistry.AddComponent<Health>(planet1, {100, 100, true}); // 100 HP for planet
    mRegistry.AddComponent<Selectable>(planet1, {false, 0.15F});
    mRegistry.AddComponent<Renderable>(planet1, {0.2F, 0.6F, 1.0F, 1.0F, 1.0F});

    EntityID planet2 = mRegistry.CreateEntity();
    mRegistry.AddComponent<Position>(planet2, {0.5F, 0.3F});
    mRegistry.AddComponent<Planet>(planet2, {0.10F, {}, false}); // Enemy planet
    mRegistry.AddComponent<Health>(planet2, {100, 100, true}); // 100 HP for enemy planet
    mRegistry.AddComponent<Selectable>(planet2, {false, 0.10F});
    mRegistry.AddComponent<Renderable>(planet2, {0.2F, 0.6F, 1.0F, 1.0F, 1.0F});

    // Create initial player ships
    EntityID playerShip1 = mRegistry.CreateEntity();
    mRegistry.AddComponent<Position>(playerShip1, {0.0F, -0.4F});
    mRegistry.AddComponent<Spacecraft>(playerShip1, {SpacecraftType::Player, 0.0F, 0.0F, 0.0F, false, false, 0.0F});
    mRegistry.AddComponent<Health>(playerShip1, {10, 10, true});
    mRegistry.AddComponent<Selectable>(playerShip1, {false, 0.04F}); // Original size
    mRegistry.AddComponent<Renderable>(playerShip1, {1.0F, 0.8F, 0.2F, 1.0F, 1.0F});

    EntityID playerShip2 = mRegistry.CreateEntity();
    mRegistry.AddComponent<Position>(playerShip2, {0.2F, 0.2F});
    mRegistry.AddComponent<Spacecraft>(playerShip2, {SpacecraftType::Player, 45.0F, 0.0F, 0.0F, false, false, 0.0F});
    mRegistry.AddComponent<Health>(playerShip2, {10, 10, true});
    mRegistry.AddComponent<Selectable>(playerShip2, {false, 0.04F}); // Original size
    mRegistry.AddComponent<Renderable>(playerShip2, {1.0F, 0.8F, 0.2F, 1.0F, 1.0F});

    // Create initial enemy
    EntityID enemyShip = mRegistry.CreateEntity();
    mRegistry.AddComponent<Position>(enemyShip, {-0.3F, 0.3F});
    mRegistry.AddComponent<Spacecraft>(enemyShip, {SpacecraftType::Enemy, 0.0F, 0.0F, 0.0F, false, false, 0.0F});
    mRegistry.AddComponent<Health>(enemyShip, {10, 10, true});
    mRegistry.AddComponent<Selectable>(enemyShip, {false, 0.04F}); // Original size
    mRegistry.AddComponent<Renderable>(enemyShip, {1.0F, 0.2F, 0.2F, 1.0F, 1.0F});

    SDL_Log("Gameplay manager initialized with initial entities");
    return true;
}

void GameplaySystem::Update(float deltaTime) {
    mSurvivalTime += deltaTime;
    
    // Update planet states
    UpdatePlanetStates();
    
    // Check for game over condition
    CheckGameOverCondition();
    
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
        
        // Spawn enemies outside the screen boundaries
        float angle = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX) * 2.0F * 3.14159F;
        
        // Screen coordinates are from -1.0 to 1.0 for X and -0.75 to 0.75 for Y (due to aspect ratio)
        // Spawn outside these boundaries with some margin
        constexpr float SPAWN_MARGIN = 0.2F; // Distance outside screen
        constexpr float SCREEN_X_BOUND = 1.0F + SPAWN_MARGIN; // Beyond -1.0 to 1.0 range
        constexpr float SCREEN_Y_BOUND = 0.75F + SPAWN_MARGIN; // Beyond -0.75 to 0.75 range
        
        float spawnX = SCREEN_X_BOUND * std::cos(angle);
        float spawnY = SCREEN_Y_BOUND * std::sin(angle);
        
        mRegistry.AddComponent<Position>(enemy, {spawnX, spawnY});
        mRegistry.AddComponent<Spacecraft>(enemy, {SpacecraftType::Enemy, 0.0F, 0.0F, 0.0F, false, false, 0.0F});
        mRegistry.AddComponent<Health>(enemy, {10, 10, true});
        mRegistry.AddComponent<Selectable>(enemy, {false, 0.04F}); // Original size
        mRegistry.AddComponent<Renderable>(enemy, {1.0F, 0.2F, 0.2F, 1.0F, 1.0F});
    }
    
    SDL_Log("Spawned wave %d: %d enemies (next spawn in %.1fs)", 
            mEnemyWaveCount + 1, enemiesToSpawn, mEnemySpawnInterval);
}

void GameplaySystem::UpdatePlanetStates() {
    using namespace Components;
    
    // Update planet visuals based on health
    mRegistry.ForEach<Planet>([&](EntityID entity, Planet& planet) {
        auto* health = mRegistry.GetComponent<Health>(entity);
        auto* renderable = mRegistry.GetComponent<Renderable>(entity);
        
        if (!health || !renderable) {
            return;
        }
        
        if (!health->isAlive) {
            // Planet is destroyed - make it red and clear build queue
            renderable->red = 1.0F;
            renderable->green = 0.0F;
            renderable->blue = 0.0F;
            
            // Clear build queue when planet is destroyed
            if (!planet.buildQueue.empty()) {
                SDL_Log("Planet %u destroyed! Clearing build queue of %zu items", 
                        entity, planet.buildQueue.size());
                planet.buildQueue.clear();
            }
        } else {
            // Planet is alive - keep normal color (blue-ish)
            renderable->red = 0.2F;
            renderable->green = 0.6F;
            renderable->blue = 1.0F;
        }
    });
}

void GameplaySystem::CheckGameOverCondition() {
    using namespace Components;
    
    // Skip if game over already triggered
    if (mGameOverTriggered) {
        return;
    }
    
    // Check if player has any living planets
    bool hasLivingPlayerPlanet = false;
    
    mRegistry.ForEach<Planet>([&](EntityID entity, const Planet& planet) {
        if (planet.isPlayerOwned) {
            auto* health = mRegistry.GetComponent<Health>(entity);
            if (health && health->isAlive) {
                hasLivingPlayerPlanet = true;
            }
        }
    });
    
    // Trigger game over if no living player planets
    if (!hasLivingPlayerPlanet) {
        SDL_Log("GAME OVER! All player planets destroyed!");
        if (mGameStateManager != nullptr) {
            mGameStateManager->EndGame(false); // false = defeat
            mGameOverTriggered = true;
        }
    }
}
