#pragma once

#include <cstdint>

/**
 * @brief Enumeration of all possible game states
 */
enum class GameState : std::uint8_t {
    MainMenu,
    Playing,
    Paused,
    GameOver,
    Victory,
    Settings,
    Loading
};

/**
 * @brief Game state management system
 * 
 * Handles transitions between different game states and manages
 * state-specific logic and UI updates.
 */
class GameStateManager {
public:
    GameStateManager();
    ~GameStateManager() = default;

    // State management
    void ChangeState(GameState newState);
    void PushState(GameState newState);
    void PopState();
    
    // State queries
    GameState GetCurrentState() const { return mCurrentState; }
    GameState GetPreviousState() const { return mPreviousState; }
    bool IsInGame() const { return mCurrentState == GameState::Playing; }
    bool IsPaused() const { return mCurrentState == GameState::Paused; }
    bool IsGameOver() const { return mCurrentState == GameState::GameOver || mCurrentState == GameState::Victory; }
    
    // Game session data
    void StartNewGame();
    void EndGame(bool victory);
    void PauseGame();
    void ResumeGame();
    
    // Statistics
    float GetGameTime() const { return mGameTime; }
    std::uint32_t GetScore() const { return mScore; }
    std::uint32_t GetEnemiesKilled() const { return mEnemiesKilled; }
    std::uint32_t GetWaveNumber() const { return mWaveNumber; }
    
    void AddScore(std::uint32_t points) { mScore += points; }
    void IncrementEnemiesKilled() { ++mEnemiesKilled; }
    void SetWaveNumber(std::uint32_t wave) { mWaveNumber = wave; }
    void UpdateGameTime(float deltaTime);
    
    // Event callbacks
    void OnStateEnter(GameState state);
    void OnStateExit(GameState state);

private:
    GameState mCurrentState;
    GameState mPreviousState;
    GameState mStateStack[8]; // Simple state stack for pause/unpause
    std::uint8_t mStackSize;
    
    // Game session data
    float mGameTime;
    std::uint32_t mScore;
    std::uint32_t mEnemiesKilled;
    std::uint32_t mWaveNumber;
    
    void LogStateChange(GameState from, GameState to) const;
};
