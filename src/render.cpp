#include "render.h"
#include <ft2build.h>
#include FT_FREETYPE_H
#include <unordered_map>
#include <iostream>
#include <cmath>

// FreeType text rendering system
struct Character {
    unsigned int textureID;  // ID handle of the glyph texture
    int width;              // Width of glyph
    int height;             // Height of glyph  
    int bearingX;           // Offset from baseline to left/top of glyph
    int bearingY;           // Offset from baseline to left/top of glyph
    unsigned int advance;   // Offset to advance to next glyph
};

static FT_Library ftLibrary;
static FT_Face ftFace;
static std::unordered_map<char, Character> characters;
static bool textRenderingInitialized = false;

bool initTextRendering() {
    if (textRenderingInitialized) {
        return true;
    }
    
    // Initialize FreeType
    if (FT_Init_FreeType(&ftLibrary) != 0) {
        std::cerr << "ERROR::FREETYPE: Could not init FreeType Library" << std::endl;
        return false;
    }
    
    // Load font (try system font first, fallback to common locations)
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
        std::cerr << "ERROR::FREETYPE: Failed to load any font" << std::endl;
        FT_Done_FreeType(ftLibrary);
        return false;
    }
    
    // Set font size (width=0 means dynamically calculated)
    FT_Set_Pixel_Sizes(ftFace, 0, 48);
    
    // Disable byte-alignment restriction
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    
    // Load first 128 ASCII characters
    for (unsigned char character = 0; character < 128; character++) {
        // Load character glyph
        if (FT_Load_Char(ftFace, character, FT_LOAD_RENDER) != 0) {
            std::cerr << "ERROR::FREETYPE: Failed to load Glyph " << character << std::endl;
            continue;
        }
        
        // Generate texture
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
        
        // Set texture options
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        
        // Store character for later use
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
    
    // Clean up FreeType resources
    FT_Done_Face(ftFace);
    FT_Done_FreeType(ftLibrary);
    
    textRenderingInitialized = true;
    return true;
}

void cleanupTextRendering() {
    if (!textRenderingInitialized) {
        return;
    }
    
    // Clean up all character textures
    for (auto& pair : characters) {
        glDeleteTextures(1, &pair.second.textureID);
    }
    characters.clear();
    
    textRenderingInitialized = false;
}

void renderText(const std::string& text, float posX, float posY, float size, float red, float green, float blue) {
    if (!textRenderingInitialized) {
        return;
    }
    
    // Enable blending for transparency
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_TEXTURE_2D);
    
    glColor3f(red, green, blue);
    
    // Calculate scale factor based on desired size
    constexpr float BASE_SIZE = 48.0F; // Base font size we loaded
    float scale = size / BASE_SIZE;
    
    float currentX = posX;
    
    for (char character : text) {
        Character ch = characters[character];
        
        float xPos = currentX + static_cast<float>(ch.bearingX) * scale;
        float yPos = posY - static_cast<float>(ch.height - ch.bearingY) * scale;
        
        float width = static_cast<float>(ch.width) * scale;
        float height = static_cast<float>(ch.height) * scale;
        
        // Update VBO for each character
        glBindTexture(GL_TEXTURE_2D, ch.textureID);
        
        glBegin(GL_QUADS);
        glTexCoord2f(0.0F, 0.0F); glVertex2f(xPos, yPos + height);
        glTexCoord2f(1.0F, 0.0F); glVertex2f(xPos + width, yPos + height);
        glTexCoord2f(1.0F, 1.0F); glVertex2f(xPos + width, yPos);
        glTexCoord2f(0.0F, 1.0F); glVertex2f(xPos, yPos);
        glEnd();
        
        // Advance cursors for next glyph (note that advance is number of 1/64 pixels)
        currentX += static_cast<float>(ch.advance >> 6) * scale; // Bitshift by 6 to get value in pixels (2^6 = 64)
    }
    
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
}

void renderTextCentered(const std::string& text, float posX, float posY, float size, float red, float green, float blue) {
    if (!textRenderingInitialized) {
        return;
    }
    
    // Calculate text width
    constexpr float BASE_SIZE = 48.0F;
    float scale = size / BASE_SIZE;
    float textWidth = 0.0F;
    
    for (char character : text) {
        Character ch = characters[character];
        textWidth += static_cast<float>(ch.advance >> 6) * scale;
    }
    
    // Render text centered
    renderText(text, posX - textWidth / 2.0F, posY, size, red, green, blue);
}

void renderTextUI(const std::string& text, int screenX, int screenY, int size, float red, float green, float blue) {
    if (!textRenderingInitialized) {
        return;
    }
    
    // Convert screen coordinates to OpenGL world coordinates
    constexpr float SCREEN_WIDTH_HALF = 800.0F;
    constexpr float OPENGL_HEIGHT_SCALE = 0.75F;
    
    float glX = (static_cast<float>(screenX) / SCREEN_WIDTH_HALF) - 1.0F;
    float glY = OPENGL_HEIGHT_SCALE - (static_cast<float>(screenY) / SCREEN_WIDTH_HALF);
    
    // Convert size from screen pixels to world units
    float worldSize = static_cast<float>(size) / SCREEN_WIDTH_HALF;
    
    renderText(text, glX, glY, worldSize, red, green, blue);
}

void renderTextUIWhite(const std::string& text, int screenX, int screenY, int size) {
    renderTextUI(text, screenX, screenY, size, 1.0F, 1.0F, 1.0F);
}

void renderTextUIGreen(const std::string& text, int screenX, int screenY, int size) {
    renderTextUI(text, screenX, screenY, size, 0.0F, 1.0F, 0.0F);
}

void renderTextUIRed(const std::string& text, int screenX, int screenY, int size) {
    renderTextUI(text, screenX, screenY, size, 1.0F, 0.0F, 0.0F);
}

void renderTextUIYellow(const std::string& text, int screenX, int screenY, int size) {
    renderTextUI(text, screenX, screenY, size, 1.0F, 1.0F, 0.0F);
}

void drawCircle(float cx, float cy, float r) {
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(cx, cy);
    for (int i = 0; i <= 32; ++i) {
        float theta = 2.0f * 3.1415926f * float(i) / 32.0f;
        glVertex2f(cx + r * cosf(theta), cy + r * sinf(theta));
    }
    glEnd();
}

void drawTriangle(float x, float y, float angle) {
    float size = 0.03f;
    glPushMatrix();
    glTranslatef(x, y, 0);
    glRotatef(angle, 0, 0, 1);
    glBegin(GL_TRIANGLES);
    glVertex2f(0, size);
    glVertex2f(-size, -size);
    glVertex2f(size, -size);
    glEnd();
    glPopMatrix();
}

void drawHealthBar(const Position& pos, const Health& health, const SpacecraftTag& tag) {
    float barWidth = (tag.type == SpacecraftType::Enemy) ? 0.12f : 0.08f;
    float barHeight = (tag.type == SpacecraftType::Enemy) ? 0.015f : 0.012f;
    float hpFrac = float(health.hp) / 10.0f;
    float barX = pos.x - barWidth/2.0f;
    float barY = pos.y + ((tag.type == SpacecraftType::Enemy) ? 0.06f : 0.045f);
    // Background (gray)
    glColor3f(0.3f, 0.3f, 0.3f);
    glBegin(GL_QUADS);
    glVertex2f(barX, barY);
    glVertex2f(barX + barWidth, barY);
    glVertex2f(barX + barWidth, barY + barHeight);
    glVertex2f(barX, barY + barHeight);
    glEnd();
    // Foreground (green)
    glColor3f(0.2f, 1.0f, 0.2f);
    glBegin(GL_QUADS);
    glVertex2f(barX, barY);
    glVertex2f(barX + barWidth * hpFrac, barY);
    glVertex2f(barX + barWidth * hpFrac, barY + barHeight);
    glVertex2f(barX, barY + barHeight);
    glEnd();
}

void drawSelectionCircle(const Position& pos) {
    glColor3f(0.0f, 1.0f, 0.0f);
    glLineWidth(2.0f);
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < 32; ++i) {
        float theta = 2.0f * 3.1415926f * float(i) / 32.0f;
        glVertex2f(pos.x + 0.04f * cosf(theta), pos.y + 0.04f * sinf(theta));
    }
    glEnd();
}

void drawUIGrid(int gridOriginX, int gridOriginY, int cellSize, int rows, int cols) {
    constexpr float GRID_COLOR = 0.4F;
    constexpr float LINE_WIDTH = 1.0F;
    constexpr float SCREEN_WIDTH_HALF = 800.0F;
    constexpr float OPENGL_HEIGHT_SCALE = 0.75F;
    
    glColor3f(GRID_COLOR, GRID_COLOR, GRID_COLOR); // Gray grid lines
    glLineWidth(LINE_WIDTH);
    
    // Convert screen coordinates to OpenGL coordinates
    // Assuming window is 1600x1200, map to OpenGL coords
    float glLeft = (static_cast<float>(gridOriginX) / SCREEN_WIDTH_HALF) - 1.0F;
    float glRight = (static_cast<float>(gridOriginX + (cols * cellSize)) / SCREEN_WIDTH_HALF) - 1.0F;
    float glBottom = OPENGL_HEIGHT_SCALE - (static_cast<float>(gridOriginY + (rows * cellSize)) / SCREEN_WIDTH_HALF);
    float glTop = OPENGL_HEIGHT_SCALE - (static_cast<float>(gridOriginY) / SCREEN_WIDTH_HALF);
    
    // Draw vertical lines
    glBegin(GL_LINES);
    for (int i = 0; i <= cols; ++i) {
        float xPos = glLeft + ((glRight - glLeft) * static_cast<float>(i) / static_cast<float>(cols));
        glVertex2f(xPos, glBottom);
        glVertex2f(xPos, glTop);
    }
    // Draw horizontal lines
    for (int i = 0; i <= rows; ++i) {
        float yPos = glBottom + ((glTop - glBottom) * static_cast<float>(i) / static_cast<float>(rows));
        glVertex2f(glLeft, yPos);
        glVertex2f(glRight, yPos);
    }
    glEnd();
}

void drawShipIcon(int posX, int posY, int size) {
    constexpr float SCREEN_WIDTH_HALF = 800.0F;
    constexpr float OPENGL_HEIGHT_SCALE = 0.75F;
    constexpr float SHIP_RED = 1.0F;
    constexpr float SHIP_GREEN = 0.8F;
    constexpr float SHIP_BLUE = 0.2F;
    constexpr float ICON_SCALE = 1600.0F;
    constexpr float HALF_DIVISOR = 2.0F;
    
    // Convert screen coordinates to OpenGL coordinates
    float glX = ((static_cast<float>(posX) + (static_cast<float>(size) / HALF_DIVISOR)) / SCREEN_WIDTH_HALF) - 1.0F;
    float glY = OPENGL_HEIGHT_SCALE - ((static_cast<float>(posY) + (static_cast<float>(size) / HALF_DIVISOR)) / SCREEN_WIDTH_HALF);
    float glSize = static_cast<float>(size) / ICON_SCALE; // Scale appropriately
    
    glColor3f(SHIP_RED, SHIP_GREEN, SHIP_BLUE); // Yellow like player ships
    glPushMatrix();
    glTranslatef(glX, glY, 0);
    glBegin(GL_TRIANGLES);
    glVertex2f(0, glSize);
    glVertex2f(-glSize, -glSize);
    glVertex2f(glSize, -glSize);
    glEnd();
    glPopMatrix();
}

auto isPointInRect(int pointX, int pointY, int rectX, int rectY, int rectW, int rectH) -> bool {
    return pointX >= rectX && pointX <= rectX + rectW && 
           pointY >= rectY && pointY <= rectY + rectH;
}

void drawUnitSelectionPanel(int panelX, int panelY, int panelWidth, int panelHeight, int cellSize) {
    constexpr float PANEL_COLOR = 0.2F;
    constexpr float BORDER_COLOR = 0.6F;
    constexpr float LINE_WIDTH = 2.0F;
    constexpr float SCREEN_WIDTH_HALF = 800.0F;
    constexpr float OPENGL_HEIGHT_SCALE = 0.75F;
    
    // Convert screen coordinates to OpenGL coordinates
    float glLeft = (static_cast<float>(panelX) / SCREEN_WIDTH_HALF) - 1.0F;
    float glRight = (static_cast<float>(panelX + panelWidth) / SCREEN_WIDTH_HALF) - 1.0F;
    float glBottom = OPENGL_HEIGHT_SCALE - (static_cast<float>(panelY + panelHeight) / SCREEN_WIDTH_HALF);
    float glTop = OPENGL_HEIGHT_SCALE - (static_cast<float>(panelY) / SCREEN_WIDTH_HALF);
    
    // Draw panel background
    glColor3f(PANEL_COLOR, PANEL_COLOR, PANEL_COLOR);
    glBegin(GL_QUADS);
    glVertex2f(glLeft, glBottom);
    glVertex2f(glRight, glBottom);
    glVertex2f(glRight, glTop);
    glVertex2f(glLeft, glTop);
    glEnd();
    
    // Draw panel border
    glColor3f(BORDER_COLOR, BORDER_COLOR, BORDER_COLOR);
    glLineWidth(LINE_WIDTH);
    glBegin(GL_LINE_LOOP);
    glVertex2f(glLeft, glBottom);
    glVertex2f(glRight, glBottom);
    glVertex2f(glRight, glTop);
    glVertex2f(glLeft, glTop);
    glEnd();
    
    // Draw grid lines for unit slots
    constexpr float GRID_COLOR = 0.4F;
    constexpr float GRID_LINE_WIDTH = 1.0F;
    glColor3f(GRID_COLOR, GRID_COLOR, GRID_COLOR);
    glLineWidth(GRID_LINE_WIDTH);
    
    int cols = panelWidth / cellSize;
    int rows = panelHeight / cellSize;
    
    glBegin(GL_LINES);
    // Vertical lines
    for (int i = 1; i < cols; ++i) {
        float xPos = glLeft + ((glRight - glLeft) * static_cast<float>(i) / static_cast<float>(cols));
        glVertex2f(xPos, glBottom);
        glVertex2f(xPos, glTop);
    }
    // Horizontal lines
    for (int i = 1; i < rows; ++i) {
        float yPos = glBottom + ((glTop - glBottom) * static_cast<float>(i) / static_cast<float>(rows));
        glVertex2f(glLeft, yPos);
        glVertex2f(glRight, yPos);
    }
    glEnd();
}

void drawUnitInPanel(int posX, int posY, int size, float healthPercent, bool isHighlighted) {
    constexpr float SCREEN_WIDTH_HALF = 800.0F;
    constexpr float OPENGL_HEIGHT_SCALE = 0.75F;
    constexpr float SHIP_RED = 1.0F;
    constexpr float SHIP_GREEN = 0.8F;
    constexpr float SHIP_BLUE = 0.2F;
    constexpr float HIGHLIGHT_RED = 0.0F;
    constexpr float HIGHLIGHT_GREEN = 1.0F;
    constexpr float HIGHLIGHT_BLUE = 0.0F;
    constexpr float ICON_SCALE = 1200.0F;
    constexpr float HALF_DIVISOR = 2.0F;
    constexpr float HEALTH_BAR_WIDTH_SCALE = 0.8F;
    constexpr float HEALTH_BAR_HEIGHT = 0.003F;
    constexpr float HEALTH_BAR_OFFSET = 0.02F;
    constexpr float BACKGROUND_HEALTH_RED = 0.3F;
    constexpr float BACKGROUND_HEALTH_GREEN = 0.3F;
    constexpr float BACKGROUND_HEALTH_BLUE = 0.3F;
    constexpr float FOREGROUND_HEALTH_RED = 0.2F;
    constexpr float FOREGROUND_HEALTH_GREEN = 1.0F;
    constexpr float FOREGROUND_HEALTH_BLUE = 0.2F;
    constexpr float HIGHLIGHT_LINE_WIDTH = 3.0F;
    constexpr float HIGHLIGHT_BORDER_OFFSET = 0.01F;
    
    // Convert screen coordinates to OpenGL coordinates
    float glX = ((static_cast<float>(posX) + (static_cast<float>(size) / HALF_DIVISOR)) / SCREEN_WIDTH_HALF) - 1.0F;
    float glY = OPENGL_HEIGHT_SCALE - ((static_cast<float>(posY) + (static_cast<float>(size) / HALF_DIVISOR)) / SCREEN_WIDTH_HALF);
    float glSize = static_cast<float>(size) / ICON_SCALE;
    
    // Draw highlight border if selected
    if (isHighlighted) {
        glColor3f(HIGHLIGHT_RED, HIGHLIGHT_GREEN, HIGHLIGHT_BLUE);
        glLineWidth(HIGHLIGHT_LINE_WIDTH);
        glBegin(GL_LINE_LOOP);
        float borderSize = glSize + HIGHLIGHT_BORDER_OFFSET;
        glVertex2f(glX - borderSize, glY - borderSize);
        glVertex2f(glX + borderSize, glY - borderSize);
        glVertex2f(glX + borderSize, glY + borderSize);
        glVertex2f(glX - borderSize, glY + borderSize);
        glEnd();
    }
    
    // Draw ship icon
    glColor3f(SHIP_RED, SHIP_GREEN, SHIP_BLUE);
    glPushMatrix();
    glTranslatef(glX, glY, 0);
    glBegin(GL_TRIANGLES);
    glVertex2f(0, glSize);
    glVertex2f(-glSize, -glSize);
    glVertex2f(glSize, -glSize);
    glEnd();
    glPopMatrix();
    
    // Draw health bar below the ship
    float healthBarWidth = glSize * HEALTH_BAR_WIDTH_SCALE * HALF_DIVISOR;
    float healthBarX = glX - (healthBarWidth / HALF_DIVISOR);
    float healthBarY = glY - glSize - HEALTH_BAR_OFFSET;
    
    // Background (gray)
    glColor3f(BACKGROUND_HEALTH_RED, BACKGROUND_HEALTH_GREEN, BACKGROUND_HEALTH_BLUE);
    glBegin(GL_QUADS);
    glVertex2f(healthBarX, healthBarY);
    glVertex2f(healthBarX + healthBarWidth, healthBarY);
    glVertex2f(healthBarX + healthBarWidth, healthBarY + HEALTH_BAR_HEIGHT);
    glVertex2f(healthBarX, healthBarY + HEALTH_BAR_HEIGHT);
    glEnd();
    
    // Foreground (green based on health)
    glColor3f(FOREGROUND_HEALTH_RED, FOREGROUND_HEALTH_GREEN, FOREGROUND_HEALTH_BLUE);
    float actualHealthWidth = healthBarWidth * healthPercent;
    glBegin(GL_QUADS);
    glVertex2f(healthBarX, healthBarY);
    glVertex2f(healthBarX + actualHealthWidth, healthBarY);
    glVertex2f(healthBarX + actualHealthWidth, healthBarY + HEALTH_BAR_HEIGHT);
    glVertex2f(healthBarX, healthBarY + HEALTH_BAR_HEIGHT);
    glEnd();
}
