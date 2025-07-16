#include <SDL2/SDL.h>
#include <GL/gl.h>
#include <SDL_log.h>
#include <vector>
#include <algorithm>
#include <ranges>
#include <cmath>
#include <cstdlib>
#include "entities.h"
#include "render.h"
#include "audio.h"


// Helper to convert screen to world coordinates

int main(int argc, char* argv[]) {
    // --- Per-planet build queue and timers ---
    std::unordered_map<EntityID, int> planetBuildQueue;
    std::unordered_map<EntityID, float> planetBuildTimer;
    std::unordered_map<EntityID, bool> planetBuildPending;
    // --- UI state ---
    EntityID selectedPlanet = 0;
    bool spawnShipPending = false;
    float spawnShipTimer = 0.0f;

    // --- UI constants ---
    constexpr int GRID_ROWS = 4;
    constexpr int GRID_COLS = 4;
    constexpr int GRID_CELL_SIZE = 48; // px
    constexpr int GRID_PADDING = 8; // px
    constexpr int SHIP_ICON_ROW = 0; // top row
    constexpr int SHIP_ICON_COL = 0; // left column
    
    // --- Unit selection panel constants ---
    constexpr int UNIT_PANEL_WIDTH = 400; // px
    constexpr int UNIT_PANEL_HEIGHT = 80; // px
    constexpr int UNIT_CELL_SIZE = 60; // px
    constexpr int UNIT_PANEL_PADDING = 8; // px
    
    // --- UI state ---
    std::vector<EntityID> selectedUnits; // All currently selected units
    
    // --- Enemy spawning system ---
    float enemySpawnTimer = 10.0F; // First enemy spawns after 10 seconds
    float enemySpawnInterval = 10.0F; // Initial spawn interval
    constexpr float ENEMY_SPAWN_INTERVAL_DECREASE = 0.9F; // Multiply interval by this each spawn (exponential)
    constexpr float MIN_ENEMY_SPAWN_INTERVAL = 2.0F; // Minimum time between spawns
    int enemyWaveCount = 0;
    
    // --- Survival timer ---
    float survivalTime = 0.0F; // Total time survived in seconds
    fillBeepBuffer();
    SDL_AudioSpec want{}, have{};
    want.freq = 48000; // AUDIO_RATE from audio.cpp
    want.format = AUDIO_F32SYS;
    want.channels = 1;
    want.samples = 512;
    // Use BeepState from audio.cpp
    BeepState beepState;
    want.callback = audioCallback;
    want.userdata = &beepState;
    if (SDL_OpenAudio(&want, &have) < 0) {
        SDL_Log("Failed to open audio: %s", SDL_GetError());
    } else {
        SDL_PauseAudio(0);
    }
    constexpr float WORLD_X_SCALE = 2.0F;
    constexpr float WORLD_X_OFFSET = 1.0F;
    constexpr float WORLD_Y_SCALE = 2.0F;
    constexpr float WORLD_Y_OFFSET = 1.0F;
    constexpr float WORLD_ASPECT_RATIO = 0.75F;

    auto screenToWorld = [](int mouseX, int mouseY, int windowWidth, int windowHeight) {
        float worldPosX = ((static_cast<float>(mouseX) / static_cast<float>(windowWidth)) * WORLD_X_SCALE) - WORLD_X_OFFSET;
        float worldPosY = WORLD_Y_OFFSET - ((static_cast<float>(mouseY) / static_cast<float>(windowHeight)) * WORLD_Y_SCALE);
        worldPosX *= 1.0F;
        worldPosY *= WORLD_ASPECT_RATIO;
        return std::make_pair(worldPosX, worldPosY);
    };

    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window* window = SDL_CreateWindow("Space RTS", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1600, 1200, SDL_WINDOW_OPENGL);
    SDL_GL_CreateContext(window);
    int win_w = 1600;
    int win_h = 1200;

    ECSRegistry ecs;
    // Create planets
    EntityID planet1 = ecs.createEntity();
    constexpr float PLANET1_POS_X = -0.5F;
    constexpr float PLANET1_POS_Y = 0.0F;
    constexpr float PLANET1_RADIUS = 0.15F;
    constexpr float PLANET2_POS_X = 0.5F;
    constexpr float PLANET2_POS_Y = 0.3F;
    constexpr float PLANET2_RADIUS = 0.10F;

    ecs.addComponent<Position>(planet1, Position{ .x = PLANET1_POS_X, .y = PLANET1_POS_Y });
    ecs.addComponent<PlanetTag>(planet1, PlanetTag{ .radius = PLANET1_RADIUS });
    planetBuildQueue[planet1] = 0;
    EntityID planet2 = ecs.createEntity();
    ecs.addComponent<Position>(planet2, Position{ .x = PLANET2_POS_X, .y = PLANET2_POS_Y });
    ecs.addComponent<PlanetTag>(planet2, PlanetTag{ .radius = PLANET2_RADIUS });
    planetBuildQueue[planet2] = 0;

    // Constants for initial positions, angles, and health
    constexpr float PLAYER_SHIP1_INIT_X = 0.0F;
    constexpr float PLAYER_SHIP1_INIT_Y = -0.4F;
    constexpr float PLAYER_SHIP2_INIT_X = 0.2F;
    constexpr float PLAYER_SHIP2_INIT_Y = 0.2F;
    constexpr float PLAYER_SHIP1_ANGLE = 0.0F;
    constexpr float PLAYER_SHIP2_ANGLE = 45.0F;
    constexpr float ENEMY_INIT_X = -0.3F;
    constexpr float ENEMY_INIT_Y = 0.3F;

    constexpr float ENEMY_ANGLE = 0.0F;
    constexpr int SHIP_HEALTH = 10;

    // Create player ship 1
    EntityID playerShip1 = ecs.createEntity();
    ecs.addComponent<Position>(playerShip1, Position{ .x = PLAYER_SHIP1_INIT_X, .y = PLAYER_SHIP1_INIT_Y });
    ecs.addComponent<SpacecraftTag>(playerShip1, SpacecraftTag{ .type = SpacecraftType::Player, .angle = PLAYER_SHIP1_ANGLE });
    ecs.addComponent<Health>(playerShip1, Health{ .hp = SHIP_HEALTH, .alive = true });

    // Create player ship 2
    EntityID playerShip2 = ecs.createEntity();
    ecs.addComponent<Position>(playerShip2, Position{ .x = PLAYER_SHIP2_INIT_X, .y = PLAYER_SHIP2_INIT_Y });
    ecs.addComponent<SpacecraftTag>(playerShip2, SpacecraftTag{ .type = SpacecraftType::Player, .angle = PLAYER_SHIP2_ANGLE });
    ecs.addComponent<Health>(playerShip2, Health{ .hp = SHIP_HEALTH, .alive = true });

    // Create enemy ship
    EntityID enemyShip = ecs.createEntity();
    ecs.addComponent<Position>(enemyShip, Position{ .x = ENEMY_INIT_X, .y = ENEMY_INIT_Y });
    ecs.addComponent<SpacecraftTag>(enemyShip, SpacecraftTag{ .type = SpacecraftType::Enemy, .angle = ENEMY_ANGLE });
    ecs.addComponent<Health>(enemyShip, Health{ .hp = SHIP_HEALTH, .alive = true });

    // Projectiles will be created dynamically
    Uint32 last_time = SDL_GetTicks();

    // Selection box
    bool dragging = false;
    int drag_start_x = 0;
    int drag_start_y = 0;
    int drag_end_x = 0;
    int drag_end_y = 0;

    bool running = true;
    int enemyIdx = 2; // index of enemy ship in ships vector
    // Enemy AI persistent target
    int enemyTargetIdx = -1;
    constexpr float MS_PER_SECOND = 1000.0F;
    SDL_Log("Starting main loop");
    while (running) {
        // --- UI grid position ---
        int gridOriginX = win_w - (GRID_COLS * GRID_CELL_SIZE) - GRID_PADDING;
        int gridOriginY = win_h - (GRID_ROWS * GRID_CELL_SIZE) - GRID_PADDING;
        
        // --- Unit selection panel position ---
        int unitPanelX = (win_w - UNIT_PANEL_WIDTH) / 2; // Center horizontally
        int unitPanelY = win_h - UNIT_PANEL_HEIGHT - UNIT_PANEL_PADDING; // Bottom of screen
        SDL_Event event;
        while (SDL_PollEvent(&event) == 1) {
            // --- Planet selection ---
            if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
                int mouseX = event.button.x;
                int mouseY = event.button.y;
                bool handledClick = false;
                
                // Check if clicked on UI grid ship icon
                int shipIconX = gridOriginX + (SHIP_ICON_COL * GRID_CELL_SIZE);
                int shipIconY = gridOriginY + (SHIP_ICON_ROW * GRID_CELL_SIZE);
                if (isPointInRect(mouseX, mouseY, shipIconX, shipIconY, GRID_CELL_SIZE, GRID_CELL_SIZE)) {
                    if (selectedPlanet != 0) {
                        planetBuildQueue[selectedPlanet]++;
                        SDL_Log("Added ship to build queue for planet %d. Queue size: %d", selectedPlanet, planetBuildQueue[selectedPlanet]);
                    }
                    handledClick = true;
                } else if (isPointInRect(mouseX, mouseY, unitPanelX, unitPanelY, UNIT_PANEL_WIDTH, UNIT_PANEL_HEIGHT)) {
                    // Check if clicked on a unit in the selection panel
                    if (!selectedUnits.empty()) {
                        // Calculate which unit cell was clicked
                        int relativeX = mouseX - unitPanelX;
                        int relativeY = mouseY - unitPanelY;
                        int clickedCol = relativeX / UNIT_CELL_SIZE;
                        int clickedRow = relativeY / UNIT_CELL_SIZE;
                        int clickedIndex = (clickedRow * (UNIT_PANEL_WIDTH / UNIT_CELL_SIZE)) + clickedCol;
                        
                        if (clickedIndex >= 0 && clickedIndex < static_cast<int>(selectedUnits.size())) {
                            // Individual selection: clear all and select only the clicked unit
                            EntityID clickedUnit = selectedUnits[static_cast<size_t>(clickedIndex)];
                            selectedUnits.clear();
                            selectedUnits.push_back(clickedUnit);
                            SDL_Log("Individually selected unit %d", clickedUnit);
                            SDL_Log("Total selected units: 1");
                        }
                    }
                    handledClick = true;
                } else {
                    // Check if we clicked on an individual ship first
                    auto [mouseWorldX, mouseWorldY] = screenToWorld(mouseX, mouseY, win_w, win_h);
                    EntityID clickedShip = 0;
                    constexpr float SHIP_CLICK_RADIUS = 0.03F; // Distance within which ship can be clicked
                    
                    for (auto& [shipId, tag] : ecs.getComponents<SpacecraftTag>()) {
                        if (tag.type != SpacecraftType::Player) continue; // Only allow selecting player ships
                        
                        Position* ppos = ecs.getComponent<Position>(shipId);
                        if (!ppos) continue;
                        float dx = mouseWorldX - ppos->x;
                        float dy = mouseWorldY - ppos->y;
                        float dist = std::sqrt(dx*dx + dy*dy);
                        if (dist <= SHIP_CLICK_RADIUS) {
                            clickedShip = shipId;
                            break; // Select the first ship we find at this location
                        }
                    }
                    
                    if (clickedShip != 0) {
                        // Individual ship selection
                        bool isCtrlHeld = (SDL_GetModState() & KMOD_CTRL) != 0;
                        
                        if (isCtrlHeld) {
                            // Add to selection or remove if already selected
                            auto it = std::find(selectedUnits.begin(), selectedUnits.end(), clickedShip);
                            if (it != selectedUnits.end()) {
                                selectedUnits.erase(it); // Deselect if already selected
                            } else {
                                selectedUnits.push_back(clickedShip); // Add to selection
                            }
                        } else {
                            // Replace selection with just this ship
                            selectedUnits.clear();
                            selectedUnits.push_back(clickedShip);
                        }
                        
                        handledClick = true;
                        SDL_Log("Individual ship selection: %zu units selected", selectedUnits.size());
                    } else {
                        // No ship clicked - check planet selection
                        for (auto& [pid, ptag] : ecs.getComponents<PlanetTag>()) {
                            Position* ppos = ecs.getComponent<Position>(pid);
                            if (!ppos) continue;
                            float dx = screenToWorld(mouseX, mouseY, win_w, win_h).first - ppos->x;
                            float dy = screenToWorld(mouseX, mouseY, win_w, win_h).second - ppos->y;
                            float dist = std::sqrt(dx*dx + dy*dy);
                            if (dist < ptag.radius) {
                                selectedPlanet = pid;
                                SDL_Log("Planet %d selected at (%.2f, %.2f)", pid, ppos->x, ppos->y);
                                handledClick = true;
                                break;
                            }
                        }
                    }
                }
                
                // Only start dragging if click wasn't handled by UI elements
                if (!handledClick) {
                    dragging = true;
                    drag_start_x = drag_end_x = event.button.x;
                    drag_start_y = drag_end_y = event.button.y;
                }
            }
        // --- Ship spawn timer ---
        // Move this after deltaTime is calculated
            if (event.type == SDL_QUIT) {
                running = false;
            }
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                win_w = event.window.data1;
                win_h = event.window.data2;
            }
            if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_LEFT) {
                if (dragging) {
                    dragging = false;
                    // Only do box selection if we actually dragged (moved more than a few pixels)
                    int dragDistance = std::abs(drag_end_x - drag_start_x) + std::abs(drag_end_y - drag_start_y);
                    if (dragDistance > 5) { // Only do box selection if dragged more than 5 pixels
                        // Compute selection rectangle in world coords
                        auto [x_start, y_start] = screenToWorld(drag_start_x, drag_start_y, win_w, win_h);
                        auto [x1, y1] = screenToWorld(drag_end_x, drag_end_y, win_w, win_h);
                        float minx = std::min(x_start, x1);
                        float maxx = std::max(x_start, x1);
                        float miny = std::min(y_start, y1);
                        float maxy = std::max(y_start, y1);
                        
                        // Clear previous selection and populate with units in the box
                        selectedUnits.clear();
                        for (auto& [id, tag] : ecs.getComponents<SpacecraftTag>()) {
                            if (tag.type == SpacecraftType::Player) {
                                Position* pos = ecs.getComponent<Position>(id);
                                if (pos) {
                                    if (pos->x >= minx && pos->x <= maxx && pos->y >= miny && pos->y <= maxy) {
                                        Health* health = ecs.getComponent<Health>(id);
                                        if (health && health->alive) {
                                            selectedUnits.push_back(id);
                                        }
                                    }
                                }
                            }
                        }
                        SDL_Log("Box selection completed - %zu units selected", selectedUnits.size());
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
                // Find enemy entity
                EntityID enemyEntity = 0;
                for (auto& [id, tag] : ecs.getComponents<SpacecraftTag>()) {
                    Health* health = ecs.getComponent<Health>(id);
                    if (tag.type == SpacecraftType::Enemy && health && health->alive) {
                        Position* pos = ecs.getComponent<Position>(id);
                        if (pos) {
                            float dist = std::sqrt(((tx - pos->x) * (tx - pos->x)) + ((ty - pos->y) * (ty - pos->y)));
                            if (dist < ENEMY_CLICK_RADIUS) {
                                enemyClicked = true;
                                enemyEntity = id;
                                break;
                            }
                        }
                    }
                }
                Health* enemyHealth = ecs.getComponent<Health>(enemyEntity);
                if (enemyClicked && enemyHealth && enemyHealth->alive) {
                    // Move units to attack the enemy
                    Position* enemyPos = ecs.getComponent<Position>(enemyEntity);
                    if (enemyPos) {
                        constexpr float ATTACK_RANGE = 0.15F; // Same as AUTO_ATTACK_RANGE in spacecraft.cpp
                        constexpr float PREFERRED_ATTACK_DISTANCE = 0.12F; // Stay slightly within range
                        
                        for (EntityID unitId : selectedUnits) {
                            SpacecraftTag* tag = ecs.getComponent<SpacecraftTag>(unitId);
                            Position* unitPos = ecs.getComponent<Position>(unitId);
                            if (tag && tag->type == SpacecraftType::Player && unitPos) {
                                // Calculate direction from enemy to unit
                                float dx = unitPos->x - enemyPos->x;
                                float dy = unitPos->y - enemyPos->y;
                                float currentDist = std::sqrt(dx*dx + dy*dy);
                                
                                // If unit is too close or too far, position it at preferred attack distance
                                if (currentDist < 0.01F) {
                                    // Units are on top of each other, pick a random direction
                                    float randomAngle = static_cast<float>(rand()) / static_cast<float>(RAND_MAX) * 2.0F * 3.14159F;
                                    dx = std::cos(randomAngle);
                                    dy = std::sin(randomAngle);
                                } else {
                                    // Normalize direction vector
                                    dx /= currentDist;
                                    dy /= currentDist;
                                }
                                
                                // Add small random offset to prevent perfect stacking
                                float offsetAngle = 0.0F;
                                if (selectedUnits.size() > 1) {
                                    constexpr float MAX_ANGLE_OFFSET = 0.5F; // radians
                                    offsetAngle = (static_cast<float>(rand()) / static_cast<float>(RAND_MAX) - 0.5F) * MAX_ANGLE_OFFSET;
                                    float cosOffset = std::cos(offsetAngle);
                                    float sinOffset = std::sin(offsetAngle);
                                    float newDx = dx * cosOffset - dy * sinOffset;
                                    float newDy = dx * sinOffset + dy * cosOffset;
                                    dx = newDx;
                                    dy = newDy;
                                }
                                
                                // Position unit at preferred attack distance from enemy
                                float destX = enemyPos->x + dx * PREFERRED_ATTACK_DISTANCE;
                                float destY = enemyPos->y + dy * PREFERRED_ATTACK_DISTANCE;
                                
                                tag->dest_x = destX;
                                tag->dest_y = destY;
                                tag->moving = true;
                                tag->attacking = true; // Set both moving and attacking
                                SDL_Log("Unit %d moving to attack position near enemy at (%.2f, %.2f)", unitId, destX, destY);
                            }
                        }
                        if (!selectedUnits.empty()) {
                            ecs.audioEvents.push_back(AudioEventType::Beep);
                            SDL_Log("Attack command issued to %zu units", selectedUnits.size());
                        }
                    }
                } else {
                    // Move selected ships to clicked location
                    for (EntityID unitId : selectedUnits) {
                        SpacecraftTag* tag = ecs.getComponent<SpacecraftTag>(unitId);
                        if (tag && tag->type == SpacecraftType::Player) {
                            // Add small random offset to destination if multiple units selected
                            // This prevents perfect stacking at destination
                            float destX = tx;
                            float destY = ty;
                            if (selectedUnits.size() > 1) {
                                constexpr float DEST_OFFSET_RANGE = 0.04F; // Max offset from click point
                                float randomAngle = static_cast<float>(rand()) / static_cast<float>(RAND_MAX) * 2.0F * 3.14159F;
                                float randomRadius = (static_cast<float>(rand()) / static_cast<float>(RAND_MAX)) * DEST_OFFSET_RANGE;
                                destX += randomRadius * cosf(randomAngle);
                                destY += randomRadius * sinf(randomAngle);
                            }
                            
                            tag->dest_x = destX;
                            tag->dest_y = destY;
                            tag->moving = true;
                            tag->attacking = false;
                            SDL_Log("Moving unit %d to (%.2f, %.2f)", unitId, destX, destY);
                        }
                    }
                    if (!selectedUnits.empty()) {
                        ecs.audioEvents.push_back(AudioEventType::Beep);
                        SDL_Log("Move command issued to %zu units", selectedUnits.size());
                    } else {
                        SDL_Log("No units selected for movement");
                    }
                }
            }
        }

        Uint32 now = SDL_GetTicks();
        float deltaTime = (static_cast<float>(now) - static_cast<float>(last_time)) / MS_PER_SECOND;
        last_time = now;
        
        // Update survival timer
        survivalTime += deltaTime;
        // --- Ship spawn timer: process all planets with build queues ---
        for (auto& [pid, queue] : planetBuildQueue) {
            if (queue > 0) {
                // Start timer if not already pending
                if (!planetBuildPending[pid]) {
                    planetBuildPending[pid] = true;
                    planetBuildTimer[pid] = 5.0F;
                }
                planetBuildTimer[pid] -= deltaTime;
                if (planetBuildTimer[pid] <= 0.0F) {
                    planetBuildTimer[pid] = 5.0F;
                    Position* planetPos = ecs.getComponent<Position>(pid);
                    if (planetPos) {
                        EntityID newShip = ecs.createEntity();
                        
                        // Add small random offset to prevent ships from spawning in exact same position
                        constexpr float SPAWN_OFFSET_RANGE = 0.08F; // Max offset from planet center
                        float randomAngle = static_cast<float>(rand()) / static_cast<float>(RAND_MAX) * 2.0F * 3.14159F;
                        float randomRadius = (static_cast<float>(rand()) / static_cast<float>(RAND_MAX)) * SPAWN_OFFSET_RANGE;
                        float spawnX = planetPos->x + randomRadius * cosf(randomAngle);
                        float spawnY = planetPos->y + randomRadius * sinf(randomAngle);
                        
                        ecs.addComponent<Position>(newShip, Position{spawnX, spawnY});
                        ecs.addComponent<SpacecraftTag>(newShip, SpacecraftTag{SpacecraftType::Player});
                        ecs.addComponent<Health>(newShip, Health{});
                        SDL_Log("Spawned new ship at planet %d (x=%.2f, y=%.2f)", pid, spawnX, spawnY);
                    }
                    --planetBuildQueue[pid];
                    if (planetBuildQueue[pid] == 0) {
                        planetBuildPending[pid] = false;
                        planetBuildTimer[pid] = 0.0F;
                    }
                }
            } else {
                planetBuildPending[pid] = false;
                planetBuildTimer[pid] = 0.0F;
            }
        }

        // Enemy spawning system
        enemySpawnTimer -= deltaTime;
        if (enemySpawnTimer <= 0.0F) {
            // Spawn enemy(ies) - exponentially increasing count
            int enemiesToSpawn = 1 + (enemyWaveCount / 3); // 1, 1, 1, 2, 2, 2, 3, etc.
            
            for (int i = 0; i < enemiesToSpawn; ++i) {
                EntityID enemy = ecs.createEntity();
                
                // Spawn enemies around the edge of the map
                float angle = static_cast<float>(rand()) / RAND_MAX * 2.0F * 3.14159F;
                float spawnX = 0.5F + 0.8F * std::cos(angle); // Spawn on outer ring
                float spawnY = 0.5F + 0.8F * std::sin(angle);
                
                ecs.addComponent<Position>(enemy, Position{spawnX, spawnY});
                ecs.addComponent<SpacecraftTag>(enemy, SpacecraftTag{SpacecraftType::Enemy});
                ecs.addComponent<Health>(enemy, Health{});
            }
            
            // Update spawning parameters for next wave
            enemyWaveCount++;
            enemySpawnInterval = std::max(MIN_ENEMY_SPAWN_INTERVAL, enemySpawnInterval * ENEMY_SPAWN_INTERVAL_DECREASE);
            enemySpawnTimer = enemySpawnInterval;
            
            SDL_Log("Spawned wave %d: %d enemies (next spawn in %.1fs)", enemyWaveCount, enemiesToSpawn, enemySpawnInterval);
        }

        ecs.updateSystems(deltaTime);
        // Process audio events
        // Trigger overlapping sounds
        for (AudioEventType evt : ecs.audioEvents) {
            switch (evt) {
                case AudioEventType::Boom:
                    beepState.boomPositions.push_back(0);
                    break;
                case AudioEventType::Pew:
                    beepState.pewPositions.push_back(0);
                    break;
                case AudioEventType::Beep:
                    beepState.beepPositions.push_back(0);
                    break;
            }
        }
        ecs.audioEvents.clear();

        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(-1, 1, -0.75, 0.75, -1, 1);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glColor3f(0.2f, 0.6f, 1.0f);
        for (auto& [id, ptag] : ecs.getComponents<PlanetTag>()) {
            Position* pos = ecs.getComponent<Position>(id);
            if (pos) {
                drawCircle(pos->x, pos->y, ptag.radius);
                // Highlight selected planet
                if (id == selectedPlanet) {
                    glColor3f(0.8F, 0.8F, 0.2F);
                    glLineWidth(4.0F);
                    glBegin(GL_LINE_LOOP);
                    for (int i = 0; i < 32; ++i) {
                        float theta = 2.0F * 3.1415926F * float(i) / 32.0F;
                        glVertex2f(pos->x + (ptag.radius + 0.02F) * cosf(theta), pos->y + (ptag.radius + 0.02F) * sinf(theta));
                    }
                    glEnd();
                }
            }
        }
        for (auto& [id, tag] : ecs.getComponents<SpacecraftTag>()) {
            Health* health = ecs.getComponent<Health>(id);
            if (!health || !health->alive) {
                continue;
            }
            Position* pos = ecs.getComponent<Position>(id);
            if (!pos) continue;
            if (tag.type == SpacecraftType::Enemy) {
                glColor3f(1.0f, 0.2f, 0.2f); // Enemy: red
                drawTriangle(pos->x, pos->y, tag.angle);
                // Draw health bar above enemy
                float barWidth = 0.12f;
                float barHeight = 0.015f;
                float hpFrac = float(health->hp) / 10.0f;
                float barX = pos->x - barWidth/2.0f;
                float barY = pos->y + 0.06f;
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
            if (tag.type == SpacecraftType::Player) {
                // Draw health bar above player unit
                float barWidth = 0.08f;
                float barHeight = 0.012f;
                float hpFrac = float(health->hp) / 10.0f;
                float barX = pos->x - barWidth/2.0f;
                float barY = pos->y + 0.045f;
                // Background (gray)
                glColor3f(0.3f, 0.3f, 0.3f);
                glBegin(GL_QUADS);
                glVertex2f(barX, barY);
                glVertex2f(barX + barWidth, barY);
                glVertex2f(barX + barWidth, barY + barHeight);
                glVertex2f(barX, barY + barHeight);
                glEnd();
                // Foreground (green)
                glColor3f(0.2f, 1.0F, 0.2F);
                glBegin(GL_QUADS);
                glVertex2f(barX, barY);
                glVertex2f(barX + barWidth * hpFrac, barY);
                glVertex2f(barX + barWidth * hpFrac, barY + barHeight);
                glVertex2f(barX, barY + barHeight);
                glEnd();
            }
            // Check if this unit is selected
            bool isSelected = std::find(selectedUnits.begin(), selectedUnits.end(), id) != selectedUnits.end();
            if (isSelected) {
                glColor3f(0.0f, 1.0f, 0.0f); // Highlight selected
                drawTriangle(pos->x, pos->y, tag.angle);
                // Draw selection circle
                glColor3f(0.0f, 1.0f, 0.0f);
                glLineWidth(2.0f);
                glBegin(GL_LINE_LOOP);
                for (int i = 0; i < 32; ++i) {
                    float theta = 2.0f * 3.1415926f * float(i) / 32.0f;
                    glVertex2f(pos->x + 0.04f * cosf(theta), pos->y + 0.04f * sinf(theta));
                }
                glEnd();
            } else {
                glColor3f(1.0f, 0.8f, 0.2f); // Player: yellow
                drawTriangle(pos->x, pos->y, tag.angle);
            }
        }

        // Draw projectiles
        glColor3f(1.0f, 1.0f, 1.0f);
        for (auto& [pid, ptag] : ecs.getComponents<ProjectileTag>()) {
            if (!ptag.active) {
                continue;
            }
            Position* pos = ecs.getComponent<Position>(pid);
            if (pos) {
                drawCircle(pos->x, pos->y, 0.012f);
            }
        }

        if (dragging) {
            auto [x_start, y_start] = screenToWorld(drag_start_x, drag_start_y, win_w, win_h);
            auto [x1, y1] = screenToWorld(drag_end_x, drag_end_y, win_w, win_h);
            glColor3f(0.0f, 1.0f, 0.0f);
            glBegin(GL_LINE_LOOP);
            glVertex2f(x_start, y_start);
            glVertex2f(x1, y_start);
            glVertex2f(x1, y1);
            glVertex2f(x_start, y1);
            glEnd();
        }
        
        // Draw UI Grid
        drawUIGrid(gridOriginX, gridOriginY, GRID_CELL_SIZE, GRID_ROWS, GRID_COLS);
        
        // Draw ship icon in top-left cell
        int shipIconX = gridOriginX + (SHIP_ICON_COL * GRID_CELL_SIZE);
        int shipIconY = gridOriginY + (SHIP_ICON_ROW * GRID_CELL_SIZE);
        drawShipIcon(shipIconX, shipIconY, GRID_CELL_SIZE);
        
        // Draw build queue number if any ships are queued for selected planet
        if (selectedPlanet != 0 && planetBuildQueue[selectedPlanet] > 0) {
            constexpr int NUMBER_OFFSET = 8;
            constexpr int NUMBER_SIZE = 20; // Made bigger for better visibility
            int numberX = shipIconX + GRID_CELL_SIZE - NUMBER_OFFSET; // Bottom right of icon
            int numberY = shipIconY + GRID_CELL_SIZE - NUMBER_OFFSET;
            drawNumber(planetBuildQueue[selectedPlanet], numberX, numberY, NUMBER_SIZE);
        }
        
        // Draw Unit Selection Panel
        drawUnitSelectionPanel(unitPanelX, unitPanelY, UNIT_PANEL_WIDTH, UNIT_PANEL_HEIGHT, UNIT_CELL_SIZE);
        
        // Draw selected units in the panel
        int unitsPerRow = UNIT_PANEL_WIDTH / UNIT_CELL_SIZE;
        for (size_t i = 0; i < selectedUnits.size(); ++i) {
            int col = static_cast<int>(i) % unitsPerRow;
            int row = static_cast<int>(i) / unitsPerRow;
            int unitX = unitPanelX + (col * UNIT_CELL_SIZE);
            int unitY = unitPanelY + (row * UNIT_CELL_SIZE);
            
            EntityID unitId = selectedUnits[i];
            Health* health = ecs.getComponent<Health>(unitId);
            if (health) {
                constexpr float MAX_HEALTH = 10.0F;
                float healthPercent = static_cast<float>(health->hp) / MAX_HEALTH;
                // For now, highlight the first unit in individual selection mode
                bool isHighlighted = (selectedUnits.size() == 1);
                drawUnitInPanel(unitX, unitY, UNIT_CELL_SIZE, healthPercent, isHighlighted);
            }
        }
        
        // Draw survival timer in top right corner
        constexpr int TIMER_PADDING = 16;
        constexpr int TIMER_SIZE = 24;
        constexpr int TIMER_WIDTH = 150;
        int timerX = win_w - TIMER_WIDTH - TIMER_PADDING;
        int timerY = TIMER_PADDING;
        
        // Convert survival time to minutes and seconds
        int totalSeconds = static_cast<int>(survivalTime);
        int minutes = totalSeconds / 60;
        int seconds = totalSeconds % 60;
        
        // Draw timer background (dark semi-transparent)
        glColor4f(0.0F, 0.0F, 0.0F, 0.7F);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glBegin(GL_QUADS);
        glVertex2i(timerX - 10, timerY - 5);
        glVertex2i(win_w - 10, timerY - 5);
        glVertex2i(win_w - 10, timerY + TIMER_SIZE + 15);
        glVertex2i(timerX - 10, timerY + TIMER_SIZE + 15);
        glEnd();
        glDisable(GL_BLEND);
        
        // Draw the timer using drawNumber function
        // Format: MM:SS using individual numbers
        glColor3f(1.0F, 1.0F, 0.0F); // Yellow text
        
        // Draw minutes (two digits)
        drawNumber(minutes / 10, timerX, timerY, TIMER_SIZE); // Tens digit
        drawNumber(minutes % 10, timerX + TIMER_SIZE, timerY, TIMER_SIZE); // Ones digit
        
        // Draw colon (using small rectangles)
        glBegin(GL_QUADS);
        glVertex2i(timerX + TIMER_SIZE * 2 + 5, timerY + 8);
        glVertex2i(timerX + TIMER_SIZE * 2 + 10, timerY + 8);
        glVertex2i(timerX + TIMER_SIZE * 2 + 10, timerY + 12);
        glVertex2i(timerX + TIMER_SIZE * 2 + 5, timerY + 12);
        
        glVertex2i(timerX + TIMER_SIZE * 2 + 5, timerY + 16);
        glVertex2i(timerX + TIMER_SIZE * 2 + 10, timerY + 16);
        glVertex2i(timerX + TIMER_SIZE * 2 + 10, timerY + 20);
        glVertex2i(timerX + TIMER_SIZE * 2 + 5, timerY + 20);
        glEnd();
        
        // Draw seconds (two digits)
        drawNumber(seconds / 10, timerX + TIMER_SIZE * 2 + 20, timerY, TIMER_SIZE); // Tens digit
        drawNumber(seconds % 10, timerX + TIMER_SIZE * 3 + 20, timerY, TIMER_SIZE); // Ones digit
        
        SDL_GL_SwapWindow(window);
        constexpr Uint32 FRAME_DELAY_MS = 16;
        SDL_Delay(FRAME_DELAY_MS);
    }
    SDL_CloseAudio();
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

