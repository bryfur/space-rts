#include "render.h"

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

void drawNumber(int number, int posX, int posY, int size) {
    if (number <= 0) {
        return;
    }
    
    constexpr float SCREEN_WIDTH_HALF = 800.0F;
    constexpr float OPENGL_HEIGHT_SCALE = 0.75F;
    constexpr float TEXT_RED = 1.0F;
    constexpr float TEXT_GREEN = 1.0F;
    constexpr float TEXT_BLUE = 1.0F;
    constexpr float BACKGROUND_RED = 0.0F;
    constexpr float BACKGROUND_GREEN = 0.0F;
    constexpr float BACKGROUND_BLUE = 0.0F;
    constexpr float BACKGROUND_ALPHA = 0.8F;
    constexpr float DIGIT_SCALE = 400.0F; // Make numbers even bigger
    constexpr float HALF_DIVISOR = 2.0F;
    constexpr float CIRCLE_RADIUS_SCALE = 0.8F;
    constexpr float MATH_PI = 3.1415926F;
    constexpr float TWO_PI = 2.0F * MATH_PI;
    constexpr int CIRCLE_SEGMENTS = 16;
    constexpr float BOLD_LINE_WIDTH = 3.0F;
    constexpr float SEGMENT_WIDTH_SCALE = 0.6F;
    constexpr float SEGMENT_HEIGHT_SCALE = 0.1F;
    constexpr float SEGMENT_THICKNESS_SCALE = 0.08F;
    constexpr int NUM_DIGITS = 10;
    constexpr int NUM_SEGMENTS = 7;
    constexpr int MAX_DIGIT = 9;
    constexpr int SEGMENT_MIDDLE = 6;
    constexpr int SEGMENT_TOP_LEFT = 5;
    constexpr int SEGMENT_BOTTOM = 3;
    
    // Convert screen coordinates to OpenGL coordinates
    float glX = (static_cast<float>(posX) / SCREEN_WIDTH_HALF) - 1.0F;
    float glY = OPENGL_HEIGHT_SCALE - (static_cast<float>(posY) / SCREEN_WIDTH_HALF);
    float digitSize = static_cast<float>(size) / DIGIT_SCALE;
    
    // Draw a dark semi-transparent background circle for contrast
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(BACKGROUND_RED, BACKGROUND_GREEN, BACKGROUND_BLUE, BACKGROUND_ALPHA);
    
    float circleRadius = digitSize * CIRCLE_RADIUS_SCALE;
    float centerX = glX + (digitSize / HALF_DIVISOR);
    float centerY = glY + (digitSize / HALF_DIVISOR);
    
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(centerX, centerY); // Center
    for (int i = 0; i <= CIRCLE_SEGMENTS; ++i) {
        float theta = TWO_PI * static_cast<float>(i) / static_cast<float>(CIRCLE_SEGMENTS);
        glVertex2f(centerX + (circleRadius * cosf(theta)), 
                   centerY + (circleRadius * sinf(theta)));
    }
    glEnd();
    
    glDisable(GL_BLEND);
    
    // Draw the number in bright white
    glColor3f(TEXT_RED, TEXT_GREEN, TEXT_BLUE);
    glLineWidth(BOLD_LINE_WIDTH); // Bold lines
    
    // Simple 7-segment style digit rendering
    float segmentWidth = digitSize * SEGMENT_WIDTH_SCALE;
    float segmentHeight = digitSize * SEGMENT_HEIGHT_SCALE;
    float segmentThickness = digitSize * SEGMENT_THICKNESS_SCALE;
    
    // Define segments for each digit (7-segment display style)
    // Segments: top, top-right, bottom-right, bottom, bottom-left, top-left, middle
    bool segments[NUM_DIGITS][NUM_SEGMENTS] = {
        {true, true, true, true, true, true, false},   // 0
        {false, true, true, false, false, false, false}, // 1
        {true, true, false, true, true, false, true},  // 2
        {true, true, true, true, false, false, true},  // 3
        {false, true, true, false, false, true, true}, // 4
        {true, false, true, true, false, true, true},  // 5
        {true, false, true, true, true, true, true},   // 6
        {true, true, true, false, false, false, false}, // 7
        {true, true, true, true, true, true, true},    // 8
        {true, true, true, true, false, true, true}    // 9
    };
    
    if (number >= 0 && number <= MAX_DIGIT) {
        // Draw horizontal segments (top, middle, bottom)
        if (segments[number][0]) { // top
            glBegin(GL_QUADS);
            glVertex2f(centerX - (segmentWidth / HALF_DIVISOR), centerY + (segmentWidth / HALF_DIVISOR));
            glVertex2f(centerX + (segmentWidth / HALF_DIVISOR), centerY + (segmentWidth / HALF_DIVISOR));
            glVertex2f(centerX + (segmentWidth / HALF_DIVISOR), centerY + (segmentWidth / HALF_DIVISOR) - segmentThickness);
            glVertex2f(centerX - (segmentWidth / HALF_DIVISOR), centerY + (segmentWidth / HALF_DIVISOR) - segmentThickness);
            glEnd();
        }
        if (segments[number][SEGMENT_MIDDLE]) { // middle
            glBegin(GL_QUADS);
            glVertex2f(centerX - (segmentWidth / HALF_DIVISOR), centerY + (segmentThickness / HALF_DIVISOR));
            glVertex2f(centerX + (segmentWidth / HALF_DIVISOR), centerY + (segmentThickness / HALF_DIVISOR));
            glVertex2f(centerX + (segmentWidth / HALF_DIVISOR), centerY - (segmentThickness / HALF_DIVISOR));
            glVertex2f(centerX - (segmentWidth / HALF_DIVISOR), centerY - (segmentThickness / HALF_DIVISOR));
            glEnd();
        }
        if (segments[number][SEGMENT_BOTTOM]) { // bottom
            glBegin(GL_QUADS);
            glVertex2f(centerX - (segmentWidth / HALF_DIVISOR), centerY - (segmentWidth / HALF_DIVISOR) + segmentThickness);
            glVertex2f(centerX + (segmentWidth / HALF_DIVISOR), centerY - (segmentWidth / HALF_DIVISOR) + segmentThickness);
            glVertex2f(centerX + (segmentWidth / HALF_DIVISOR), centerY - (segmentWidth / HALF_DIVISOR));
            glVertex2f(centerX - (segmentWidth / HALF_DIVISOR), centerY - (segmentWidth / HALF_DIVISOR));
            glEnd();
        }
        
        // Draw vertical segments
        if (segments[number][1]) { // top-right
            glBegin(GL_QUADS);
            glVertex2f(centerX + (segmentWidth / HALF_DIVISOR), centerY + (segmentWidth / HALF_DIVISOR));
            glVertex2f(centerX + (segmentWidth / HALF_DIVISOR) - segmentThickness, centerY + (segmentWidth / HALF_DIVISOR));
            glVertex2f(centerX + (segmentWidth / HALF_DIVISOR) - segmentThickness, centerY);
            glVertex2f(centerX + (segmentWidth / HALF_DIVISOR), centerY);
            glEnd();
        }
        if (segments[number][2]) { // bottom-right
            glBegin(GL_QUADS);
            glVertex2f(centerX + (segmentWidth / HALF_DIVISOR), centerY);
            glVertex2f(centerX + (segmentWidth / HALF_DIVISOR) - segmentThickness, centerY);
            glVertex2f(centerX + (segmentWidth / HALF_DIVISOR) - segmentThickness, centerY - (segmentWidth / HALF_DIVISOR));
            glVertex2f(centerX + (segmentWidth / HALF_DIVISOR), centerY - (segmentWidth / HALF_DIVISOR));
            glEnd();
        }
        if (segments[number][SEGMENT_TOP_LEFT]) { // top-left
            glBegin(GL_QUADS);
            glVertex2f(centerX - (segmentWidth / HALF_DIVISOR) + segmentThickness, centerY + (segmentWidth / HALF_DIVISOR));
            glVertex2f(centerX - (segmentWidth / HALF_DIVISOR), centerY + (segmentWidth / HALF_DIVISOR));
            glVertex2f(centerX - (segmentWidth / HALF_DIVISOR), centerY);
            glVertex2f(centerX - (segmentWidth / HALF_DIVISOR) + segmentThickness, centerY);
            glEnd();
        }
        if (segments[number][4]) { // bottom-left
            glBegin(GL_QUADS);
            glVertex2f(centerX - (segmentWidth / HALF_DIVISOR) + segmentThickness, centerY);
            glVertex2f(centerX - (segmentWidth / HALF_DIVISOR), centerY);
            glVertex2f(centerX - (segmentWidth / HALF_DIVISOR), centerY - (segmentWidth / HALF_DIVISOR));
            glVertex2f(centerX - (segmentWidth / HALF_DIVISOR) + segmentThickness, centerY - (segmentWidth / HALF_DIVISOR));
            glEnd();
        }
    }
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
