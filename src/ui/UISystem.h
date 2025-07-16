#pragma once

#include "../core/ECSRegistry.h"
#include "../core/SystemBase.h"
#include "../components/Components.h"
#include <vector>

// Forward declarations
class Renderer;

/**
 * @brief UI system with build interface
 */
class UISystem : public SystemBase {
public:
    explicit UISystem(ECSRegistry& registry);
    ~UISystem() override;

    // SystemBase interface
    bool Initialize() override;
    void Update(float deltaTime) override;
    void Shutdown() override;

    // UI rendering
    void RenderUI();
    void RenderGameOverScreen();
    void SetRenderer(Renderer* renderer) { mRenderer = renderer; }

    // UI state management  
    void ShowGameUI(bool show);
    void UpdateGameTime(float gameTime);
    void UpdateSelectedCount(int count);
    void SetSelectedPlanet(EntityID planet);
    void SetGameStateManager(class GameStateManager* gameStateManager);

    // UI queries
    bool IsUIVisible() const { return mShowUI; }
    float GetGameTime() const { return mGameTime; }
    int GetSelectedCount() const { return mSelectedCount; }
    EntityID GetSelectedPlanet() const { return mSelectedPlanet; }

    // Build interface
    void HandleUIClick(int mouseX, int mouseY);
    bool IsClickInBuildInterface(int mouseX, int mouseY) const;

private:
    // UI rendering functions
    void RenderBuildInterface();
    void RenderBuildButton(float x, float y, float width, float height, 
                          Components::BuildableUnit unitType, int queueCount);
    void RenderGameInfo();
    void RenderUnitSelectionPanel();
    
    // Unit selection tracking
    struct SelectedUnitGroup {
        Components::SpacecraftType unitType;
        int count;
        float averageHealth;
    };
    std::vector<SelectedUnitGroup> GetSelectedUnitGroups() const;
    
    // Build queue management
    void AddToBuildQueue(EntityID planet, Components::BuildableUnit unitType);
    int GetBuildQueueCount(EntityID planet, Components::BuildableUnit unitType) const;
    void CompleteBuild(EntityID planet, Components::BuildableUnit unitType);

    // UI state
    bool mShowUI;
    float mGameTime;
    int mSelectedCount;
    EntityID mSelectedPlanet;
    Renderer* mRenderer = nullptr;
    class GameStateManager* mGameStateManager = nullptr;
    
    // UI layout constants
    static constexpr float UI_MARGIN = 0.02F;
    static constexpr float UI_TEXT_SIZE = 0.05F;
    static constexpr float UI_PANEL_ALPHA = 0.8F;
    static constexpr float BUILD_INTERFACE_WIDTH = 0.3F;
    static constexpr float BUILD_INTERFACE_HEIGHT = 0.4F;
    static constexpr float BUILD_BUTTON_SIZE = 0.08F;
    static constexpr float BUILD_BUTTON_SPACING = 0.02F;
};
