#include "spacecraft.h"
#include <cmath>
#include <ranges>

void updateSpacecrafts(ECSRegistry& ecs, float dt) {
    auto& spacecraftMap = ecs.getComponents<SpacecraftTag>();
    
    // Apply separation forces to prevent ships from stacking
    for (auto& [id, tag] : spacecraftMap) {
        Health* health = ecs.getComponent<Health>(id);
        if (!health || !health->alive) continue;
        Position* pos = ecs.getComponent<Position>(id);
        if (!pos) continue;
        
        // Calculate separation force from nearby ships
        float separationX = 0.0f;
        float separationY = 0.0f;
        int nearbyCount = 0;
        
        constexpr float SEPARATION_RADIUS = 0.05f; // How close ships can get before pushing apart
        constexpr float SEPARATION_STRENGTH = 0.8f; // How strong the push force is
        
        for (auto& [otherId, otherTag] : spacecraftMap) {
            if (id == otherId) continue; // Don't separate from self
            Health* otherHealth = ecs.getComponent<Health>(otherId);
            if (!otherHealth || !otherHealth->alive) continue;
            Position* otherPos = ecs.getComponent<Position>(otherId);
            if (!otherPos) continue;
            
            // Only apply separation between ships of the same type to avoid weird enemy behavior
            if (tag.type != otherTag.type) continue;
            
            float dx = pos->x - otherPos->x;
            float dy = pos->y - otherPos->y;
            float dist = std::sqrt(dx*dx + dy*dy);
            
            if (dist > 0.001f && dist < SEPARATION_RADIUS) { // Avoid division by zero
                // Normalize and scale by inverse distance (closer = stronger push)
                float force = SEPARATION_STRENGTH * (SEPARATION_RADIUS - dist) / SEPARATION_RADIUS;
                separationX += (dx / dist) * force * dt;
                separationY += (dy / dist) * force * dt;
                nearbyCount++;
            }
        }
        
        // Apply separation force
        if (nearbyCount > 0) {
            pos->x += separationX;
            pos->y += separationY;
        }
    }
    
    // Original movement logic
    for (auto& [id, tag] : spacecraftMap) {
        Health* health = ecs.getComponent<Health>(id);
        if (!health || !health->alive) continue;
        Position* pos = ecs.getComponent<Position>(id);
        if (!pos) continue;
        if (tag.moving) {
            float dx = tag.dest_x - pos->x;
            float dy = tag.dest_y - pos->y;
            float dist = std::sqrt(dx*dx + dy*dy);
            if (dist > 0.005f) {
                float speed = 0.01f;
                pos->x += speed * dx / dist;
                pos->y += speed * dy / dist;
                tag.angle = atan2f(dy, dx) * 180.0f / 3.1415926f - 90.0f;
            } else {
                pos->x = tag.dest_x;
                pos->y = tag.dest_y;
                tag.moving = false;
            }
        }
        // Player ships: auto-attack nearest enemy or follow orders
        if (tag.type == SpacecraftType::Player) {
            // Find nearest enemy within range
            EntityID nearestEnemyId = 0;
            float nearestEnemyDist = 1000.0f;
            constexpr float AUTO_ATTACK_RANGE = 0.15f; // Slightly larger than attack range
            
            auto& spacecraftMap2 = ecs.getComponents<SpacecraftTag>();
            for (auto& [eid, etag] : spacecraftMap2) {
                Health* enemyHealth = ecs.getComponent<Health>(eid);
                if (etag.type == SpacecraftType::Enemy && enemyHealth && enemyHealth->alive) {
                    Position* epos = ecs.getComponent<Position>(eid);
                    if (!epos) continue;
                    float dx = epos->x - pos->x;
                    float dy = epos->y - pos->y;
                    float dist = std::sqrt(dx*dx + dy*dy);
                    if (dist < AUTO_ATTACK_RANGE && dist < nearestEnemyDist) {
                        nearestEnemyDist = dist;
                        nearestEnemyId = eid;
                    }
                }
            }
            
            // Auto-attack if enemy in range and not already attacking or moving
            if (nearestEnemyId && !tag.attacking && !tag.moving) {
                tag.attacking = true;
            }
            
            if (tag.attacking) {
                // Find current target (use nearest enemy if no specific target)
                EntityID targetId = nearestEnemyId;
                if (!targetId) {
                    // No enemies in range, stop attacking
                    tag.attacking = false;
                } else {
                    Position* epos = ecs.getComponent<Position>(targetId);
                    if (!epos) {
                        tag.attacking = false;
                    } else {
                        float dx = epos->x - pos->x;
                        float dy = epos->y - pos->y;
                        float dist = std::sqrt(dx*dx + dy*dy);
                        constexpr float ATTACK_RANGE = 0.12f;
                        if (dist < ATTACK_RANGE) {
                            tag.moving = false;
                            tag.dest_x = pos->x;
                            tag.dest_y = pos->y;
                            tag.angle = atan2f(dy, dx) * 180.0f / 3.1415926f - 90.0f;
                            tag.attack_cooldown -= dt;
                            if (tag.attack_cooldown <= 0.0f) {
                                float norm = std::sqrt(dx*dx + dy*dy);
                                float vx = dx / norm * 0.02f;
                                float vy = dy / norm * 0.02f;
                                EntityID projId = ecs.createEntity();
                                ecs.addComponent<Position>(projId, Position{ pos->x, pos->y });
                                ecs.addComponent<Velocity>(projId, Velocity{ vx, vy });
                                ecs.addComponent<ProjectileTag>(projId, ProjectileTag{ id, targetId, 2.0f, true });
                                ecs.audioEvents.push_back(AudioEventType::Pew);
                                tag.attack_cooldown = 1.5f;
                            }
                        } else if (dist < AUTO_ATTACK_RANGE) {
                            // Move closer to enemy
                            tag.dest_x = epos->x;
                            tag.dest_y = epos->y;
                            tag.moving = true;
                        } else {
                            // Enemy too far, stop attacking
                            tag.attacking = false;
                        }
                    }
                }
            }
        } else if (tag.type == SpacecraftType::Enemy) {
            // Enemy AI: lock onto a single player unit until destroyed
            EntityID targetId = 0;
            float minDist = 1000.0f;
            auto& spacecraftMap3 = ecs.getComponents<SpacecraftTag>();
            for (auto& [pid, ptag] : spacecraftMap3) {
                Health* playerHealth = ecs.getComponent<Health>(pid);
                if (ptag.type == SpacecraftType::Player && playerHealth && playerHealth->alive) {
                    Position* ppos = ecs.getComponent<Position>(pid);
                    if (!ppos) continue;
                    float dx = ppos->x - pos->x;
                    float dy = ppos->y - pos->y;
                    float dist = std::sqrt(dx*dx + dy*dy);
                    if (dist < minDist) {
                        minDist = dist;
                        targetId = pid;
                    }
                }
            }
            constexpr float ATTACK_RANGE = 0.12f;
            constexpr float ATTACK_STOP_RANGE = 0.10f;
            if (targetId) {
                Position* tpos = ecs.getComponent<Position>(targetId);
                if (!tpos) return;
                float dx = tpos->x - pos->x;
                float dy = tpos->y - pos->y;
                float dist = std::sqrt(dx*dx + dy*dy);
                tag.angle = atan2f(dy, dx) * 180.0f / 3.1415926f - 90.0f;
                if (std::abs(tag.dest_x - tpos->x) > 1e-4f || std::abs(tag.dest_y - tpos->y) > 1e-4f) {
                    tag.dest_x = tpos->x;
                    tag.dest_y = tpos->y;
                }
                if (dist > ATTACK_RANGE) {
                    tag.moving = true;
                } else if (dist < ATTACK_STOP_RANGE) {
                    tag.moving = false;
                    tag.dest_x = pos->x;
                    tag.dest_y = pos->y;
                    tag.attack_cooldown -= dt;
                    if (tag.attack_cooldown <= 0.0f) {
                        float norm = std::sqrt(dx*dx + dy*dy);
                        float vx = dx / norm * 0.02f;
                        float vy = dy / norm * 0.02f;
                        EntityID projId = ecs.createEntity();
                        ecs.addComponent<Position>(projId, Position{ pos->x, pos->y });
                        ecs.addComponent<Velocity>(projId, Velocity{ vx, vy });
                        ecs.addComponent<ProjectileTag>(projId, ProjectileTag{ id, targetId, 2.0f, true });
                        ecs.audioEvents.push_back(AudioEventType::Pew);
                        tag.attack_cooldown = 1.5f;
                    }
                } else {
                    tag.moving = true;
                }
            } else {
                tag.moving = false;
                tag.attack_cooldown = std::max(0.0f, tag.attack_cooldown - dt);
            }
        } else {
            tag.attack_cooldown = std::max(0.0f, tag.attack_cooldown - dt);
        }
    }
}
