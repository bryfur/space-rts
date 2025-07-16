#include "GameStateManager.h"
#include <SDL2/SDL_log.h>

GameStateManager::GameStateManager()
    : mCurrentState(GameState::MainMenu)
    , mPreviousState(GameState::MainMenu)
    , mStateStack{}
    , mStackSize(0)
    , mGameTime(0.0F)
    , mScore(0)
    , mEnemiesKilled(0)
    , mWaveNumber(1)
{
    SDL_Log("Game state manager initialized - starting in MainMenu state");
}

void GameStateManager::ChangeState(GameState newState) {
    if (mCurrentState == newState) {
        return; // No change needed
    }
    
    OnStateExit(mCurrentState);
    LogStateChange(mCurrentState, newState);
    
    mPreviousState = mCurrentState;
    mCurrentState = newState;
    
    OnStateEnter(mCurrentState);
}

void GameStateManager::PushState(GameState newState) {
    if (mStackSize < sizeof(mStateStack) / sizeof(mStateStack[0])) {
        mStateStack[mStackSize] = mCurrentState;
        ++mStackSize;
        ChangeState(newState);
    } else {
        SDL_Log("Warning: State stack overflow, cannot push state");
    }
}

void GameStateManager::PopState() {
    if (mStackSize > 0) {
        --mStackSize;
        GameState previousState = mStateStack[mStackSize];
        ChangeState(previousState);
    } else {
        SDL_Log("Warning: Cannot pop state - stack is empty");
    }
}

void GameStateManager::StartNewGame() {
    // Reset all game statistics
    mGameTime = 0.0F;
    mScore = 0;
    mEnemiesKilled = 0;
    mWaveNumber = 1;
    
    // Clear state stack
    mStackSize = 0;
    
    ChangeState(GameState::Playing);
    SDL_Log("New game started - statistics reset");
}

void GameStateManager::EndGame(bool victory) {
    GameState endState = victory ? GameState::Victory : GameState::GameOver;
    ChangeState(endState);
    
    SDL_Log("Game ended - Victory: %s, Time: %.1fs, Score: %u, Enemies: %u, Wave: %u",
            victory ? "Yes" : "No", mGameTime, mScore, mEnemiesKilled, mWaveNumber);
}

void GameStateManager::PauseGame() {
    if (mCurrentState == GameState::Playing) {
        PushState(GameState::Paused);
        SDL_Log("Game paused");
    }
}

void GameStateManager::ResumeGame() {
    if (mCurrentState == GameState::Paused) {
        PopState();
        SDL_Log("Game resumed");
    }
}

void GameStateManager::UpdateGameTime(float deltaTime) {
    if (mCurrentState == GameState::Playing) {
        mGameTime += deltaTime;
    }
}

void GameStateManager::OnStateEnter(GameState state) {
    switch (state) {
        case GameState::MainMenu:
            SDL_Log("Entered Main Menu");
            break;
        case GameState::Playing:
            SDL_Log("Entered Playing state - game active");
            break;
        case GameState::Paused:
            SDL_Log("Entered Paused state");
            break;
        case GameState::GameOver:
            SDL_Log("Entered Game Over state");
            break;
        case GameState::Victory:
            SDL_Log("Entered Victory state - player won!");
            break;
        case GameState::Settings:
            SDL_Log("Entered Settings menu");
            break;
        case GameState::Loading:
            SDL_Log("Entered Loading state");
            break;
    }
}

void GameStateManager::OnStateExit(GameState state) {
    switch (state) {
        case GameState::MainMenu:
            SDL_Log("Exited Main Menu");
            break;
        case GameState::Playing:
            SDL_Log("Exited Playing state");
            break;
        case GameState::Paused:
            SDL_Log("Exited Paused state");
            break;
        case GameState::GameOver:
            SDL_Log("Exited Game Over state");
            break;
        case GameState::Victory:
            SDL_Log("Exited Victory state");
            break;
        case GameState::Settings:
            SDL_Log("Exited Settings menu");
            break;
        case GameState::Loading:
            SDL_Log("Exited Loading state");
            break;
    }
}

void GameStateManager::LogStateChange(GameState from, GameState target) const {
    const char* fromStr = "Unknown";
    const char* toStr = "Unknown";
    
    // Convert from state to string
    switch (from) {
        case GameState::MainMenu: fromStr = "MainMenu"; break;
        case GameState::Playing: fromStr = "Playing"; break;
        case GameState::Paused: fromStr = "Paused"; break;
        case GameState::GameOver: fromStr = "GameOver"; break;
        case GameState::Victory: fromStr = "Victory"; break;
        case GameState::Settings: fromStr = "Settings"; break;
        case GameState::Loading: fromStr = "Loading"; break;
    }
    
    // Convert to state to string
    switch (target) {
        case GameState::MainMenu: toStr = "MainMenu"; break;
        case GameState::Playing: toStr = "Playing"; break;
        case GameState::Paused: toStr = "Paused"; break;
        case GameState::GameOver: toStr = "GameOver"; break;
        case GameState::Victory: toStr = "Victory"; break;
        case GameState::Settings: toStr = "Settings"; break;
        case GameState::Loading: toStr = "Loading"; break;
    }
    
    SDL_Log("State transition: %s -> %s", fromStr, toStr);
}
