#include "Renderer.h"
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

Renderer::Renderer(ECSRegistry& registry)
    : mRegistry(registry)
    , mWindowWidth(0)  // Will be set in Initialize()
    , mWindowHeight(0) // Will be set in Initialize()
    , mTextRenderingInitialized(false)
    , mDragSelectionActive(false)
    , mDragStartX(0)
    , mDragStartY(0)
    , mDragEndX(0)
    , mDragEndY(0)
{
}

Renderer::~Renderer() {
    Shutdown();
}

bool Renderer::Initialize(int windowWidth, int windowHeight) {
    mWindowWidth = windowWidth;
    mWindowHeight = windowHeight;
    
    SetupOpenGL();
    
    if (!InitializeTextRendering()) {
        SDL_Log("Warning: Failed to initialize text rendering");
    }
    
    SDL_Log("Renderer initialized");
    return true;
}

void Renderer::Shutdown() {
    CleanupTextRendering();
    SDL_Log("Renderer shutdown");
}

void Renderer::BeginFrame() {
    glClearColor(0.0F, 0.0F, 0.0F, 1.0F);
    glClear(GL_COLOR_BUFFER_BIT);
    
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-1.0, 1.0, -WORLD_ASPECT_RATIO, WORLD_ASPECT_RATIO, -1.0, 1.0);
    
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

void Renderer::RenderWorld() {
    RenderPlanets();
    RenderSpacecraft();
    RenderProjectiles();
    RenderSelectionBoxes();
    RenderDragSelectionBox();
}

void Renderer::RenderUI() {
    // UI rendering will be handled by UISystem
    // This method is kept for interface compatibility
}

void Renderer::EndFrame() {
    // Frame ending is handled by the main game loop
}

void Renderer::OnWindowResize(int newWidth, int newHeight) {
    mWindowWidth = newWidth;
    mWindowHeight = newHeight;
    glViewport(0, 0, newWidth, newHeight);
}

void Renderer::SetupOpenGL() {
    glViewport(0, 0, mWindowWidth, mWindowHeight);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void Renderer::RenderSpacecraft() {
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
        
        // Draw AI state text above health bar (only for enemy units)
        if (spacecraft.type == SpacecraftType::Enemy) {
            constexpr float AI_STATE_TEXT_OFFSET = 0.07F;
            constexpr float AI_STATE_TEXT_SIZE = 0.02F;
            
            std::string aiStateText = GetAIStateString(spacecraft.aiState);
            
            // Use different colors for different states
            switch (spacecraft.aiState) {
                case Components::AIState::Search:
                    RenderTextCentered(aiStateText, position->posX, position->posY + AI_STATE_TEXT_OFFSET, 
                                     AI_STATE_TEXT_SIZE, 0.7F, 0.7F, 0.7F); // Gray
                    break;
                case Components::AIState::Approach:
                    RenderTextCentered(aiStateText, position->posX, position->posY + AI_STATE_TEXT_OFFSET, 
                                     AI_STATE_TEXT_SIZE, 1.0F, 1.0F, 0.0F); // Yellow
                    break;
                case Components::AIState::Engage:
                    RenderTextCentered(aiStateText, position->posX, position->posY + AI_STATE_TEXT_OFFSET, 
                                     AI_STATE_TEXT_SIZE, 1.0F, 0.0F, 0.0F); // Red
                    break;
                case Components::AIState::Retreat:
                    RenderTextCentered(aiStateText, position->posX, position->posY + AI_STATE_TEXT_OFFSET, 
                                     AI_STATE_TEXT_SIZE, 0.0F, 0.0F, 1.0F); // Blue
                    break;
                case Components::AIState::Regroup:
                    RenderTextCentered(aiStateText, position->posX, position->posY + AI_STATE_TEXT_OFFSET, 
                                     AI_STATE_TEXT_SIZE, 0.0F, 1.0F, 0.0F); // Green
                    break;
            }
        }
    });
}

void Renderer::RenderPlanets() {
    using namespace Components;
    
    mRegistry.ForEach<Planet>([this](EntityID entity, Planet& planet) {
        auto* position = mRegistry.GetComponent<Position>(entity);
        auto* renderable = mRegistry.GetComponent<Renderable>(entity);
        auto* health = mRegistry.GetComponent<Health>(entity);
        
        if (!position || !renderable) {
            return;
        }
        
        // Use renderable component for color
        glColor3f(renderable->red, renderable->green, renderable->blue);
        DrawCircle(position->posX, position->posY, planet.radius);
        
        // Draw health bar for planets
        if (health && health->isAlive) {
            constexpr float PLANET_HEALTH_BAR_WIDTH = 0.12F;
            constexpr float PLANET_HEALTH_BAR_HEIGHT = 0.015F;
            float planetHealthBarOffset = planet.radius + 0.05F;
            
            float healthPercent = static_cast<float>(health->currentHP) / static_cast<float>(health->maxHP);
            DrawHealthBar(
                position->posX - PLANET_HEALTH_BAR_WIDTH / 2.0F,
                position->posY + planetHealthBarOffset,
                PLANET_HEALTH_BAR_WIDTH,
                PLANET_HEALTH_BAR_HEIGHT,
                healthPercent
            );
        }
        
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
        }
    });
}

void Renderer::RenderProjectiles() {
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

void Renderer::RenderSelectionBoxes() {
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

void Renderer::SetDragSelectionBox(int startX, int startY, int endX, int endY, bool active) {
    mDragSelectionActive = active;
    mDragStartX = startX;
    mDragStartY = startY;
    mDragEndX = endX;
    mDragEndY = endY;
}

void Renderer::RenderDragSelectionBox() {
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
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0.0F, 1.0F, 0.0F, 0.1F); // Green with alpha
    glBegin(GL_QUADS);
    glVertex2f(worldStartX, worldStartY);
    glVertex2f(worldEndX, worldStartY);
    glVertex2f(worldEndX, worldEndY);
    glVertex2f(worldStartX, worldEndY);
    glEnd();
    glDisable(GL_BLEND);
}

void Renderer::DrawCircle(float centerX, float centerY, float radius) {
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(centerX, centerY);
    for (int i = 0; i <= CIRCLE_SEGMENTS; ++i) {
        float theta = 2.0F * 3.14159F * static_cast<float>(i) / static_cast<float>(CIRCLE_SEGMENTS);
        glVertex2f(centerX + radius * std::cos(theta), centerY + radius * std::sin(theta));
    }
    glEnd();
}

std::string Renderer::GetAIStateString(Components::AIState state) const {
    switch (state) {
        case Components::AIState::Search:
            return "SEARCH";
        case Components::AIState::Approach:
            return "APPROACH";
        case Components::AIState::Engage:
            return "ENGAGE";
        case Components::AIState::Retreat:
            return "RETREAT";
        case Components::AIState::Regroup:
            return "REGROUP";
        default:
            return "UNKNOWN";
    }
}

void Renderer::DrawTriangle(float posX, float posY, float angle) {
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

void Renderer::DrawHealthBar(float posX, float posY, float width, float height, float healthPercent) {
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

bool Renderer::InitializeTextRendering() {
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

void Renderer::CleanupTextRendering() {
    if (!mTextRenderingInitialized) {
        return;
    }
    
    for (auto& pair : characters) {
        glDeleteTextures(1, &pair.second.textureID);
    }
    characters.clear();
    
    mTextRenderingInitialized = false;
}

void Renderer::RenderText(const std::string& text, float posX, float posY, float size, float red, float green, float blue) {
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

void Renderer::RenderTextCentered(const std::string& text, float posX, float posY, float size, float red, float green, float blue) {
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

void Renderer::RenderTextUI(const std::string& text, int screenX, int screenY, int size, float red, float green, float blue) {
    if (!mTextRenderingInitialized) {
        return;
    }
    
    constexpr float HALF_DIVISOR = 2.0F;
    float screenWidthHalf = static_cast<float>(mWindowWidth) / HALF_DIVISOR;
    constexpr float OPENGL_HEIGHT_SCALE = 0.75F;
    
    float glX = (static_cast<float>(screenX) / screenWidthHalf) - 1.0F;
    float glY = OPENGL_HEIGHT_SCALE - (static_cast<float>(screenY) / screenWidthHalf);
    
    float worldSize = static_cast<float>(size) / screenWidthHalf;
    
    RenderText(text, glX, glY, worldSize, red, green, blue);
}

void Renderer::RenderTextUIWhite(const std::string& text, int screenX, int screenY, int size) {
    RenderTextUI(text, screenX, screenY, size, 1.0F, 1.0F, 1.0F);
}

void Renderer::RenderTextUIGreen(const std::string& text, int screenX, int screenY, int size) {
    RenderTextUI(text, screenX, screenY, size, 0.0F, 1.0F, 0.0F);
}

void Renderer::RenderTextUIRed(const std::string& text, int screenX, int screenY, int size) {
    RenderTextUI(text, screenX, screenY, size, 1.0F, 0.0F, 0.0F);
}

void Renderer::RenderTextUIYellow(const std::string& text, int screenX, int screenY, int size) {
    RenderTextUI(text, screenX, screenY, size, 1.0F, 1.0F, 0.0F);
}

void Renderer::DrawGridBorder(float posX, float posY, float size) {
    // Draw grid border using OpenGL rectangles
    glColor3f(1.0F, 1.0F, 0.0F); // Yellow border
    glLineWidth(2.0F);
    
    // Draw border rectangle
    glBegin(GL_LINE_LOOP);
    glVertex2f(posX, posY + size/2);
    glVertex2f(posX + size, posY + size/2);
    glVertex2f(posX + size, posY - size/2);
    glVertex2f(posX, posY - size/2);
    glEnd();
    
    // Draw grid lines
    glLineWidth(1.0F);
    glColor3f(0.5F, 0.5F, 0.0F); // Darker yellow for grid
    
    // Vertical line
    glBegin(GL_LINES);
    glVertex2f(posX + size/2, posY + size/2);
    glVertex2f(posX + size/2, posY - size/2);
    glEnd();
    
    // Horizontal line  
    glBegin(GL_LINES);
    glVertex2f(posX, posY);
    glVertex2f(posX + size, posY);
    glEnd();
}

void Renderer::RenderBuildIcon(float posX, float posY, float size, Components::BuildableUnit unitType, int queueCount) {
    // Draw icon background (filled rectangle)
    glColor3f(0.3F, 0.3F, 0.3F); // Dark gray background
    glBegin(GL_QUADS);
    glVertex2f(posX - size/2, posY + size/2);
    glVertex2f(posX + size/2, posY + size/2);
    glVertex2f(posX + size/2, posY - size/2);
    glVertex2f(posX - size/2, posY - size/2);
    glEnd();
    
    // Draw icon border
    glColor3f(0.6F, 0.6F, 0.6F); // Light gray border
    glLineWidth(2.0F);
    glBegin(GL_LINE_LOOP);
    glVertex2f(posX - size/2, posY + size/2);
    glVertex2f(posX + size/2, posY + size/2);
    glVertex2f(posX + size/2, posY - size/2);
    glVertex2f(posX - size/2, posY - size/2);
    glEnd();
    
    // For now, assume unitType 0 = Spacecraft, but we'll make this more generic
    if (unitType == Components::BuildableUnit::Spacecraft) {
        // Draw triangle icon (like spacecraft) - filled triangle
        glColor3f(1.0F, 1.0F, 0.2F); // Yellow triangle
        glBegin(GL_TRIANGLES);
        glVertex2f(posX, posY + size/3);          // Top point
        glVertex2f(posX - size/4, posY - size/3); // Bottom left
        glVertex2f(posX + size/4, posY - size/3); // Bottom right
        glEnd();
        
        // Draw triangle outline
        glColor3f(1.0F, 0.8F, 0.0F); // Darker yellow outline
        glLineWidth(1.5F);
        glBegin(GL_LINE_LOOP);
        glVertex2f(posX, posY + size/3);
        glVertex2f(posX - size/4, posY - size/3);
        glVertex2f(posX + size/4, posY - size/3);
        glEnd();
    } else {
        // Draw question mark or placeholder
        glColor3f(0.7F, 0.7F, 0.7F);
        RenderText("?", posX - 0.01F, posY, 0.025F, 0.7F, 0.7F, 0.7F);
    }
    
    // Show queue count if any - positioned lower in the icon
    if (queueCount > 0) {
        std::string queueText = std::to_string(queueCount);
        constexpr float QUEUE_TEXT_SIZE = 0.05F; // Larger size for better visibility
        // Position text lower and to the right in the icon
        RenderText(queueText, posX + (size/4), posY - (size/4), QUEUE_TEXT_SIZE, 1.0F, 1.0F, 0.0F);
    }
}

void Renderer::RenderEmptyIcon(float posX, float posY, float size) {
    // Draw empty icon slot background
    glColor3f(0.2F, 0.2F, 0.2F); // Very dark gray background
    glBegin(GL_QUADS);
    glVertex2f(posX - size/2, posY + size/2);
    glVertex2f(posX + size/2, posY + size/2);
    glVertex2f(posX + size/2, posY - size/2);
    glVertex2f(posX - size/2, posY - size/2);
    glEnd();
    
    // Draw empty icon border (dashed effect using smaller rectangles)
    glColor3f(0.4F, 0.4F, 0.4F); // Gray border
    glLineWidth(1.0F);
    glBegin(GL_LINE_LOOP);
    glVertex2f(posX - size/2, posY + size/2);
    glVertex2f(posX + size/2, posY + size/2);
    glVertex2f(posX + size/2, posY - size/2);
    glVertex2f(posX - size/2, posY - size/2);
    glEnd();
    
    // Draw dash in center to indicate empty slot
    glColor3f(0.4F, 0.4F, 0.4F);
    glLineWidth(3.0F);
    glBegin(GL_LINES);
    glVertex2f(posX - size/4, posY);
    glVertex2f(posX + size/4, posY);
    glEnd();
}

void Renderer::RenderUnitSelectionPanel(float posX, float posY, float width, float height) {
    // Draw panel background
    glColor4f(0.1F, 0.1F, 0.1F, 0.8F); // Dark background with transparency
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBegin(GL_QUADS);
    glVertex2f(posX - width/2, posY - height/2);
    glVertex2f(posX + width/2, posY - height/2);
    glVertex2f(posX + width/2, posY + height/2);
    glVertex2f(posX - width/2, posY + height/2);
    glEnd();
    glDisable(GL_BLEND);
    
    // Draw panel border
    glColor3f(0.6F, 0.6F, 0.6F); // Light gray border
    glLineWidth(2.0F);
    glBegin(GL_LINE_LOOP);
    glVertex2f(posX - width/2, posY - height/2);
    glVertex2f(posX + width/2, posY - height/2);
    glVertex2f(posX + width/2, posY + height/2);
    glVertex2f(posX - width/2, posY + height/2);
    glEnd();
}

void Renderer::RenderSelectedUnitIcon(float posX, float posY, float size, Components::SpacecraftType unitType, int count, float healthPercent) {
    // Draw icon background
    glColor3f(0.2F, 0.2F, 0.2F); // Dark gray background
    glBegin(GL_QUADS);
    glVertex2f(posX - size/2, posY + size/2);
    glVertex2f(posX + size/2, posY + size/2);
    glVertex2f(posX + size/2, posY - size/2);
    glVertex2f(posX - size/2, posY - size/2);
    glEnd();
    
    // Draw icon border
    glColor3f(0.5F, 0.5F, 0.5F); // Gray border
    glLineWidth(1.5F);
    glBegin(GL_LINE_LOOP);
    glVertex2f(posX - size/2, posY + size/2);
    glVertex2f(posX + size/2, posY + size/2);
    glVertex2f(posX + size/2, posY - size/2);
    glVertex2f(posX - size/2, posY - size/2);
    glEnd();
    
    // Draw unit icon based on type
    if (unitType == Components::SpacecraftType::Player) {
        // Draw triangle icon (like spacecraft) - filled triangle
        glColor3f(1.0F, 0.8F, 0.2F); // Yellow triangle
        glBegin(GL_TRIANGLES);
        glVertex2f(posX, posY + size/3);          // Top point
        glVertex2f(posX - size/4, posY - size/3); // Bottom left
        glVertex2f(posX + size/4, posY - size/3); // Bottom right
        glEnd();
        
        // Draw triangle outline
        glColor3f(1.0F, 0.6F, 0.0F); // Darker yellow outline
        glLineWidth(1.0F);
        glBegin(GL_LINE_LOOP);
        glVertex2f(posX, posY + size/3);
        glVertex2f(posX - size/4, posY - size/3);
        glVertex2f(posX + size/4, posY - size/3);
        glEnd();
    } else if (unitType == Components::SpacecraftType::Enemy) {
        // Draw different icon for enemy units
        glColor3f(1.0F, 0.2F, 0.2F); // Red triangle
        glBegin(GL_TRIANGLES);
        glVertex2f(posX, posY + size/3);
        glVertex2f(posX - size/4, posY - size/3);
        glVertex2f(posX + size/4, posY - size/3);
        glEnd();
    }
    
    // Draw health bar below the icon
    float healthBarWidth = size * 0.8F;
    float healthBarHeight = size * 0.1F;
    float healthBarY = posY - size/2 - healthBarHeight - 0.01F;
    
    // Health bar background
    glColor3f(0.3F, 0.3F, 0.3F);
    glBegin(GL_QUADS);
    glVertex2f(posX - healthBarWidth/2, healthBarY);
    glVertex2f(posX + healthBarWidth/2, healthBarY);
    glVertex2f(posX + healthBarWidth/2, healthBarY + healthBarHeight);
    glVertex2f(posX - healthBarWidth/2, healthBarY + healthBarHeight);
    glEnd();
    
    // Health bar fill
    float healthColor = healthPercent > 0.5F ? 0.2F + (healthPercent - 0.5F) : 0.2F;
    glColor3f(1.0F - healthPercent + 0.2F, healthColor + healthPercent * 0.8F, 0.2F);
    float actualWidth = healthBarWidth * healthPercent;
    glBegin(GL_QUADS);
    glVertex2f(posX - healthBarWidth/2, healthBarY);
    glVertex2f(posX - healthBarWidth/2 + actualWidth, healthBarY);
    glVertex2f(posX - healthBarWidth/2 + actualWidth, healthBarY + healthBarHeight);
    glVertex2f(posX - healthBarWidth/2, healthBarY + healthBarHeight);
    glEnd();
    
    // Show count if more than 1
    if (count > 1) {
        std::string countText = std::to_string(count);
        constexpr float COUNT_TEXT_SIZE = 0.03F;
        // Position count in bottom-right corner of icon
        RenderText(countText, posX + size/3, posY - size/3, COUNT_TEXT_SIZE, 1.0F, 1.0F, 0.0F);
    }
}
