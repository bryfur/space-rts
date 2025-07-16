#include "Game.h"
#include "../components/Components.h"
#include "../systems/RenderSystem.h"
#include "../systems/MovementSystem.h"
#include "../systems/CollisionSystem.h"
#include "../systems/CombatSystem.h"
#include "GameStateManager.h"
#include "../input/InputSystem.h"
#include "../rendering/AudioManager.h"
#include "../gameplay/GameplaySystem.h"
#include "../ui/UISystem.h"
#include <SDL2/SDL_log.h>
#include <GL/gl.h>
#include <algorithm>

namespace Core {

Game::Game()
    : mWindow(nullptr)
    , mGLContext(nullptr)
    , mRunning(false)
    , mLastFrameTime(0)
    , mWindowWidth(DEFAULT_WINDOW_WIDTH)
    , mWindowHeight(DEFAULT_WINDOW_HEIGHT)
{
}

Game::~Game() {
    Shutdown();
}

bool Game::Initialize() {
    SDL_Log("Initializing Space RTS Game Engine...");

    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        SDL_Log("Failed to initialize SDL: %s", SDL_GetError());
        return false;
    }

    // Create window
    mWindow = SDL_CreateWindow(
        "Space RTS - Professional Edition",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        mWindowWidth,
        mWindowHeight,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE
    );

    if (!mWindow) {
        SDL_Log("Failed to create window: %s", SDL_GetError());
        return false;
    }

    // Create OpenGL context
    mGLContext = SDL_GL_CreateContext(mWindow);
    if (!mGLContext) {
        SDL_Log("Failed to create OpenGL context: %s", SDL_GetError());
        return false;
    }

    // Enable VSync
    SDL_GL_SetSwapInterval(1);

    // Initialize subsystems
    mECS = std::make_unique<ECSRegistry>();
    mRenderer = std::make_unique<RenderSystem>(*mECS);
    mMovementSystem = std::make_unique<MovementSystem>(*mECS);
    mCollisionSystem = std::make_unique<CollisionSystem>(*mECS);
    mCombatSystem = std::make_unique<CombatSystem>(*mECS);
    mGameStateManager = std::make_unique<GameStateManager>();
    mInputSystem = std::make_unique<InputSystem>(*mECS);
    mAudioManager = std::make_unique<AudioManager>();
    mGameplaySystem = std::make_unique<GameplaySystem>(*mECS);
    mUISystem = std::make_unique<UISystem>(*mECS);

    // Initialize all subsystems
    if (!mRenderer->Initialize(mWindowWidth, mWindowHeight)) {
        SDL_Log("Failed to initialize render system");
        return false;
    }

    if (!mMovementSystem->Initialize()) {
        SDL_Log("Failed to initialize movement system");
        return false;
    }

    if (!mCollisionSystem->Initialize()) {
        SDL_Log("Failed to initialize collision system");
        return false;
    }

    if (!mCombatSystem->Initialize()) {
        SDL_Log("Failed to initialize combat system");
        return false;
    }

    if (!mInputSystem->Initialize()) {
        SDL_Log("Failed to initialize input manager");
        return false;
    }

    if (!mAudioManager->Initialize()) {
        SDL_Log("Failed to initialize audio manager");
        return false;
    }

    if (!mGameplaySystem->Initialize()) {
        SDL_Log("Failed to initialize gameplay manager");
        return false;
    }

    if (!mUISystem->Initialize()) {
        SDL_Log("Failed to initialize UI manager");
        return false;
    }

    // Connect subsystems that need cross-system communication
    mCombatSystem->SetAudioManager(mAudioManager.get());
    mCollisionSystem->SetAudioManager(mAudioManager.get());
    mInputSystem->SetGameStateManager(mGameStateManager.get());
    mInputSystem->SetRenderSystem(mRenderer.get());
    mInputSystem->SetUISystem(mUISystem.get());
    mUISystem->SetRenderSystem(mRenderer.get());
    
    // Start background music
    mAudioManager->PlayBackgroundMusic();

    mLastFrameTime = SDL_GetTicks();
    mRunning = true;

    SDL_Log("Game engine initialized successfully!");
    return true;
}

void Game::Run() {
    SDL_Log("Starting main game loop...");
    
    while (mRunning) {
        std::uint32_t currentTime = SDL_GetTicks();
        float deltaTime = static_cast<float>(currentTime - mLastFrameTime) / 1000.0F;
        mLastFrameTime = currentTime;

        // Cap delta time to prevent large jumps
        deltaTime = std::min(deltaTime, 1.0F / 30.0F);

        ProcessEvents();
        Update(deltaTime);
        Render();

        // Frame rate limiting
        std::uint32_t frameTime = SDL_GetTicks() - currentTime;
        if (frameTime < FRAME_TIME_MS) {
            SDL_Delay(static_cast<std::uint32_t>(FRAME_TIME_MS - frameTime));
        }
    }
}

void Game::ProcessEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                RequestShutdown();
                break;
                
            case SDL_WINDOWEVENT:
                if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                    mWindowWidth = event.window.data1;
                    mWindowHeight = event.window.data2;
                    mRenderer->OnWindowResize(mWindowWidth, mWindowHeight);
                }
                break;
                
            default:
                // Pass other events to input manager
                mInputSystem->ProcessEvent(event);
                break;
        }
    }
}

void Game::Update(float deltaTime) {
    // Update game state first
    mGameStateManager->UpdateGameTime(deltaTime);
    
    // Update all systems in order
    mInputSystem->Update(deltaTime);
    mMovementSystem->Update(deltaTime);
    mCollisionSystem->Update(deltaTime);
    mCombatSystem->Update(deltaTime);
    mGameplaySystem->Update(deltaTime);
    mUISystem->Update(deltaTime);
}

void Game::Render() {
    mRenderer->BeginFrame();
    mRenderer->RenderWorld();
    mUISystem->RenderUI(); // Render UI elements
    mRenderer->RenderUI(); // Render any additional renderer UI
    mRenderer->EndFrame();
    
    SDL_GL_SwapWindow(mWindow);
}

void Game::Shutdown() {
    SDL_Log("Shutting down game engine...");
    
    // Cleanup subsystems in reverse order
    mUISystem.reset();
    mGameplaySystem.reset();
    mAudioManager.reset();
    mInputSystem.reset();
    mRenderer.reset();
    mECS.reset();

    // Cleanup SDL resources
    if (mGLContext) {
        SDL_GL_DeleteContext(mGLContext);
        mGLContext = nullptr;
    }

    if (mWindow) {
        SDL_DestroyWindow(mWindow);
        mWindow = nullptr;
    }

    SDL_Quit();
    SDL_Log("Game engine shutdown complete.");
}

} // namespace Core
