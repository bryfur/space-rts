#pragma once

#include "../core/ECSRegistry.h"
#include "../components/Components.h"
#include <string>

/**
 * @brief Professional renderer using modern OpenGL practices
 * 
 * This is a pure rendering service, not an ECS system.
 * It handles all graphics rendering without needing regular updates.
 */
class Renderer {
public:
    explicit Renderer(ECSRegistry& registry);
    ~Renderer();

    // Core lifecycle
    bool Initialize(int windowWidth, int windowHeight);
    void Shutdown();

    // Rendering interface
    void BeginFrame();
    void RenderWorld();
    void RenderUI();
    void EndFrame();
    
    // Window management
    void OnWindowResize(int newWidth, int newHeight);

    // Text rendering interface
    void RenderText(const std::string& text, float posX, float posY, float size, 
                   float red = 1.0F, float green = 1.0F, float blue = 1.0F);
    void RenderTextCentered(const std::string& text, float posX, float posY, float size,
                           float red = 1.0F, float green = 1.0F, float blue = 1.0F);
    void RenderTextUI(const std::string& text, int screenX, int screenY, int size,
                     float red = 1.0F, float green = 1.0F, float blue = 1.0F);

    // Convenience text rendering functions
    void RenderTextUIWhite(const std::string& text, int screenX, int screenY, int size);
    void RenderTextUIGreen(const std::string& text, int screenX, int screenY, int size);
    void RenderTextUIRed(const std::string& text, int screenX, int screenY, int size);
    void RenderTextUIYellow(const std::string& text, int screenX, int screenY, int size);

    // Selection box interface
    void SetDragSelectionBox(int startX, int startY, int endX, int endY, bool active);

    // UI rendering interface
    void DrawGridBorder(float posX, float posY, float size);
    void RenderBuildIcon(float posX, float posY, float size, Components::BuildableUnit unitType, int queueCount);
    void RenderEmptyIcon(float posX, float posY, float size);
    
    // Unit selection panel
    void RenderUnitSelectionPanel(float posX, float posY, float width, float height);
    void RenderSelectedUnitIcon(float posX, float posY, float size, Components::SpacecraftType unitType, int count, float healthPercent);

private:
    // Helper functions
    std::string GetAIStateString(Components::AIState state) const;
    
    // Core rendering
    void SetupOpenGL();
    void RenderSpacecraft();
    void RenderPlanets();
    void RenderProjectiles();
    void RenderSelectionBoxes();
    void RenderDragSelectionBox();
    
    // Primitive rendering
    void DrawCircle(float centerX, float centerY, float radius);
    void DrawTriangle(float posX, float posY, float angle);
    void DrawHealthBar(float posX, float posY, float width, float height, float healthPercent);

    // Text rendering system
    bool InitializeTextRendering();
    void CleanupTextRendering();

    // ECS registry reference
    ECSRegistry& mRegistry;

    // Window properties
    int mWindowWidth;
    int mWindowHeight;
    
    // OpenGL state
    bool mTextRenderingInitialized;
    
    // Drag selection box state
    bool mDragSelectionActive = false;
    int mDragStartX = 0; 
    int mDragStartY = 0; 
    int mDragEndX = 0;
    int mDragEndY = 0;
    
    // Rendering constants
    static constexpr float WORLD_ASPECT_RATIO = 0.75F;
    static constexpr float TRIANGLE_SIZE = 0.03F;
    static constexpr int CIRCLE_SEGMENTS = 32;
};
