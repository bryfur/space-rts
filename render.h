#pragma once
#include "entities.h"
#include <GL/gl.h>

void drawCircle(float cx, float cy, float r);
void drawTriangle(float x, float y, float angle);
void drawHealthBar(const Spacecraft& s);
void drawSelectionCircle(const Spacecraft& s);
