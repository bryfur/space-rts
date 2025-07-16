#include "RenderSystem.h"
#include "../components/Components.h"
#include <GL/gl.h>
#include <SDL_log.h>
#include <cmath>

// FreeType includes
#include <ft2build.h>
#include FT_FREETYPE_H
#include <unordered_map>
#include <iostream>

namespace {
    // FreeType character structure
    struct Character {
        unsigned int textureID;
        int width;
        int height;
        int bearingX;
        int bearingY;
        unsigned int advance;
    };

    // Static FreeType data
    FT_Library ftLibrary;
    FT_Face ftFace;
    std::unordered_map<char, Character> characters;
}

RenderSystem::RenderSystem(ECSRegistry& registry)
    : SystemBase(registry)
    , mWindowWidth(1600)
    , mWindowHeight(1200)
    , mTextRenderingInitialized(false)
    , mDragSelectionActive(false)
    , mDragStartX(0)
    , mDragStartY(0)
    , mDragEndX(0)
    , mDragEndY(0)
{
}

RenderSystem::~RenderSystem() {
    Shutdown();
}

bool RenderSystem::Initialize(int windowWidth, int windowHeight) {
    mWindowWidth = windowWidth;
    mWindowHeight = windowHeight;
    
    SetupOpenGL();
    
    if (!InitializeTextRendering()) {
        SDL_Log("Warning: Failed to initialize text rendering");
    }
    
    SDL_Log("Render system initialized");
    return true;
}

void RenderSystem::Update(float deltaTime) {
    // Render system doesn't need regular updates
    (void)deltaTime; // Suppress unused parameter warning
}

void RenderSystem::Shutdown() {
    CleanupTextRendering();
    SDL_Log("Render system shutdown");
}

void RenderSystem::BeginFrame() {
    glClearColor(0.0F, 0.0F, 0.0F, 1.0F);
    glClear(GL_COLOR_BUFFER_BIT);
    
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-1.0, 1.0, -WORLD_ASPECT_RATIO, WORLD_ASPECT_RATIO, -1.0, 1.0);
    
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

void RenderSystem::RenderWorld() {
    RenderPlanets();
    RenderSpacecraft();
    RenderProjectiles();
    RenderSelectionBoxes();
    RenderDragSelectionBox();
}

void RenderSystem::RenderUI() {
    // UI rendering will be handled by UIManager
    // This method is kept for interface compatibility
}

void RenderSystem::EndFrame() {
    // Frame ending is handled by the main game loop
}

void RenderSystem::OnWindowResize(int newWidth, int newHeight) {
    mWindowWidth = newWidth;
    mWindowHeight = newHeight;
    glViewport(0, 0, newWidth, newHeight);
}

void RenderSystem::SetupOpenGL() {
    glViewport(0, 0, mWindowWidth, mWindowHeight);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void RenderSystem::RenderSpacecraft() {
    using namespace Components;
    
    mRegistry.ForEach<Spacecraft>([this](EntityID entity, Spacecraft& spacecraft) {
        auto* position = mRegistry.GetComponent<Position>(entity);
        auto* health = mRegistry.GetComponent<Health>(entity);
        auto* renderable = mRegistry.GetComponent<Renderable>(entity);
        
        if (!position || !health || !health->isAlive) {
            return;
        }
        
        // Set color based on type
        if (spacecraft.type == SpacecraftType::Enemy) {
            glColor3f(1.0F, 0.2F, 0.2F); // Red for enemies
        } else {
            // Check if selected
            auto* selectable = mRegistry.GetComponent<Selectable>(entity);
            if (selectable && selectable->isSelected) {
                glColor3f(0.0F, 1.0F, 0.0F); // Green for selected
            } else {
                glColor3f(1.0F, 0.8F, 0.2F); // Yellow for player
            }
        }
        
        DrawTriangle(position->posX, position->posY, spacecraft.angle);
        
        // Draw health bar
        constexpr float HEALTH_BAR_WIDTH = 0.08F;
        constexpr float HEALTH_BAR_HEIGHT = 0.012F;
        constexpr float HEALTH_BAR_OFFSET = 0.045F;
        
        float healthPercent = static_cast<float>(health->currentHP) / static_cast<float>(health->maxHP);
        DrawHealthBar(
            position->posX - HEALTH_BAR_WIDTH / 2.0F,
            position->posY + HEALTH_BAR_OFFSET,
            HEALTH_BAR_WIDTH,
            HEALTH_BAR_HEIGHT,
            healthPercent
        );
    });
}

void RenderSystem::RenderPlanets() {
    using namespace Components;
    
    glColor3f(0.2F, 0.6F, 1.0F); // Blue for planets
    
    mRegistry.ForEach<Planet>([this](EntityID entity, Planet& planet) {
        auto* position = mRegistry.GetComponent<Position>(entity);
        if (!position) {
            return;
        }
        
        DrawCircle(position->posX, position->posY, planet.radius);
        
        // Draw selection highlight if selected
        auto* selectable = mRegistry.GetComponent<Selectable>(entity);
        if (selectable && selectable->isSelected) {
            glColor3f(0.8F, 0.8F, 0.2F); // Yellow highlight
            glLineWidth(4.0F);
            glBegin(GL_LINE_LOOP);
            for (int i = 0; i < CIRCLE_SEGMENTS; ++i) {
                float theta = 2.0F * 3.14159F * static_cast<float>(i) / static_cast<float>(CIRCLE_SEGMENTS);
                glVertex2f(
                    position->posX + (planet.radius + 0.02F) * std::cos(theta),
                    position->posY + (planet.radius + 0.02F) * std::sin(theta)
                );
            }
            glEnd();
            glColor3f(0.2F, 0.6F, 1.0F); // Reset color
        }
    });
}

void RenderSystem::RenderProjectiles() {
    using namespace Components;
    
    glColor3f(1.0F, 1.0F, 1.0F); // White for projectiles
    
    mRegistry.ForEach<Projectile>([this](EntityID entity, Projectile& projectile) {
        if (!projectile.isActive) {
            return;
        }
        
        auto* position = mRegistry.GetComponent<Position>(entity);
        if (!position) {
            return;
        }
        
        constexpr float PROJECTILE_RADIUS = 0.012F;
        DrawCircle(position->posX, position->posY, PROJECTILE_RADIUS);
    });
}

void RenderSystem::RenderSelectionBoxes() {
    using namespace Components;
    
    glColor3f(0.0F, 1.0F, 0.0F); // Green for selection
    glLineWidth(2.0F);
    
    mRegistry.ForEach<Selectable>([this](EntityID entity, Selectable& selectable) {
        if (!selectable.isSelected) {
            return;
        }
        
        auto* position = mRegistry.GetComponent<Position>(entity);
        if (!position) {
            return;
        }
        
        // Draw selection circle
        glBegin(GL_LINE_LOOP);
        for (int i = 0; i < CIRCLE_SEGMENTS; ++i) {
            float theta = 2.0F * 3.14159F * static_cast<float>(i) / static_cast<float>(CIRCLE_SEGMENTS);
            glVertex2f(
                position->posX + selectable.selectionRadius * std::cos(theta),
                position->posY + selectable.selectionRadius * std::sin(theta)
            );
        }
        glEnd();
    });
}

void RenderSystem::SetDragSelectionBox(int startX, int startY, int endX, int endY, bool active) {
    mDragSelectionActive = active;
    mDragStartX = startX;
    mDragStartY = startY;
    mDragEndX = endX;
    mDragEndY = endY;
}

void RenderSystem::RenderDragSelectionBox() {
    if (!mDragSelectionActive) {
        return;
    }
    
    // Convert screen coordinates to world coordinates  
    float worldStartX = (static_cast<float>(mDragStartX) / static_cast<float>(mWindowWidth)) * 2.0F - 1.0F;
    float worldStartY = -((static_cast<float>(mDragStartY) / static_cast<float>(mWindowHeight)) * 2.0F * WORLD_ASPECT_RATIO - WORLD_ASPECT_RATIO);
    float worldEndX = (static_cast<float>(mDragEndX) / static_cast<float>(mWindowWidth)) * 2.0F - 1.0F;  
    float worldEndY = -((static_cast<float>(mDragEndY) / static_cast<float>(mWindowHeight)) * 2.0F * WORLD_ASPECT_RATIO - WORLD_ASPECT_RATIO);
    
    // Draw selection box outline
    glColor3f(0.0F, 1.0F, 0.0F); // Green
    glLineWidth(2.0F);
    glBegin(GL_LINE_LOOP);
    glVertex2f(worldStartX, worldStartY);
    glVertex2f(worldEndX, worldStartY);
    glVertex2f(worldEndX, worldEndY);
    glVertex2f(worldStartX, worldEndY);
    glEnd();
    
    // Draw semi-transparent fill
    glColor4f(0.0F, 1.0F, 0.0F, 0.1F); // Green with alpha
    glBegin(GL_QUADS);
    glVertex2f(worldStartX, worldStartY);
    glVertex2f(worldEndX, worldStartY);
    glVertex2f(worldEndX, worldEndY);
    glVertex2f(worldStartX, worldEndY);
    glEnd();
}

void RenderSystem::DrawCircle(float centerX, float centerY, float radius) {
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(centerX, centerY);
    for (int i = 0; i <= CIRCLE_SEGMENTS; ++i) {
        float theta = 2.0F * 3.14159F * static_cast<float>(i) / static_cast<float>(CIRCLE_SEGMENTS);
        glVertex2f(centerX + radius * std::cos(theta), centerY + radius * std::sin(theta));
    }
    glEnd();
}

void RenderSystem::DrawTriangle(float posX, float posY, float angle) {
    glPushMatrix();
    glTranslatef(posX, posY, 0.0F);
    glRotatef(angle, 0.0F, 0.0F, 1.0F);
    glBegin(GL_TRIANGLES);
    glVertex2f(0.0F, TRIANGLE_SIZE);
    glVertex2f(-TRIANGLE_SIZE, -TRIANGLE_SIZE);
    glVertex2f(TRIANGLE_SIZE, -TRIANGLE_SIZE);
    glEnd();
    glPopMatrix();
}

void RenderSystem::DrawHealthBar(float posX, float posY, float width, float height, float healthPercent) {
    // Background (gray)
    glColor3f(0.3F, 0.3F, 0.3F);
    glBegin(GL_QUADS);
    glVertex2f(posX, posY);
    glVertex2f(posX + width, posY);
    glVertex2f(posX + width, posY + height);
    glVertex2f(posX, posY + height);
    glEnd();
    
    // Foreground (green)
    glColor3f(0.2F, 1.0F, 0.2F);
    float actualWidth = width * healthPercent;
    glBegin(GL_QUADS);
    glVertex2f(posX, posY);
    glVertex2f(posX + actualWidth, posY);
    glVertex2f(posX + actualWidth, posY + height);
    glVertex2f(posX, posY + height);
    glEnd();
}

bool RenderSystem::InitializeTextRendering() {
    if (mTextRenderingInitialized) {
        return true;
    }
    
    // Initialize FreeType
    if (FT_Init_FreeType(&ftLibrary) != 0) {
        std::cerr << "ERROR::FREETYPE: Could not init FreeType Library\n";
        return false;
    }
    
    // Try to load a font
    const char* fontPaths[] = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf", 
        "/System/Library/Fonts/Arial.ttf",
        "/Windows/Fonts/arial.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf"
    };
    
    bool fontLoaded = false;
    for (const char* fontPath : fontPaths) {
        if (FT_New_Face(ftLibrary, fontPath, 0, &ftFace) == 0) {
            fontLoaded = true;
            break;
        }
    }
    
    if (!fontLoaded) {
        std::cerr << "ERROR::FREETYPE: Failed to load any font\n";
        FT_Done_FreeType(ftLibrary);
        return false;
    }
    
    // Set font size
    constexpr unsigned int FONT_SIZE = 48;
    FT_Set_Pixel_Sizes(ftFace, 0, FONT_SIZE);
    
    // Disable byte-alignment restriction
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    
    // Load first 128 ASCII characters
    constexpr unsigned int CHAR_COUNT = 128;
    for (unsigned char character = 0; character < CHAR_COUNT; character++) {
        if (FT_Load_Char(ftFace, character, FT_LOAD_RENDER) != 0) {
            std::cerr << "ERROR::FREETYPE: Failed to load Glyph " << character << '\n';
            continue;
        }
        
        unsigned int texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_RED,
            static_cast<int>(ftFace->glyph->bitmap.width),
            static_cast<int>(ftFace->glyph->bitmap.rows),
            0,
            GL_RED,
            GL_UNSIGNED_BYTE,
            ftFace->glyph->bitmap.buffer
        );
        
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        
        Character ch = {
            texture,
            static_cast<int>(ftFace->glyph->bitmap.width),
            static_cast<int>(ftFace->glyph->bitmap.rows),
            ftFace->glyph->bitmap_left,
            ftFace->glyph->bitmap_top,
            static_cast<unsigned int>(ftFace->glyph->advance.x)
        };
        characters.insert(std::pair<char, Character>(static_cast<char>(character), ch));
    }
    
    FT_Done_Face(ftFace);
    FT_Done_FreeType(ftLibrary);
    
    mTextRenderingInitialized = true;
    return true;
}

void RenderSystem::CleanupTextRendering() {
    if (!mTextRenderingInitialized) {
        return;
    }
    
    for (auto& pair : characters) {
        glDeleteTextures(1, &pair.second.textureID);
    }
    characters.clear();
    
    mTextRenderingInitialized = false;
}

void RenderSystem::RenderText(const std::string& text, float posX, float posY, float size, float red, float green, float blue) {
    if (!mTextRenderingInitialized) {
        return;
    }
    
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_TEXTURE_2D);
    
    glColor3f(red, green, blue);
    
    constexpr float BASE_SIZE = 48.0F;
    float scale = size / BASE_SIZE;
    float currentX = posX;
    
    for (char character : text) {
        Character characterData = characters[character];
        
        float xPos = currentX + static_cast<float>(characterData.bearingX) * scale;
        float yPos = posY - static_cast<float>(characterData.height - characterData.bearingY) * scale;
        
        float width = static_cast<float>(characterData.width) * scale;
        float height = static_cast<float>(characterData.height) * scale;
        
        glBindTexture(GL_TEXTURE_2D, characterData.textureID);
        
        glBegin(GL_QUADS);
        glTexCoord2f(0.0F, 0.0F); glVertex2f(xPos, yPos + height);
        glTexCoord2f(1.0F, 0.0F); glVertex2f(xPos + width, yPos + height);
        glTexCoord2f(1.0F, 1.0F); glVertex2f(xPos + width, yPos);
        glTexCoord2f(0.0F, 1.0F); glVertex2f(xPos, yPos);
        glEnd();
        
        constexpr int ADVANCE_SHIFT = 6; // 2^6 = 64
        currentX += static_cast<float>(characterData.advance >> ADVANCE_SHIFT) * scale;
    }
    
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
}

void RenderSystem::RenderTextCentered(const std::string& text, float posX, float posY, float size, float red, float green, float blue) {
    if (!mTextRenderingInitialized) {
        return;
    }
    
    constexpr float BASE_SIZE = 48.0F;
    float scale = size / BASE_SIZE;
    float textWidth = 0.0F;
    
    for (char character : text) {
        Character characterData = characters[character];
        constexpr int ADVANCE_SHIFT = 6;
        textWidth += static_cast<float>(characterData.advance >> ADVANCE_SHIFT) * scale;
    }
    
    RenderText(text, posX - textWidth / 2.0F, posY, size, red, green, blue);
}

void RenderSystem::RenderTextUI(const std::string& text, int screenX, int screenY, int size, float red, float green, float blue) {
    if (!mTextRenderingInitialized) {
        return;
    }
    
    constexpr float SCREEN_WIDTH_HALF = 800.0F;
    constexpr float OPENGL_HEIGHT_SCALE = 0.75F;
    
    float glX = (static_cast<float>(screenX) / SCREEN_WIDTH_HALF) - 1.0F;
    float glY = OPENGL_HEIGHT_SCALE - (static_cast<float>(screenY) / SCREEN_WIDTH_HALF);
    
    float worldSize = static_cast<float>(size) / SCREEN_WIDTH_HALF;
    
    RenderText(text, glX, glY, worldSize, red, green, blue);
}

void RenderSystem::RenderTextUIWhite(const std::string& text, int screenX, int screenY, int size) {
    RenderTextUI(text, screenX, screenY, size, 1.0F, 1.0F, 1.0F);
}

void RenderSystem::RenderTextUIGreen(const std::string& text, int screenX, int screenY, int size) {
    RenderTextUI(text, screenX, screenY, size, 0.0F, 1.0F, 0.0F);
}

void RenderSystem::RenderTextUIRed(const std::string& text, int screenX, int screenY, int size) {
    RenderTextUI(text, screenX, screenY, size, 1.0F, 0.0F, 0.0F);
}

void RenderSystem::RenderTextUIYellow(const std::string& text, int screenX, int screenY, int size) {
    RenderTextUI(text, screenX, screenY, size, 1.0F, 1.0F, 0.0F);
}
