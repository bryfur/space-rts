#include "UISystem.h"
#include "../rendering/Renderer.h"
#include "../core/GameStateManager.h"
#include <SDL2/SDL_log.h>
#include <SDL2/SDL_mouse.h>
#include <algorithm>
#include <map>
#include <cmath>
#include <cstdlib>

UISystem::UISystem(ECSRegistry& registry)
    : SystemBase(registry)
    , mShowUI(true)
    , mGameTime(0.0F)
    , mSelectedCount(0)
    , mSelectedPlanet(INVALID_ENTITY)
{
}

UISystem::~UISystem() {
    Shutdown();
}

bool UISystem::Initialize() {
    SDL_Log("UI manager initialized");
    return true;
}

void UISystem::Update(float deltaTime) {
    mGameTime += deltaTime;
    
    // Update build timers for all planets
    using namespace Components;
    mRegistry.ForEach<Planet>([&](EntityID entity, Planet& planet) {
        if (!planet.buildQueue.empty()) {
            auto& currentBuild = planet.buildQueue.front();
            currentBuild.timeRemaining -= deltaTime;
            
            if (currentBuild.timeRemaining <= 0.0F) {
                // Build completed - spawn unit
                CompleteBuild(entity, currentBuild.unitType);
                planet.buildQueue.erase(planet.buildQueue.begin());
            }
        }
    });
}

void UISystem::Shutdown() {
    SDL_Log("UI manager shutdown");
}

void UISystem::RenderUI() {
    if (!mShowUI || mRenderer == nullptr) {
        return;
    }
    
    // Check if game is over and render game over screen
    if (mGameStateManager != nullptr && mGameStateManager->GetCurrentState() == GameState::GameOver) {
        RenderGameOverScreen();
        return;
    }
    
    RenderGameInfo();
    
    if (mSelectedPlanet != INVALID_ENTITY) {
        RenderBuildInterface();
    }
    
    // Render unit selection panel if units are selected
    if (mSelectedCount > 0) {
        RenderUnitSelectionPanel();
    }
}

void UISystem::ShowGameUI(bool show) {
    mShowUI = show;
}

void UISystem::UpdateGameTime(float gameTime) {
    mGameTime = gameTime;
}

void UISystem::UpdateSelectedCount(int count) {
    mSelectedCount = count;
}

void UISystem::SetSelectedPlanet(EntityID planet) {
    mSelectedPlanet = planet;
    if (planet != INVALID_ENTITY) {
        SDL_Log("UI: Planet %u selected for building", planet);
    } else {
        SDL_Log("UI: Planet selection cleared");
    }
}

void UISystem::SetGameStateManager(GameStateManager* gameStateManager) {
    mGameStateManager = gameStateManager;
}

void UISystem::HandleUIClick(int mouseX, int mouseY) {
    if (!IsClickInBuildInterface(mouseX, mouseY) || mSelectedPlanet == INVALID_ENTITY) {
        return;
    }
    
    // Calculate which build button was clicked
    // For now, just handle spacecraft building
    AddToBuildQueue(mSelectedPlanet, Components::BuildableUnit::Spacecraft);
    SDL_Log("Added spacecraft to build queue for planet %u", mSelectedPlanet);
}

bool UISystem::IsClickInBuildInterface(int mouseX, int mouseY) const {
    if (mSelectedPlanet == INVALID_ENTITY) {
        return false;
    }
    
    // Get window size
    int windowWidth = 0;
    int windowHeight = 0;
    SDL_GetWindowSize(SDL_GetMouseFocus(), &windowWidth, &windowHeight);
    
    // Convert mouse coordinates to world coordinates
    float worldX = (static_cast<float>(mouseX) / static_cast<float>(windowWidth)) * 2.0F - 1.0F;
    float worldY = -((static_cast<float>(mouseY) / static_cast<float>(windowHeight)) * 2.0F * 0.75F - 0.75F);
    
    // Grid layout parameters (matching RenderBuildInterface)
    constexpr float PANEL_X = 0.55F;
    constexpr float PANEL_Y = -0.5F;  // Updated to align bottom borders
    constexpr float GRID_SIZE = 0.4F;
    constexpr float ICON_SIZE = 0.18F;
    
    // Calculate spacecraft icon position (top-left cell)
    float cellSize = GRID_SIZE / 2;
    float iconX = PANEL_X + cellSize/2;
    float iconY = PANEL_Y + cellSize/2;
    
    // Check if click is on the spacecraft icon
    float iconLeft = iconX - ICON_SIZE/2;
    float iconRight = iconX + ICON_SIZE/2;
    float iconTop = iconY + ICON_SIZE/2;
    float iconBottom = iconY - ICON_SIZE/2;
    
    bool isOnSpacecraftIcon = (worldX >= iconLeft && worldX <= iconRight && 
                              worldY >= iconBottom && worldY <= iconTop);
    
    SDL_Log("UI Click check: mouse(%d,%d) world(%.3f,%.3f) icon(%.3f,%.3f) bounds=%.3f-%.3f,%.3f-%.3f onIcon=%s", 
            mouseX, mouseY, worldX, worldY, iconX, iconY,
            iconLeft, iconRight, iconBottom, iconTop,
            isOnSpacecraftIcon ? "true" : "false");
    
    return isOnSpacecraftIcon;
}

void UISystem::RenderBuildInterface() {
    if (mSelectedPlanet == INVALID_ENTITY) {
        return;
    }
    
    // Get planet component to check build queue
    auto* planet = mRegistry.GetComponent<Components::Planet>(mSelectedPlanet);
    auto* planetHealth = mRegistry.GetComponent<Components::Health>(mSelectedPlanet);
    
    if (planet == nullptr || !planet->isPlayerOwned) {
        return;
    }
    
    // Position build interface so bottom border aligns with selection panel
    float panelX = 0.55F;  // Right side
    float panelY = -0.5F;  // Adjusted so bottom borders align at same level
    constexpr float GRID_SIZE = 0.4F;
    constexpr float ICON_SIZE = 0.18F;  // Make icons larger to fill grid cells
    constexpr float ICON_SPACING = 0.2F;  // Match grid cell size
    
    // Draw grid border using Renderer
    mRenderer->DrawGridBorder(panelX, panelY, GRID_SIZE);
    
    // Check if planet is destroyed
    if (planetHealth == nullptr || !planetHealth->isAlive) {
        // Show destruction message instead of build options
        mRenderer->RenderText("PLANET DESTROYED", panelX + 0.05F, panelY + 0.25F, 0.025F, 1.0F, 0.0F, 0.0F);
        mRenderer->RenderText("CANNOT BUILD", panelX + 0.05F, panelY + 0.0F, 0.025F, 1.0F, 0.0F, 0.0F);
        return;
    }
    
    // Draw title above the grid
    mRenderer->RenderText("BUILD MENU", panelX + 0.05F, panelY + 0.25F, 0.025F, 1.0F, 1.0F, 1.0F);
    
    // Center icons in grid cells - align with grid lines
    float cellSize = GRID_SIZE / 2;  // 2x2 grid
    float startX = panelX + cellSize/2;  // Center of first cell
    float startY = panelY + cellSize/2;  // Center of first cell
    
    // Icon 1: Spacecraft (top-left cell)
    int spacecraftQueueCount = GetBuildQueueCount(mSelectedPlanet, Components::BuildableUnit::Spacecraft);
    mRenderer->RenderBuildIcon(startX, startY, ICON_SIZE, Components::BuildableUnit::Spacecraft, spacecraftQueueCount);
    
    // Icon 2: Reserved for future units (top-right cell)
    mRenderer->RenderEmptyIcon(startX + cellSize, startY, ICON_SIZE);
    
    // Icon 3: Reserved for future units (bottom-left cell)
    mRenderer->RenderEmptyIcon(startX, startY - cellSize, ICON_SIZE);
    
    // Icon 4: Reserved for future units (bottom-right cell)
    mRenderer->RenderEmptyIcon(startX + cellSize, startY - cellSize, ICON_SIZE);
    
    // Show build progress below the grid
    if (!planet->buildQueue.empty()) {
        auto& currentBuild = planet->buildQueue.front();
        float progress = (currentBuild.totalBuildTime - currentBuild.timeRemaining) / currentBuild.totalBuildTime * 100.0F;
        std::string progressText = "Building: " + std::to_string(static_cast<int>(progress)) + "%";
        mRenderer->RenderText(progressText, panelX + 0.05F, panelY - 0.25F, 0.025F, 0.0F, 1.0F, 0.0F);
    }
}

void UISystem::RenderBuildButton(float posX, float posY, float width, float height, 
                                Components::BuildableUnit unitType, int queueCount) {
    if (mRenderer == nullptr) {
        return;
    }
    
    // Render button background using text characters
    mRenderer->RenderText("[                ]", posX - 0.05F, posY, 0.04F, 0.5F, 0.5F, 0.5F);
    
    // Render button icon/text
    mRenderer->RenderText("[ BUILD SHIP ]", posX, posY, 0.035F, 1.0F, 1.0F, 1.0F);
    
    // Render queue count prominently
    if (queueCount > 0) {
        std::string queueText = "Queue: " + std::to_string(queueCount);
        mRenderer->RenderText(queueText, posX, posY - 0.08F, 0.03F, 1.0F, 1.0F, 0.0F);
    }
    
    // Add instructions
    mRenderer->RenderText("Click to build", posX, posY - 0.15F, 0.025F, 0.7F, 0.7F, 0.7F);
}

void UISystem::RenderGameInfo() {
    if (mRenderer == nullptr) {
        return;
    }
    
    // Render game time and selected count in top-left
    std::string timeText = "Time: " + std::to_string(static_cast<int>(mGameTime));
    mRenderer->RenderText(timeText, -0.95F, 0.9F, UI_TEXT_SIZE, 1.0F, 1.0F, 1.0F);
    
    if (mSelectedCount > 0) {
        std::string selectionText = "Selected: " + std::to_string(mSelectedCount);
        mRenderer->RenderText(selectionText, -0.95F, 0.85F, UI_TEXT_SIZE, 1.0F, 1.0F, 1.0F);
    }
}

void UISystem::AddToBuildQueue(EntityID planet, Components::BuildableUnit unitType) {
    auto* planetComp = mRegistry.GetComponent<Components::Planet>(planet);
    auto* planetHealth = mRegistry.GetComponent<Components::Health>(planet);
    
    if (planetComp == nullptr || !planetComp->isPlayerOwned) {
        return;
    }
    
    // Check if planet is destroyed - prevent building
    if (planetHealth == nullptr || !planetHealth->isAlive) {
        SDL_Log("Cannot build - planet is destroyed!");
        return;
    }
    
    Components::BuildQueueEntry entry;
    entry.unitType = unitType;
    entry.totalBuildTime = Components::Planet::SPACECRAFT_BUILD_TIME;
    entry.timeRemaining = entry.totalBuildTime;
    
    planetComp->buildQueue.push_back(entry);
}

int UISystem::GetBuildQueueCount(EntityID planet, Components::BuildableUnit unitType) const {
    auto* planetComp = mRegistry.GetComponent<Components::Planet>(planet);
    if (planetComp == nullptr) {
        return 0;
    }
    
    return static_cast<int>(std::count_if(planetComp->buildQueue.begin(), planetComp->buildQueue.end(),
        [unitType](const Components::BuildQueueEntry& entry) {
            return entry.unitType == unitType;
        }));
}

void UISystem::CompleteBuild(EntityID planet, Components::BuildableUnit unitType) {
    if (unitType == Components::BuildableUnit::Spacecraft) {
        // Get planet position to spawn ship nearby
        auto* position = mRegistry.GetComponent<Components::Position>(planet);
        if (position == nullptr) {
            return;
        }
        
        // Create new spacecraft at random position near the planet
        EntityID newShip = mRegistry.CreateEntity();
        
        // Generate random position around planet
        constexpr float MIN_DISTANCE = 0.12F; // Minimum distance from planet center
        constexpr float MAX_DISTANCE = 0.25F; // Maximum distance from planet center
        
        // Random angle (0 to 2π)
        float angle = static_cast<float>(rand()) / static_cast<float>(RAND_MAX) * 2.0F * 3.14159F;
        
        // Random distance between min and max
        float distance = MIN_DISTANCE + (static_cast<float>(rand()) / static_cast<float>(RAND_MAX)) * (MAX_DISTANCE - MIN_DISTANCE);
        
        // Calculate spawn position
        float spawnX = position->posX + std::cos(angle) * distance;
        float spawnY = position->posY + std::sin(angle) * distance;
        
        mRegistry.AddComponent<Components::Position>(newShip, {spawnX, spawnY});
        mRegistry.AddComponent<Components::Spacecraft>(newShip, {Components::SpacecraftType::Player, 0.0F, 0.0F, 0.0F, false, false, 0.0F});
        mRegistry.AddComponent<Components::Health>(newShip, {10, 10, true});
        mRegistry.AddComponent<Components::Selectable>(newShip, {false, 0.04F}); // Original size
        mRegistry.AddComponent<Components::Renderable>(newShip, {1.0F, 0.8F, 0.2F, 1.0F, 1.0F});
        
        SDL_Log("Spacecraft built and deployed from planet %u", planet);
    }
}

void UISystem::RenderUnitSelectionPanel() {
    auto selectedGroups = GetSelectedUnitGroups();
    if (selectedGroups.empty()) {
        return;
    }
        
    // Position panel at bottom center
    constexpr float PANEL_Y = -0.6F; // Bottom area
    constexpr float PANEL_WIDTH = 0.8F;
    constexpr float PANEL_HEIGHT = 0.2F;
    constexpr float ICON_SIZE = 0.12F;
    constexpr float ICON_SPACING = 0.15F;
    
    // Render panel background
    mRenderer->RenderUnitSelectionPanel(0.0F, PANEL_Y, PANEL_WIDTH, PANEL_HEIGHT);
    
    // Calculate starting position for icons (centered)
    float totalWidth = static_cast<float>(selectedGroups.size()) * ICON_SPACING;
    float startX = -totalWidth / 2.0F + ICON_SPACING / 2.0F;
    
    // Render unit icons
    for (size_t i = 0; i < selectedGroups.size(); ++i) {
        const auto& group = selectedGroups[i];
        float iconX = startX + static_cast<float>(i) * ICON_SPACING;
        mRenderer->RenderSelectedUnitIcon(iconX, PANEL_Y, ICON_SIZE, group.unitType, group.count, group.averageHealth);
    }
}

std::vector<UISystem::SelectedUnitGroup> UISystem::GetSelectedUnitGroups() const {
    std::vector<SelectedUnitGroup> groups;
    std::map<Components::SpacecraftType, SelectedUnitGroup> groupMap;
    
    using namespace Components;
    
    // Iterate through all spacecraft to find selected ones
    mRegistry.ForEach<Spacecraft>([&](EntityID entity, const Spacecraft& spacecraft) {
        auto* selectable = mRegistry.GetComponent<Selectable>(entity);
        auto* health = mRegistry.GetComponent<Health>(entity);
        
        if (!selectable || !selectable->isSelected || !health || !health->isAlive) {
            return;
        }
        
        // Find or create group for this unit type
        auto it = groupMap.find(spacecraft.type);
        if (it == groupMap.end()) {
            SelectedUnitGroup newGroup;
            newGroup.unitType = spacecraft.type;
            newGroup.count = 1;
            newGroup.averageHealth = static_cast<float>(health->currentHP) / static_cast<float>(health->maxHP);
            groupMap[spacecraft.type] = newGroup;
        } else {
            // Update existing group
            float currentAverage = it->second.averageHealth;
            int currentCount = it->second.count;
            float newHealthPercent = static_cast<float>(health->currentHP) / static_cast<float>(health->maxHP);
            
            // Calculate new average health
            it->second.averageHealth = (currentAverage * static_cast<float>(currentCount) + newHealthPercent) / static_cast<float>(currentCount + 1);
            it->second.count++;
        }
    });
    
    // Convert map to vector
    for (const auto& pair : groupMap) {
        groups.push_back(pair.second);
    }
    
    return groups;
}

void UISystem::RenderGameOverScreen() {
    if (mRenderer == nullptr || mGameStateManager == nullptr) {
        return;
    }
    
    // Check if game is over
    if (mGameStateManager->GetCurrentState() != GameState::GameOver) {
        return;
    }
    
    // Render dark overlay using the UI panel renderer
    mRenderer->RenderUnitSelectionPanel(0.0F, 0.0F, 2.0F, 1.5F);
    
    // Game Over title
    mRenderer->RenderText("GAME OVER", -0.2F, 0.3F, 0.08F, 1.0F, 0.2F, 0.2F);
    
    // Defeat message
    mRenderer->RenderText("All planets destroyed!", -0.25F, 0.15F, 0.04F, 1.0F, 1.0F, 1.0F);
    
    // Game statistics
    char statsText[256];
    
    // Survival time
    float survivalTime = mGameStateManager->GetGameTime();
    int minutes = static_cast<int>(survivalTime) / 60;
    int seconds = static_cast<int>(survivalTime) % 60;
    snprintf(statsText, sizeof(statsText), "Survival Time: %d:%02d", minutes, seconds);
    mRenderer->RenderText(statsText, -0.15F, 0.0F, 0.03F, 0.8F, 0.8F, 0.8F);
    
    // Score
    snprintf(statsText, sizeof(statsText), "Final Score: %u", mGameStateManager->GetScore());
    mRenderer->RenderText(statsText, -0.15F, -0.05F, 0.03F, 0.8F, 0.8F, 0.8F);
    
    // Enemies killed
    snprintf(statsText, sizeof(statsText), "Enemies Defeated: %u", mGameStateManager->GetEnemiesKilled());
    mRenderer->RenderText(statsText, -0.15F, -0.1F, 0.03F, 0.8F, 0.8F, 0.8F);
    
    // Wave reached
    snprintf(statsText, sizeof(statsText), "Wave Reached: %u", mGameStateManager->GetWaveNumber());
    mRenderer->RenderText(statsText, -0.15F, -0.15F, 0.03F, 0.8F, 0.8F, 0.8F);
    
    // Instructions
    mRenderer->RenderText("Press ESC to return to menu", -0.2F, -0.3F, 0.025F, 0.6F, 0.6F, 0.6F);
}
