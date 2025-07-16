#include "UISystem.h"
#include "../systems/RenderSystem.h"
#include <SDL2/SDL_log.h>
#include <SDL2/SDL_mouse.h>
#include <GL/gl.h>
#include <algorithm>

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
    if (!mShowUI || mRenderSystem == nullptr) {
        return;
    }
    
    RenderGameInfo();
    
    if (mSelectedPlanet != INVALID_ENTITY) {
        RenderBuildInterface();
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
    constexpr float PANEL_Y = -0.4F;  // Updated to match new position
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
    if (planet == nullptr || !planet->isPlayerOwned) {
        return;
    }
    
    // Position build interface at bottom-right but not obscured
    float panelX = 0.55F;  // Right side
    float panelY = -0.4F;  // Move up from bottom edge to avoid obscuring
    constexpr float GRID_SIZE = 0.4F;
    constexpr float ICON_SIZE = 0.18F;  // Make icons larger to fill grid cells
    constexpr float ICON_SPACING = 0.2F;  // Match grid cell size
    
    // Draw grid border
    DrawGridBorder(panelX, panelY, GRID_SIZE);
    
    // Draw title above the grid
    mRenderSystem->RenderText("BUILD MENU", panelX + 0.05F, panelY + 0.25F, 0.025F, 1.0F, 1.0F, 1.0F);
    
    // Center icons in grid cells - align with grid lines
    float cellSize = GRID_SIZE / 2;  // 2x2 grid
    float startX = panelX + cellSize/2;  // Center of first cell
    float startY = panelY + cellSize/2;  // Center of first cell
    
    // Icon 1: Spacecraft (top-left cell)
    int spacecraftQueueCount = GetBuildQueueCount(mSelectedPlanet, Components::BuildableUnit::Spacecraft);
    RenderBuildIcon(startX, startY, ICON_SIZE, Components::BuildableUnit::Spacecraft, spacecraftQueueCount);
    
    // Icon 2: Reserved for future units (top-right cell)
    RenderEmptyIcon(startX + cellSize, startY, ICON_SIZE);
    
    // Icon 3: Reserved for future units (bottom-left cell)
    RenderEmptyIcon(startX, startY - cellSize, ICON_SIZE);
    
    // Icon 4: Reserved for future units (bottom-right cell)
    RenderEmptyIcon(startX + cellSize, startY - cellSize, ICON_SIZE);
    
    // Show build progress below the grid
    if (!planet->buildQueue.empty()) {
        auto& currentBuild = planet->buildQueue.front();
        float progress = (currentBuild.totalBuildTime - currentBuild.timeRemaining) / currentBuild.totalBuildTime * 100.0F;
        std::string progressText = "Building: " + std::to_string(static_cast<int>(progress)) + "%";
        mRenderSystem->RenderText(progressText, panelX + 0.05F, panelY - 0.25F, 0.025F, 0.0F, 1.0F, 0.0F);
    }
}

void UISystem::RenderBuildButton(float posX, float posY, float width, float height, 
                                Components::BuildableUnit unitType, int queueCount) {
    if (mRenderSystem == nullptr) {
        return;
    }
    
    // Render button background using text characters
    mRenderSystem->RenderText("[                ]", posX - 0.05F, posY, 0.04F, 0.5F, 0.5F, 0.5F);
    
    // Render button icon/text
    mRenderSystem->RenderText("[ BUILD SHIP ]", posX, posY, 0.035F, 1.0F, 1.0F, 1.0F);
    
    // Render queue count prominently
    if (queueCount > 0) {
        std::string queueText = "Queue: " + std::to_string(queueCount);
        mRenderSystem->RenderText(queueText, posX, posY - 0.08F, 0.03F, 1.0F, 1.0F, 0.0F);
    }
    
    // Add instructions
    mRenderSystem->RenderText("Click to build", posX, posY - 0.15F, 0.025F, 0.7F, 0.7F, 0.7F);
}

void UISystem::RenderGameInfo() {
    if (mRenderSystem == nullptr) {
        return;
    }
    
    // Render game time and selected count in top-left
    std::string timeText = "Time: " + std::to_string(static_cast<int>(mGameTime));
    mRenderSystem->RenderText(timeText, -0.95F, 0.9F, UI_TEXT_SIZE, 1.0F, 1.0F, 1.0F);
    
    if (mSelectedCount > 0) {
        std::string selectionText = "Selected: " + std::to_string(mSelectedCount);
        mRenderSystem->RenderText(selectionText, -0.95F, 0.85F, UI_TEXT_SIZE, 1.0F, 1.0F, 1.0F);
    }
}

void UISystem::DrawGridBorder(float x, float y, float size) {
    if (mRenderSystem == nullptr) {
        return;
    }
    
    // Draw grid border using OpenGL rectangles
    glColor3f(1.0F, 1.0F, 0.0F); // Yellow border
    glLineWidth(2.0F);
    
    // Draw border rectangle
    glBegin(GL_LINE_LOOP);
    glVertex2f(x, y + size/2);
    glVertex2f(x + size, y + size/2);
    glVertex2f(x + size, y - size/2);
    glVertex2f(x, y - size/2);
    glEnd();
    
    // Draw grid lines
    glLineWidth(1.0F);
    glColor3f(0.5F, 0.5F, 0.0F); // Darker yellow for grid
    
    // Vertical line
    glBegin(GL_LINES);
    glVertex2f(x + size/2, y + size/2);
    glVertex2f(x + size/2, y - size/2);
    glEnd();
    
    // Horizontal line  
    glBegin(GL_LINES);
    glVertex2f(x, y);
    glVertex2f(x + size, y);
    glEnd();
}

void UISystem::RenderBuildIcon(float x, float y, float size, Components::BuildableUnit unitType, int queueCount) {
    if (mRenderSystem == nullptr) {
        return;
    }
    
    // Draw icon background (filled rectangle)
    glColor3f(0.3F, 0.3F, 0.3F); // Dark gray background
    glBegin(GL_QUADS);
    glVertex2f(x - size/2, y + size/2);
    glVertex2f(x + size/2, y + size/2);
    glVertex2f(x + size/2, y - size/2);
    glVertex2f(x - size/2, y - size/2);
    glEnd();
    
    // Draw icon border
    glColor3f(0.6F, 0.6F, 0.6F); // Light gray border
    glLineWidth(2.0F);
    glBegin(GL_LINE_LOOP);
    glVertex2f(x - size/2, y + size/2);
    glVertex2f(x + size/2, y + size/2);
    glVertex2f(x + size/2, y - size/2);
    glVertex2f(x - size/2, y - size/2);
    glEnd();
    
    switch (unitType) {
        case Components::BuildableUnit::Spacecraft:
            // Draw triangle icon (like spacecraft) - filled triangle
            glColor3f(1.0F, 1.0F, 0.2F); // Yellow triangle
            glBegin(GL_TRIANGLES);
            glVertex2f(x, y + size/3);          // Top point
            glVertex2f(x - size/4, y - size/3); // Bottom left
            glVertex2f(x + size/4, y - size/3); // Bottom right
            glEnd();
            
            // Draw triangle outline
            glColor3f(1.0F, 0.8F, 0.0F); // Darker yellow outline
            glLineWidth(1.5F);
            glBegin(GL_LINE_LOOP);
            glVertex2f(x, y + size/3);
            glVertex2f(x - size/4, y - size/3);
            glVertex2f(x + size/4, y - size/3);
            glEnd();
            break;
        default:
            // Draw question mark or placeholder
            glColor3f(0.7F, 0.7F, 0.7F);
            mRenderSystem->RenderText("?", x - 0.01F, y, 0.025F, 0.7F, 0.7F, 0.7F);
            break;
    }
    
    // Show queue count if any
    if (queueCount > 0) {
        std::string queueText = std::to_string(queueCount);
        mRenderSystem->RenderText(queueText, x + size/3, y + size/3, 0.02F, 1.0F, 1.0F, 0.0F);
    }
}

void UISystem::RenderEmptyIcon(float x, float y, float size) {
    if (mRenderSystem == nullptr) {
        return;
    }
    
    // Draw empty icon slot background
    glColor3f(0.2F, 0.2F, 0.2F); // Very dark gray background
    glBegin(GL_QUADS);
    glVertex2f(x - size/2, y + size/2);
    glVertex2f(x + size/2, y + size/2);
    glVertex2f(x + size/2, y - size/2);
    glVertex2f(x - size/2, y - size/2);
    glEnd();
    
    // Draw empty icon border (dashed effect using smaller rectangles)
    glColor3f(0.4F, 0.4F, 0.4F); // Gray border
    glLineWidth(1.0F);
    glBegin(GL_LINE_LOOP);
    glVertex2f(x - size/2, y + size/2);
    glVertex2f(x + size/2, y + size/2);
    glVertex2f(x + size/2, y - size/2);
    glVertex2f(x - size/2, y - size/2);
    glEnd();
    
    // Draw dash in center to indicate empty slot
    glColor3f(0.4F, 0.4F, 0.4F);
    glLineWidth(3.0F);
    glBegin(GL_LINES);
    glVertex2f(x - size/4, y);
    glVertex2f(x + size/4, y);
    glEnd();
}

void UISystem::AddToBuildQueue(EntityID planet, Components::BuildableUnit unitType) {
    auto* planetComp = mRegistry.GetComponent<Components::Planet>(planet);
    if (planetComp == nullptr || !planetComp->isPlayerOwned) {
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
        
        // Create new spacecraft near the planet
        EntityID newShip = mRegistry.CreateEntity();
        mRegistry.AddComponent<Components::Position>(newShip, {position->posX + 0.1F, position->posY + 0.1F});
        mRegistry.AddComponent<Components::Spacecraft>(newShip, {Components::SpacecraftType::Player, 0.0F, 0.0F, 0.0F, false, false, 0.0F});
        mRegistry.AddComponent<Components::Health>(newShip, {10, 10, true});
        mRegistry.AddComponent<Components::Selectable>(newShip, {false, 0.03F});
        mRegistry.AddComponent<Components::Renderable>(newShip, {1.0F, 0.8F, 0.2F, 1.0F, 1.0F});
        
        SDL_Log("Spacecraft built and deployed from planet %u", planet);
    }
}
