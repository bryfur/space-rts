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

void drawHealthBar(const Spacecraft& s) {
    float barWidth = (s.type == SpacecraftType::Enemy) ? 0.12f : 0.08f;
    float barHeight = (s.type == SpacecraftType::Enemy) ? 0.015f : 0.012f;
    float hpFrac = float(s.hp) / 10.0f;
    float barX = s.x - barWidth/2.0f;
    float barY = s.y + ((s.type == SpacecraftType::Enemy) ? 0.06f : 0.045f);
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

void drawSelectionCircle(const Spacecraft& s) {
    glColor3f(0.0f, 1.0f, 0.0f);
    glLineWidth(2.0f);
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < 32; ++i) {
        float theta = 2.0f * 3.1415926f * float(i) / 32.0f;
        glVertex2f(s.x + 0.04f * cosf(theta), s.y + 0.04f * sinf(theta));
    }
    glEnd();
}
