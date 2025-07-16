#pragma once
#include "entities.h"
#include <GL/gl.h>

void drawCircle(float cx, float cy, float r);
void drawTriangle(float x, float y, float angle);
void drawHealthBar(const Position& pos, const Health& health, const SpacecraftTag& tag);
void drawSelectionCircle(const Position& pos);

// UI rendering functions
void drawUIGrid(int gridOriginX, int gridOriginY, int cellSize, int rows, int cols);
void drawShipIcon(int posX, int posY, int size);
void drawNumber(int number, int posX, int posY, int size);
bool isPointInRect(int pointX, int pointY, int rectX, int rectY, int rectW, int rectH);

// Unit selection panel functions
void drawUnitSelectionPanel(int panelX, int panelY, int panelWidth, int panelHeight, int cellSize);
void drawUnitInPanel(int posX, int posY, int size, float healthPercent, bool isHighlighted);
