#pragma once

#include "../core/ECSRegistry.h"
#include "../core/SystemBase.h"
#include <SDL2/SDL.h>
#include <vector>
#include <functional>

// Forward declaration
class GameStateManager;
class Renderer;
class UISystem;
class GameplaySystem;

/**
 * @brief Professional input system
 */
class InputSystem : public SystemBase {
public:
    explicit InputSystem(ECSRegistry& registry);
    ~InputSystem() override;

    // Set game state manager for pause/resume functionality
    void SetGameStateManager(GameStateManager* gameStateManager) { mGameStateManager = gameStateManager; }

    // Set renderer for visual feedback
    void SetRenderer(Renderer* renderer) { mRenderer = renderer; }

    // Set UI system for interaction
    void SetUISystem(UISystem* uiSystem) { mUISystem = uiSystem; }

    // Set gameplay system for game state resets
    void SetGameplaySystem(GameplaySystem* gameplaySystem) { mGameplaySystem = gameplaySystem; }

    // SystemBase interface
    bool Initialize() override;
    void Update(float deltaTime) override;
    void Shutdown() override;

    // Event processing
    void ProcessEvent(const SDL_Event& event);

    // Input state queries
    bool IsKeyPressed(SDL_Scancode key) const;
    bool IsMouseButtonPressed(std::uint8_t button) const;
    void GetMousePosition(int& mouseX, int& mouseY) const;

    // Coordinate conversion
    std::pair<float, float> ScreenToWorld(int screenX, int screenY, int windowWidth, int windowHeight) const;

private:
    // Event handlers
    void HandleMouseButtonDown(const SDL_MouseButtonEvent& event);
    void HandleMouseButtonUp(const SDL_MouseButtonEvent& event);
    void HandleMouseMotion(const SDL_MouseMotionEvent& event);
    void HandleKeyDown(const SDL_KeyboardEvent& event);
    void HandleKeyUp(const SDL_KeyboardEvent& event);

    // Selection system
    void HandleSelection(int mouseX, int mouseY, bool isCtrlHeld);
    void HandleBoxSelection(int startX, int startY, int endX, int endY);
    void HandleMovement(int mouseX, int mouseY);
    void HandleAttackCommand(int mouseX, int mouseY);

    // Utility functions
    EntityID FindEntityAtPosition(float worldX, float worldY, float radius) const;
    EntityID FindSelectableEntityAtPosition(float worldX, float worldY, float radius) const;
    EntityID FindSelectablePlanetAtPosition(float worldX, float worldY, float radius) const;
    EntityID FindEnemyAtPosition(float worldX, float worldY, float radius) const;
    std::vector<EntityID> FindEntitiesInBox(float minX, float minY, float maxX, float maxY) const;
    std::vector<EntityID> FindSelectableEntitiesInBox(float minX, float minY, float maxX, float maxY) const;

    // Selection management
    void SetEntitySelected(EntityID entity, bool selected);
    void SetPlanetSelected(EntityID planet, bool selected);
    void ClearAllSelections();
    void CleanupDeadEntitiesFromSelection();

    // Input state
    std::vector<bool> mKeyStates;
    std::vector<bool> mMouseStates;
    int mMouseX;
    int mMouseY;

    // Selection state
    std::vector<EntityID> mSelectedEntities;
    bool mIsDragging;
    int mDragStartX;
    int mDragStartY;
    int mDragEndX;
    int mDragEndY;

    // UI state
    EntityID mSelectedPlanet;

    // Game state integration
    GameStateManager* mGameStateManager;

    // Renderer integration
    Renderer* mRenderer = nullptr;

    // UI system integration
    UISystem* mUISystem = nullptr;

    // Gameplay system integration
    GameplaySystem* mGameplaySystem = nullptr;

    // Constants
    static constexpr float SHIP_CLICK_RADIUS = 0.06F; // Increased for easier targeting
    static constexpr float ENEMY_CLICK_RADIUS = 0.12F; // Increased for easier targeting
    static constexpr float PLANET_CLICK_RADIUS = 0.18F; // Slightly increased
    static constexpr int MIN_DRAG_DISTANCE = 5;
    static constexpr float WORLD_X_SCALE = 2.0F;
    static constexpr float WORLD_X_OFFSET = 1.0F;
    static constexpr float WORLD_Y_SCALE = 2.0F;
    static constexpr float WORLD_Y_OFFSET = 1.0F;
    static constexpr float WORLD_ASPECT_RATIO = 0.75F;
};
