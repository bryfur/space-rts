#include "projectile.h"
#include <cmath>
#include <ranges>

void updateProjectiles(ECSRegistry& ecs, float dt) {
    // Use ranges to process projectiles
    constexpr float COLLISION_RADIUS = 0.04F;
    std::vector<EntityID> toRemove;
    auto& projectileMap = ecs.getComponents<ProjectileTag>();
    auto active_view = projectileMap | std::views::filter([](const auto& pair) { return pair.second.active; });
    for (auto&& [pid, ptag] : active_view) {
        Position* pos = ecs.getComponent<Position>(pid);
        Velocity* vel = ecs.getComponent<Velocity>(pid);
        if (!pos || !vel) continue;
        pos->x += vel->dx;
        pos->y += vel->dy;
        ptag.life -= dt;
        // Check collision with target
        if (ptag.target != 0) {
            Health* targetHealth = ecs.getComponent<Health>(ptag.target);
            Position* targetPos = ecs.getComponent<Position>(ptag.target);
            if (targetHealth && targetHealth->alive && targetPos) {
                float deltaX = targetPos->x - pos->x;
                float deltaY = targetPos->y - pos->y;
                float distance = std::sqrt((deltaX * deltaX) + (deltaY * deltaY));
                if (distance < COLLISION_RADIUS) {
                    ptag.active = false;
                    targetHealth->hp--;
                    if (targetHealth->hp <= 0) {
                        targetHealth->alive = false;
                        ecs.audioEvents.push_back(AudioEventType::Boom);
                    }
                }
            }
        }
        if (ptag.life <= 0.0F) {
            ptag.active = false;
        }
        if (!ptag.active) {
            toRemove.push_back(pid);
        }
    }
    for (EntityID pid : toRemove) {
        ecs.removeComponent<ProjectileTag>(pid);
        ecs.removeComponent<Position>(pid);
        ecs.removeComponent<Velocity>(pid);
    }
}
