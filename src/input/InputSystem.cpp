#include "InputSystem.h"
#include "../components/Components.h"
#include "../core/GameStateManager.h"
#include "../gameplay/GameplaySystem.h"
#include "../rendering/Renderer.h"
#include "../ui/UISystem.h"
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_mouse.h>
#include <SDL2/SDL_keyboard.h>
#include <SDL2/SDL_log.h>
#include <algorithm>
#include <cmath>

InputSystem::InputSystem(ECSRegistry& registry)
    : SystemBase(registry)
    , mKeyStates(SDL_NUM_SCANCODES, false)
    , mMouseStates(32, false)
    , mMouseX(0)
    , mMouseY(0)
    , mSelectedEntities()
    , mIsDragging(false)
    , mDragStartX(0)
    , mDragStartY(0)
    , mDragEndX(0)
    , mDragEndY(0)
    , mSelectedPlanet(INVALID_ENTITY)
    , mGameStateManager(nullptr)
    , mGameplaySystem(nullptr)
{
}

InputSystem::~InputSystem() {
    Shutdown();
}

bool InputSystem::Initialize() {
    SDL_Log("Input manager initialized");
    return true;
}

void InputSystem::Update(float deltaTime) {
    (void)deltaTime; // Suppress unused parameter warning
    
    // Clean up dead entities from selection
    CleanupDeadEntitiesFromSelection();
}

void InputSystem::Shutdown() {
    SDL_Log("Input manager shutdown");
}

void InputSystem::ProcessEvent(const SDL_Event& event) {
    switch (event.type) {
        case SDL_QUIT:
            // Game should handle quit
            break;
        case SDL_MOUSEBUTTONDOWN:
            HandleMouseButtonDown(event.button);
            break;
        case SDL_MOUSEBUTTONUP:
            HandleMouseButtonUp(event.button);
            break;
        case SDL_MOUSEMOTION:
            HandleMouseMotion(event.motion);
            break;
        case SDL_KEYDOWN:
            HandleKeyDown(event.key);
            break;
        case SDL_KEYUP:
            HandleKeyUp(event.key);
            break;
        default:
            break;
    }
}

bool InputSystem::IsKeyPressed(SDL_Scancode key) const {
    if (key >= 0 && key < static_cast<SDL_Scancode>(mKeyStates.size())) {
        return mKeyStates[key];
    }
    return false;
}

bool InputSystem::IsMouseButtonPressed(std::uint8_t button) const {
    if (button < mMouseStates.size()) {
        return mMouseStates[button];
    }
    return false;
}

void InputSystem::GetMousePosition(int& mouseX, int& mouseY) const {
    mouseX = mMouseX;
    mouseY = mMouseY;
}

std::pair<float, float> InputSystem::ScreenToWorld(int screenX, int screenY, int windowWidth, int windowHeight) const {
    float worldX = (static_cast<float>(screenX) / static_cast<float>(windowWidth)) * WORLD_X_SCALE - WORLD_X_OFFSET;
    float worldY = -((static_cast<float>(screenY) / static_cast<float>(windowHeight)) * WORLD_Y_SCALE - WORLD_Y_OFFSET);
    return {worldX, worldY};
}

void InputSystem::HandleMouseButtonDown(const SDL_MouseButtonEvent& event) {
    if (event.button < mMouseStates.size()) {
        mMouseStates[event.button] = true;
    }
    
    if (event.button == SDL_BUTTON_LEFT) {
        // Check if click is in UI first
        if (mUISystem != nullptr && mUISystem->IsClickInBuildInterface(event.x, event.y)) {
            mUISystem->HandleUIClick(event.x, event.y);
            return; // Don't process world clicks if UI was clicked
        }
        
        bool isCtrlHeld = IsKeyPressed(SDL_SCANCODE_LCTRL) || IsKeyPressed(SDL_SCANCODE_RCTRL);
        HandleSelection(event.x, event.y, isCtrlHeld);
        
        // Start drag selection
        mIsDragging = true;
        mDragStartX = event.x;
        mDragStartY = event.y;
    } else if (event.button == SDL_BUTTON_RIGHT) {
        // Handle movement/attack commands
        const Uint8* keyState = SDL_GetKeyboardState(nullptr);
        if (keyState[SDL_SCANCODE_LSHIFT] || keyState[SDL_SCANCODE_RSHIFT]) {
            HandleAttackCommand(event.x, event.y);
        } else {
            HandleMovement(event.x, event.y);
        }
    }
}

void InputSystem::HandleMouseButtonUp(const SDL_MouseButtonEvent& event) {
    if (event.button < mMouseStates.size()) {
        mMouseStates[event.button] = false;
    }
    
    if (event.button == SDL_BUTTON_LEFT && mIsDragging) {
        mIsDragging = false;
        
        // Hide drag selection box
        if (mRenderer != nullptr) {
            mRenderer->SetDragSelectionBox(0, 0, 0, 0, false);
        }
        
        // Check if this was a drag selection
        int deltaX = std::abs(event.x - mDragStartX);
        int deltaY = std::abs(event.y - mDragStartY);
        
        if (deltaX > MIN_DRAG_DISTANCE || deltaY > MIN_DRAG_DISTANCE) {
            mDragEndX = event.x;
            mDragEndY = event.y;
            HandleBoxSelection(mDragStartX, mDragStartY, mDragEndX, mDragEndY);
        }
    }
}

void InputSystem::HandleMouseMotion(const SDL_MouseMotionEvent& event) {
    mMouseX = event.x;
    mMouseY = event.y;
    
    // Update drag selection box if dragging
    if (mIsDragging && mRenderer != nullptr) {
        mRenderer->SetDragSelectionBox(mDragStartX, mDragStartY, event.x, event.y, true);
    }
}

void InputSystem::HandleKeyDown(const SDL_KeyboardEvent& event) {
    if (event.keysym.scancode < mKeyStates.size()) {
        mKeyStates[event.keysym.scancode] = true;
    }
    
    switch (event.keysym.sym) {
        case SDLK_ESCAPE:
            if (mGameStateManager != nullptr && mGameStateManager->GetCurrentState() == GameState::GameOver) {
                // Restart the game
                mGameStateManager->StartNewGame();
                // Reset gameplay system state
                if (mGameplaySystem != nullptr) {
                    mGameplaySystem->ResetGameState();
                }
                SDL_Log("Game restart requested from game over screen");
            } else {
                mSelectedEntities.clear();
            }
            break;
        case SDLK_SPACE:
            // Toggle pause/resume with spacebar
            if (mGameStateManager) {
                if (mGameStateManager->GetCurrentState() == GameState::Playing) {
                    mGameStateManager->PauseGame();
                } else if (mGameStateManager->GetCurrentState() == GameState::Paused) {
                    mGameStateManager->ResumeGame();
                }
            }
            break;
        case SDLK_a:
            if (IsKeyPressed(SDL_SCANCODE_LCTRL) || IsKeyPressed(SDL_SCANCODE_RCTRL)) {
                // Select all player units that are alive
                using namespace Components;
                ClearAllSelections();
                mSelectedEntities.clear();
                mRegistry.ForEach<Spacecraft>([&](EntityID entity, const Spacecraft& spacecraft) {
                    auto* health = mRegistry.GetComponent<Health>(entity);
                    if (spacecraft.type == SpacecraftType::Player && health && health->isAlive) {
                        mSelectedEntities.push_back(entity);
                        SetEntitySelected(entity, true);
                    }
                });
                
                // Update UI with new selection count
                if (mUISystem != nullptr) {
                    mUISystem->UpdateSelectedCount(static_cast<int>(mSelectedEntities.size()));
                }
            }
            break;
        default:
            break;
    }
}

void InputSystem::HandleKeyUp(const SDL_KeyboardEvent& event) {
    if (event.keysym.scancode < mKeyStates.size()) {
        mKeyStates[event.keysym.scancode] = false;
    }
}

void InputSystem::HandleSelection(int mouseX, int mouseY, bool isCtrlHeld) {
    // Get window size for coordinate conversion
    int windowWidth = 0;
    int windowHeight = 0;
    SDL_GetWindowSize(SDL_GetMouseFocus(), &windowWidth, &windowHeight);
    
    auto [worldX, worldY] = ScreenToWorld(mouseX, mouseY, windowWidth, windowHeight);
    
    // Find entity at position - only select player units
    EntityID clickedEntity = FindSelectableEntityAtPosition(worldX, worldY, SHIP_CLICK_RADIUS);
    
    // Also check for planet selection
    EntityID clickedPlanet = FindSelectablePlanetAtPosition(worldX, worldY, PLANET_CLICK_RADIUS);
    
    if (clickedEntity != INVALID_ENTITY) {
        // Clear planet selection when selecting ships
        mSelectedPlanet = INVALID_ENTITY;
        SetPlanetSelected(INVALID_ENTITY, false);
        
        if (!isCtrlHeld) {
            // Clear previous selections
            ClearAllSelections();
            mSelectedEntities.clear();
        }
        
        // Toggle selection
        auto iterator = std::find(mSelectedEntities.begin(), mSelectedEntities.end(), clickedEntity);
        if (iterator != mSelectedEntities.end()) {
            // Deselect
            mSelectedEntities.erase(iterator);
            SetEntitySelected(clickedEntity, false);
        } else {
            // Select
            mSelectedEntities.push_back(clickedEntity);
            SetEntitySelected(clickedEntity, true);
        }
        
        SDL_Log("Selected %zu units", mSelectedEntities.size());
        
        // Update UI with new selection count
        if (mUISystem != nullptr) {
            mUISystem->UpdateSelectedCount(static_cast<int>(mSelectedEntities.size()));
        }
    } else if (clickedPlanet != INVALID_ENTITY) {
        // Clear ship selections when selecting planets
        ClearAllSelections();
        mSelectedEntities.clear();
        
        // Select planet
        SetPlanetSelected(mSelectedPlanet, false); // Clear old selection
        mSelectedPlanet = clickedPlanet;
        SetPlanetSelected(clickedPlanet, true);
        
        // Notify UI system of planet selection
        if (mUISystem != nullptr) {
            mUISystem->SetSelectedPlanet(clickedPlanet);
        }
        
        SDL_Log("Selected planet %u", clickedPlanet);
    } else if (!isCtrlHeld) {
        // Clear all selections including planet selection
        ClearAllSelections();
        mSelectedEntities.clear();
        
        // Clear planet selection and hide UI
        SetPlanetSelected(mSelectedPlanet, false);
        mSelectedPlanet = INVALID_ENTITY;
        
        if (mUISystem != nullptr) {
            mUISystem->SetSelectedPlanet(INVALID_ENTITY);
            mUISystem->UpdateSelectedCount(0);
        }
        
        SDL_Log("All selections cleared");
    }
}

void InputSystem::HandleBoxSelection(int startX, int startY, int endX, int endY) {
    // Get window size for coordinate conversion
    int windowWidth = 0;
    int windowHeight = 0;
    SDL_GetWindowSize(SDL_GetMouseFocus(), &windowWidth, &windowHeight);
    
    auto [worldStartX, worldStartY] = ScreenToWorld(startX, startY, windowWidth, windowHeight);
    auto [worldEndX, worldEndY] = ScreenToWorld(endX, endY, windowWidth, windowHeight);
    
    float minX = std::min(worldStartX, worldEndX);
    float maxX = std::max(worldStartX, worldEndX);
    float minY = std::min(worldStartY, worldEndY);
    float maxY = std::max(worldStartY, worldEndY);
    
    std::vector<EntityID> entitiesInBox = FindSelectableEntitiesInBox(minX, minY, maxX, maxY);
    
    if (!IsKeyPressed(SDL_SCANCODE_LCTRL) && !IsKeyPressed(SDL_SCANCODE_RCTRL)) {
        ClearAllSelections();
        mSelectedEntities.clear();
    }
    
    for (EntityID entity : entitiesInBox) {
        auto it = std::find(mSelectedEntities.begin(), mSelectedEntities.end(), entity);
        if (it == mSelectedEntities.end()) {
            mSelectedEntities.push_back(entity);
            SetEntitySelected(entity, true);
        }
    }
    
    SDL_Log("Box selected %zu units", mSelectedEntities.size());
    
    // Update UI with new selection count
    if (mUISystem != nullptr) {
        mUISystem->UpdateSelectedCount(static_cast<int>(mSelectedEntities.size()));
    }
}

void InputSystem::HandleMovement(int mouseX, int mouseY) {
    if (mSelectedEntities.empty()) {
        return;
    }
    
    // Get window size for coordinate conversion
    int windowWidth = 0;
    int windowHeight = 0;
    SDL_GetWindowSize(SDL_GetMouseFocus(), &windowWidth, &windowHeight);
    
    auto [worldX, worldY] = ScreenToWorld(mouseX, mouseY, windowWidth, windowHeight);
    
    // Check if there's an enemy at the clicked position (smart-click)
    EntityID enemyTarget = FindEnemyAtPosition(worldX, worldY, ENEMY_CLICK_RADIUS);
    
    if (enemyTarget != INVALID_ENTITY) {
        // Right-clicked on enemy - treat as attack command
        using namespace Components;
        for (EntityID entity : mSelectedEntities) {
            if (auto* spacecraft = mRegistry.GetComponent<Spacecraft>(entity)) {
                if (spacecraft->type == SpacecraftType::Player) {
                    spacecraft->targetEntity = enemyTarget; // Set the target to pursue
                    spacecraft->isMoving = true;
                    spacecraft->isAttacking = true;
                    SDL_Log("Unit %u ordered to attack and pursue enemy %u (smart-click)", entity, enemyTarget);
                }
            }
        }
    } else {
        // Right-clicked on empty space - normal move command
        using namespace Components;
        for (EntityID entity : mSelectedEntities) {
            if (auto* spacecraft = mRegistry.GetComponent<Spacecraft>(entity)) {
                if (spacecraft->type == SpacecraftType::Player) {
                    spacecraft->destX = worldX;
                    spacecraft->destY = worldY;
                    spacecraft->isMoving = true;
                    spacecraft->isAttacking = false; // Clear attack mode
                    spacecraft->targetEntity = INVALID_ENTITY; // Clear target
                }
            }
        }
    }
}

void InputSystem::HandleAttackCommand(int mouseX, int mouseY) {
    if (mSelectedEntities.empty()) {
        return;
    }
    
    // Get window size for coordinate conversion
    int windowWidth = 0;
    int windowHeight = 0;
    SDL_GetWindowSize(SDL_GetMouseFocus(), &windowWidth, &windowHeight);
    
    auto [worldX, worldY] = ScreenToWorld(mouseX, mouseY, windowWidth, windowHeight);
    
    // Find enemy target
    EntityID target = FindEnemyAtPosition(worldX, worldY, ENEMY_CLICK_RADIUS);
    
    if (target != INVALID_ENTITY) {
        // Command selected units to attack the target entity
        using namespace Components;
        for (EntityID entity : mSelectedEntities) {
            if (auto* spacecraft = mRegistry.GetComponent<Spacecraft>(entity)) {
                if (spacecraft->type == SpacecraftType::Player) {
                    spacecraft->targetEntity = target; // Set the target to pursue
                    spacecraft->isMoving = true;
                    spacecraft->isAttacking = true;
                    SDL_Log("Unit %u ordered to attack and pursue enemy %u", entity, target);
                }
            }
        }
    } else {
        // No enemy target, move to attack position
        using namespace Components;
        for (EntityID entity : mSelectedEntities) {
            if (auto* spacecraft = mRegistry.GetComponent<Spacecraft>(entity)) {
                if (spacecraft->type == SpacecraftType::Player) {
                    spacecraft->destX = worldX;
                    spacecraft->destY = worldY;
                    spacecraft->isMoving = true;
                    spacecraft->isAttacking = false;
                }
            }
        }
        SDL_Log("Units ordered to move to attack position (%.2f, %.2f)", worldX, worldY);
    }
}

EntityID InputSystem::FindEntityAtPosition(float worldX, float worldY, float radius) const {
    using namespace Components;
    
    EntityID closestEntity = INVALID_ENTITY;
    float closestDistance = std::numeric_limits<float>::max();
    
    mRegistry.ForEach<Position>([&](EntityID entity, const Position& pos) {
        float dx = pos.posX - worldX;
        float dy = pos.posY - worldY;
        float distance = std::sqrt(dx * dx + dy * dy);
        
        if (distance <= radius && distance < closestDistance) {
            closestDistance = distance;
            closestEntity = entity;
        }
    });
    
    return closestEntity;
}

EntityID InputSystem::FindSelectableEntityAtPosition(float worldX, float worldY, float radius) const {
    using namespace Components;
    
    EntityID closestEntity = INVALID_ENTITY;
    float closestDistance = std::numeric_limits<float>::max();
    
    // Only find player spacecraft that are alive
    mRegistry.ForEach<Position>([&](EntityID entity, const Position& pos) {
        auto* spacecraft = mRegistry.GetComponent<Spacecraft>(entity);
        auto* health = mRegistry.GetComponent<Health>(entity);
        if (!spacecraft || spacecraft->type != SpacecraftType::Player || 
            !health || !health->isAlive) {
            return; // Skip non-player entities or dead entities
        }
        
        float deltaX = pos.posX - worldX;
        float deltaY = pos.posY - worldY;
        float distance = std::sqrt((deltaX * deltaX) + (deltaY * deltaY));
        
        if (distance <= radius && distance < closestDistance) {
            closestDistance = distance;
            closestEntity = entity;
        }
    });
    
    return closestEntity;
}

EntityID InputSystem::FindEnemyAtPosition(float worldX, float worldY, float radius) const {
    using namespace Components;
    
    EntityID closestEntity = INVALID_ENTITY;
    float closestDistance = std::numeric_limits<float>::max();
    
    // Only find enemy spacecraft that are alive
    mRegistry.ForEach<Position>([&](EntityID entity, const Position& pos) {
        auto* spacecraft = mRegistry.GetComponent<Spacecraft>(entity);
        auto* health = mRegistry.GetComponent<Health>(entity);
        if (!spacecraft || spacecraft->type != SpacecraftType::Enemy || 
            !health || !health->isAlive) {
            return; // Skip non-enemy entities or dead enemies
        }
        
        float deltaX = pos.posX - worldX;
        float deltaY = pos.posY - worldY;
        float distance = std::sqrt((deltaX * deltaX) + (deltaY * deltaY));
        
        if (distance <= radius && distance < closestDistance) {
            closestDistance = distance;
            closestEntity = entity;
        }
    });
    
    return closestEntity;
}

EntityID InputSystem::FindSelectablePlanetAtPosition(float worldX, float worldY, float radius) const {
    using namespace Components;
    
    EntityID closestEntity = INVALID_ENTITY;
    float closestDistance = std::numeric_limits<float>::max();
    
    // Only find planets that are player-owned
    mRegistry.ForEach<Position>([&](EntityID entity, const Position& pos) {
        auto* planet = mRegistry.GetComponent<Planet>(entity);
        if (!planet || !planet->isPlayerOwned) {
            return; // Skip non-planets or enemy planets
        }
        
        float deltaX = pos.posX - worldX;
        float deltaY = pos.posY - worldY;
        float distance = std::sqrt((deltaX * deltaX) + (deltaY * deltaY));
        
        if (distance <= radius && distance < closestDistance) {
            closestDistance = distance;
            closestEntity = entity;
        }
    });
    
    return closestEntity;
}

std::vector<EntityID> InputSystem::FindEntitiesInBox(float minX, float minY, float maxX, float maxY) const {
    using namespace Components;
    
    std::vector<EntityID> entities;
    
    mRegistry.ForEach<Position>([&](EntityID entity, const Position& pos) {
        if (pos.posX >= minX && pos.posX <= maxX && pos.posY >= minY && pos.posY <= maxY) {
            entities.push_back(entity);
        }
    });
    
    return entities;
}

std::vector<EntityID> InputSystem::FindSelectableEntitiesInBox(float minX, float minY, float maxX, float maxY) const {
    using namespace Components;
    
    std::vector<EntityID> entities;
    
    mRegistry.ForEach<Position>([&](EntityID entity, const Position& pos) {
        if (pos.posX >= minX && pos.posX <= maxX && pos.posY >= minY && pos.posY <= maxY) {
            // Only include player spacecraft that are alive
            auto* spacecraft = mRegistry.GetComponent<Spacecraft>(entity);
            auto* health = mRegistry.GetComponent<Health>(entity);
            if (spacecraft && spacecraft->type == SpacecraftType::Player && 
                health && health->isAlive) {
                entities.push_back(entity);
            }
        }
    });
    
    return entities;
}

void InputSystem::SetEntitySelected(EntityID entity, bool selected) {
    using namespace Components;
    
    auto* selectable = mRegistry.GetComponent<Selectable>(entity);
    if (selectable != nullptr) {
        selectable->isSelected = selected;
    }
}

void InputSystem::SetPlanetSelected(EntityID planet, bool selected) {
    using namespace Components;
    
    if (planet == INVALID_ENTITY) {
        // Clear all planet selections
        mRegistry.ForEach<Planet>([&](EntityID entity, Planet& planetComp) {
            auto* selectable = mRegistry.GetComponent<Selectable>(entity);
            if (selectable != nullptr) {
                selectable->isSelected = false;
            }
        });
        return;
    }
    
    auto* selectable = mRegistry.GetComponent<Selectable>(planet);
    if (selectable != nullptr) {
        selectable->isSelected = selected;
    }
}

void InputSystem::ClearAllSelections() {
    using namespace Components;
    
    // Clear all entity selections
    mRegistry.ForEach<Selectable>([&](EntityID entity, Selectable& selectable) {
        (void)entity; // Suppress unused parameter warning
        selectable.isSelected = false;
    });
}

void InputSystem::CleanupDeadEntitiesFromSelection() {
    using namespace Components;
    
    // Remove dead entities from selection and clear their visual highlight
    auto iterator = std::remove_if(mSelectedEntities.begin(), mSelectedEntities.end(), 
        [this](EntityID entity) {
            auto* health = mRegistry.GetComponent<Health>(entity);
            if (!health || !health->isAlive) {
                // Clear the visual selection highlight for dead entities
                SetEntitySelected(entity, false);
                return true;
            }
            return false;
        });
    
    mSelectedEntities.erase(iterator, mSelectedEntities.end());
}
