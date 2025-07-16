

#include <SDL2/SDL.h>
#include <GL/gl.h>
#include <vector>
#include <algorithm>
#include <cmath>
#include "entities.h"
#include "render.h"
#include "audio.h"


// Helper to convert screen to world coordinates

int main(int argc, char* argv[]) {
    fillBeepBuffer();
    SDL_AudioSpec want{}, have{};
    want.freq = 48000; // AUDIO_RATE from audio.cpp
    want.format = AUDIO_F32SYS;
    want.channels = 1;
    want.samples = 512;
    // Use BeepState from audio.cpp
    BeepState beepState = {48000 * 100 / 1000, 48000 * 60 / 1000, 48000 * 250 / 1000, SoundType::None};
    want.callback = audioCallback;
    want.userdata = &beepState;
    if (SDL_OpenAudio(&want, &have) < 0) {
        SDL_Log("Failed to open audio: %s", SDL_GetError());
    } else {
        SDL_PauseAudio(0);
    }
    auto screenToWorld = [](int mx, int my, int w, int h) {
        float x = (float(mx) / w) * 2.0f - 1.0f;
        float y = 1.0f - (float(my) / h) * 2.0f;
        x *= 1.0f;
        y *= 0.75f;
        return std::make_pair(x, y);
    };

    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window* window = SDL_CreateWindow("Space RTS", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 600, SDL_WINDOW_OPENGL);
    SDL_GL_CreateContext(window);
    int win_w = 800, win_h = 600;

    std::vector<Planet> planets = { { -0.5f, 0.0f, 0.15f }, { 0.5f, 0.3f, 0.10f } };


    std::vector<Spacecraft> ships = {
        { 0.0f, -0.4f, 0.0f, false, 0.0f, 0.0f, false, SpacecraftType::Player, 0.0f, false, 10, true },
        { 0.2f, 0.2f, 45.0f, false, 0.0f, 0.0f, false, SpacecraftType::Player, 0.0f, false, 10, true },
        { -0.3f, 0.3f, 0.0f, false, 0.0f, 0.0f, false, SpacecraftType::Enemy, 0.0f, false, 10, true }
    };

    std::vector<Projectile> projectiles;
    Uint32 last_time = SDL_GetTicks();

    // Selection box
    bool dragging = false;
    int drag_start_x = 0, drag_start_y = 0, drag_end_x = 0, drag_end_y = 0;

    bool running = true;
    int enemyIdx = 2; // index of enemy ship in ships vector
    // Enemy AI persistent target
    int enemyTargetIdx = -1;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = false;
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                win_w = event.window.data1;
                win_h = event.window.data2;
            }
            if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
                dragging = true;
                drag_start_x = drag_end_x = event.button.x;
                drag_start_y = drag_end_y = event.button.y;
            }
            if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_LEFT) {
                dragging = false;
                // Compute selection rectangle in world coords
                auto [x0, y0] = screenToWorld(drag_start_x, drag_start_y, win_w, win_h);
                auto [x1, y1] = screenToWorld(drag_end_x, drag_end_y, win_w, win_h);
                float minx = std::min(x0, x1), maxx = std::max(x0, x1);
                float miny = std::min(y0, y1), maxy = std::max(y0, y1);
                for (auto& s : ships) {
                    if (s.type == SpacecraftType::Player) {
                        s.selected = (s.x >= minx && s.x <= maxx && s.y >= miny && s.y <= maxy);
                    } else {
                        s.selected = false;
                    }
                }
            }
            if (event.type == SDL_MOUSEMOTION && dragging) {
                drag_end_x = event.motion.x;
                drag_end_y = event.motion.y;
            }
            if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_RIGHT) {
                // Check if right-clicked on enemy (world units)
                auto [tx, ty] = screenToWorld(event.button.x, event.button.y, win_w, win_h);
                bool enemyClicked = false;
                constexpr float ENEMY_CLICK_RADIUS = 0.06f; // world units (approx 50px)
                if (ships[enemyIdx].alive) {
                    for (size_t i = 0; i < ships.size(); ++i) {
                        if (ships[i].type == SpacecraftType::Enemy) {
                            float ex = ships[i].x, ey = ships[i].y;
                            float dist = std::sqrt((tx - ex)*(tx - ex) + (ty - ey)*(ty - ey));
                            if (dist < ENEMY_CLICK_RADIUS) {
                                enemyClicked = true;
                                break;
                            }
                        }
                    }
                }
                if (enemyClicked && ships[enemyIdx].alive) {
                    for (auto& s : ships) {
                        if (s.selected && s.type == SpacecraftType::Player) {
                            s.attacking = true;
                            s.moving = false;
                        }
                    }
                } else {
                    // Move selected ships to clicked location
                    bool anySelected = false;
                    for (auto& s : ships) {
                        if (s.selected && s.type == SpacecraftType::Player) {
                            s.dest_x = tx;
                            s.dest_y = ty;
                            s.moving = true;
                            s.attacking = false;
                            anySelected = true;
                        }
                    }
                    if (anySelected) beepRequested = true;
                }
            }
        }

        // Timing for attack cooldown and projectile movement
        Uint32 now = SDL_GetTicks();
        float dt = (now - last_time) / 1000.0f;
        last_time = now;

        // Move ships and handle attacks
        for (size_t i = 0; i < ships.size(); ++i) {
            auto& s = ships[i];
            if (!s.alive) continue;
            if (s.moving) {
                float dx = s.dest_x - s.x;
                float dy = s.dest_y - s.y;
                float dist = std::sqrt(dx*dx + dy*dy);
                if (dist > 0.005f) {
                    float speed = 0.01f;
                    s.x += speed * dx / dist;
                    s.y += speed * dy / dist;
                    s.angle = atan2f(dy, dx) * 180.0f / 3.1415926f - 90.0f;
                } else {
                    s.x = s.dest_x;
                    s.y = s.dest_y;
                    s.moving = false;
                }
            }
            // Player attack logic
            if (s.attacking && s.type == SpacecraftType::Player && ships[enemyIdx].alive) {
                auto& e = ships[enemyIdx];
                float dx = e.x - s.x;
                float dy = e.y - s.y;
                float dist = std::sqrt(dx*dx + dy*dy);
                constexpr float ATTACK_RANGE = 0.12f;
                if (dist < ATTACK_RANGE) {
                    s.moving = false;
                    s.x = s.x;
                    s.y = s.y;
                    s.dest_x = s.x;
                    s.dest_y = s.y;
                    s.angle = atan2f(dy, dx) * 180.0f / 3.1415926f - 90.0f;
                    s.attack_cooldown -= dt;
                    if (s.attack_cooldown <= 0.0f) {
                        float norm = std::sqrt(dx*dx + dy*dy);
                        float vx = dx / norm * 0.02f;
                        float vy = dy / norm * 0.02f;
                        projectiles.push_back({s.x, s.y, vx, vy, 2.0f, true, SpacecraftType::Player, enemyIdx});
                        pewRequested = true;
                        s.attack_cooldown = 1.5f;
                    }
                } else {
                    s.dest_x = e.x;
                    s.dest_y = e.y;
                    s.moving = true;
                }
            } else if (s.type == SpacecraftType::Enemy && s.alive) {
                // Enemy AI: lock onto a single player unit until destroyed, use dest_x/dest_y for smooth pursuit
                if (enemyTargetIdx == -1 || enemyTargetIdx >= (int)ships.size() || !ships[enemyTargetIdx].alive || ships[enemyTargetIdx].type != SpacecraftType::Player) {
                    float minDist = 1000.0f;
                    int bestIdx = -1;
                    for (size_t j = 0; j < ships.size(); ++j) {
                        if (ships[j].type == SpacecraftType::Player && ships[j].alive) {
                            float dx = ships[j].x - s.x;
                            float dy = ships[j].y - s.y;
                            float dist = std::sqrt(dx*dx + dy*dy);
                            if (dist < minDist) {
                                minDist = dist;
                                bestIdx = j;
                            }
                        }
                    }
                    enemyTargetIdx = bestIdx;
                }
                constexpr float ATTACK_RANGE = 0.12f;
                constexpr float ATTACK_STOP_RANGE = 0.10f;
                if (enemyTargetIdx != -1) {
                    float tx = ships[enemyTargetIdx].x;
                    float ty = ships[enemyTargetIdx].y;
                    float dx = tx - s.x;
                    float dy = ty - s.y;
                    float dist = std::sqrt(dx*dx + dy*dy);
                    s.angle = atan2f(dy, dx) * 180.0f / 3.1415926f - 90.0f;
                    // Only update dest if target moved significantly
                    if (std::abs(s.dest_x - tx) > 1e-4f || std::abs(s.dest_y - ty) > 1e-4f) {
                        s.dest_x = tx;
                        s.dest_y = ty;
                    }
                    if (dist > ATTACK_RANGE) {
                        s.moving = true;
                    } else if (dist < ATTACK_STOP_RANGE) {
                        s.moving = false;
                        s.x = s.x;
                        s.y = s.y;
                        s.dest_x = s.x;
                        s.dest_y = s.y;
                        s.attack_cooldown -= dt;
                        if (s.attack_cooldown <= 0.0f) {
                            float norm = std::sqrt(dx*dx + dy*dy);
                            float vx = dx / norm * 0.02f;
                            float vy = dy / norm * 0.02f;
                            projectiles.push_back({s.x, s.y, vx, vy, 2.0f, true, SpacecraftType::Enemy, enemyTargetIdx});
                            s.attack_cooldown = 1.5f;
                        }
                    } else {
                        s.moving = true;
                    }
                } else {
                    // No player units left, idle in place
                    s.moving = false;
                    s.attack_cooldown = std::max(0.0f, s.attack_cooldown - dt);
                }
            } else {
                s.attack_cooldown = std::max(0.0f, s.attack_cooldown - dt);
            }
        }

        // Move projectiles and handle hits
        for (auto& proj : projectiles) {
            if (!proj.active) continue;
            proj.x += proj.dx;
            proj.y += proj.dy;
            proj.life -= dt;
            // Check collision with target
            if (proj.targetIdx >= 0 && proj.targetIdx < (int)ships.size() && ships[proj.targetIdx].alive) {
                float dx = ships[proj.targetIdx].x - proj.x;
                float dy = ships[proj.targetIdx].y - proj.y;
                float dist = std::sqrt(dx*dx + dy*dy);
                if (dist < 0.04f) {
                    proj.active = false;
                    ships[proj.targetIdx].hp--;
                    if (ships[proj.targetIdx].hp <= 0) {
                        ships[proj.targetIdx].alive = false;
                        boomRequested = true;
                    }
                }
            }
            if (proj.life <= 0.0f) proj.active = false;
        }
        // Remove inactive projectiles
        projectiles.erase(std::remove_if(projectiles.begin(), projectiles.end(), [](const Projectile& p){ return !p.active; }), projectiles.end());

        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(-1, 1, -0.75, 0.75, -1, 1);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glColor3f(0.2f, 0.6f, 1.0f);
        for (const auto& p : planets) drawCircle(p.x, p.y, p.radius);
        for (const auto& s : ships) {
            if (!s.alive) continue;
            if (s.type == SpacecraftType::Enemy) {
                glColor3f(1.0f, 0.2f, 0.2f); // Enemy: red
                drawTriangle(s.x, s.y, s.angle);
                // Draw health bar above enemy
                float barWidth = 0.12f;
                float barHeight = 0.015f;
                float hpFrac = float(s.hp) / 10.0f;
                float barX = s.x - barWidth/2.0f;
                float barY = s.y + 0.06f;
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
                continue;
            }
            if (s.type == SpacecraftType::Player) {
                // Draw health bar above player unit
                float barWidth = 0.08f;
                float barHeight = 0.012f;
                float hpFrac = float(s.hp) / 10.0f;
                float barX = s.x - barWidth/2.0f;
                float barY = s.y + 0.045f;
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
            if (s.selected) {
                glColor3f(0.0f, 1.0f, 0.0f); // Highlight selected
                drawTriangle(s.x, s.y, s.angle);
                // Draw selection circle
                glColor3f(0.0f, 1.0f, 0.0f);
                glLineWidth(2.0f);
                glBegin(GL_LINE_LOOP);
                for (int i = 0; i < 32; ++i) {
                    float theta = 2.0f * 3.1415926f * float(i) / 32.0f;
                    glVertex2f(s.x + 0.04f * cosf(theta), s.y + 0.04f * sinf(theta));
                }
                glEnd();
            } else {
                glColor3f(1.0f, 0.8f, 0.2f); // Player: yellow
                drawTriangle(s.x, s.y, s.angle);
            }
        }

        // Draw projectiles
        glColor3f(1.0f, 1.0f, 1.0f);
        for (const auto& proj : projectiles) {
            if (!proj.active) continue;
            drawCircle(proj.x, proj.y, 0.012f);
        }
        // Draw selection rectangle
        if (dragging) {
            auto [x0, y0] = screenToWorld(drag_start_x, drag_start_y, win_w, win_h);
            auto [x1, y1] = screenToWorld(drag_end_x, drag_end_y, win_w, win_h);
            glColor3f(0.0f, 1.0f, 0.0f);
            glBegin(GL_LINE_LOOP);
            glVertex2f(x0, y0);
            glVertex2f(x1, y0);
            glVertex2f(x1, y1);
            glVertex2f(x0, y1);
            glEnd();
        }
        SDL_GL_SwapWindow(window);
        SDL_Delay(16);
    }
    SDL_CloseAudio();
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

