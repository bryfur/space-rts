#pragma once
#include "entities.h"
#include <GL/gl.h>
#include <string>

// Initialize and cleanup text rendering system
bool initTextRendering();
void cleanupTextRendering();

// Text rendering functions
void renderText(const std::string& text, float posX, float posY, float size, float red = 1.0F, float green = 1.0F, float blue = 1.0F);
void renderTextCentered(const std::string& text, float posX, float posY, float size, float red = 1.0F, float green = 1.0F, float blue = 1.0F);
void renderTextUI(const std::string& text, int screenX, int screenY, int size, float red = 1.0F, float green = 1.0F, float blue = 1.0F);

// Convenient color preset functions for common text rendering
void renderTextUIWhite(const std::string& text, int screenX, int screenY, int size);
void renderTextUIGreen(const std::string& text, int screenX, int screenY, int size);
void renderTextUIRed(const std::string& text, int screenX, int screenY, int size);
void renderTextUIYellow(const std::string& text, int screenX, int screenY, int size);

void drawCircle(float cx, float cy, float r);
void drawTriangle(float x, float y, float angle);
void drawHealthBar(const Position& pos, const Health& health, const SpacecraftTag& tag);
void drawSelectionCircle(const Position& pos);

// UI rendering functions
void drawUIGrid(int gridOriginX, int gridOriginY, int cellSize, int rows, int cols);
void drawShipIcon(int posX, int posY, int size);
bool isPointInRect(int pointX, int pointY, int rectX, int rectY, int rectW, int rectH);

// Unit selection panel functions
void drawUnitSelectionPanel(int panelX, int panelY, int panelWidth, int panelHeight, int cellSize);
void drawUnitInPanel(int posX, int posY, int size, float healthPercent, bool isHighlighted);
