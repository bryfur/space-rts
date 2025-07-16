#pragma once

#include "../core/ECSRegistry.h"
#include "../core/SystemBase.h"
#include <string>

/**
 * @brief Professional rendering system using modern OpenGL practices
 */
class RenderSystem : public SystemBase {
public:
    explicit RenderSystem(ECSRegistry& registry);
    ~RenderSystem() override;

    // SystemBase interface
    bool Initialize(int windowWidth, int windowHeight);
    bool Initialize() override { return true; } // Required by SystemBase
    void Update(float deltaTime) override;
    void Shutdown() override;

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

private:
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
