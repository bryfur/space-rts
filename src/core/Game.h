#pragma once

#include <SDL2/SDL.h>
#include <memory>
#include <cstdint>

// Forward declarations
class ECSRegistry;
class RenderSystem;
class MovementSystem;
class CollisionSystem;
class CombatSystem;
class GameStateManager;
class InputSystem;
class AudioManager;
class GameplaySystem;
class UISystem;

namespace Core {

/**
 * @brief Main game class that orchestrates all game systems
 * 
 * This class follows the composition pattern and manages the game loop,
 * system initialization, and coordinates between different subsystems.
 */
class Game {
public:
    Game();
    ~Game();

    // Non-copyable
    Game(const Game&) = delete;
    Game& operator=(const Game&) = delete;

    /**
     * @brief Initialize the game engine and all subsystems
     * @return true if initialization succeeded, false otherwise
     */
    bool Initialize();

    /**
     * @brief Run the main game loop
     */
    void Run();

    /**
     * @brief Clean shutdown of all systems
     */
    void Shutdown();

    /**
     * @brief Check if the game should continue running
     * @return true if game should continue, false to exit
     */
    bool IsRunning() const { return mRunning; }

    /**
     * @brief Request game shutdown
     */
    void RequestShutdown() { mRunning = false; }

    // Getters for subsystems
    ECSRegistry& GetECS() { return *mECS; }
    RenderSystem& GetRenderer() { return *mRenderer; }
    MovementSystem& GetMovementSystem() { return *mMovementSystem; }
    CollisionSystem& GetCollisionSystem() { return *mCollisionSystem; }
    CombatSystem& GetCombatSystem() { return *mCombatSystem; }
    GameStateManager& GetGameStateManager() { return *mGameStateManager; }
    InputSystem& GetInputSystem() { return *mInputSystem; }
    AudioManager& GetAudioManager() { return *mAudioManager; }
    GameplaySystem& GetGameplaySystem() { return *mGameplaySystem; }
    UISystem& GetUISystem() { return *mUISystem; }

private:
    void ProcessEvents();
    void Update(float deltaTime);
    void Render();

    // Core SDL resources
    SDL_Window* mWindow;
    SDL_GLContext mGLContext;
    
    // Game state
    bool mRunning;
    std::uint32_t mLastFrameTime;
    
    // Window properties
    int mWindowWidth;
    int mWindowHeight;
    
    // Subsystems (using composition)
    std::unique_ptr<ECSRegistry> mECS;
    std::unique_ptr<RenderSystem> mRenderer;
    std::unique_ptr<MovementSystem> mMovementSystem;
    std::unique_ptr<CollisionSystem> mCollisionSystem;
    std::unique_ptr<CombatSystem> mCombatSystem;
    std::unique_ptr<GameStateManager> mGameStateManager;
    std::unique_ptr<InputSystem> mInputSystem;
    std::unique_ptr<AudioManager> mAudioManager;
    std::unique_ptr<GameplaySystem> mGameplaySystem;
    std::unique_ptr<UISystem> mUISystem;

    // Configuration constants
    static constexpr int DEFAULT_WINDOW_WIDTH = 1600;
    static constexpr int DEFAULT_WINDOW_HEIGHT = 1200;
    static constexpr float TARGET_FPS = 60.0F;
    static constexpr float FRAME_TIME_MS = 1000.0F / TARGET_FPS;
};

} // namespace Core
