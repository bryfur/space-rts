#pragma once
#include <vector>
#include <cmath>

enum class SpacecraftType { Player, Enemy };

struct Projectile {
    float x, y;
    float dx, dy;
    float life = 1.0f;
    bool active = true;
    SpacecraftType owner = SpacecraftType::Player;
    int targetIdx = -1;
};

struct Spacecraft {
    float x, y, angle;
    bool selected = false;
    float dest_x = 0.0f, dest_y = 0.0f;
    bool moving = false;
    SpacecraftType type = SpacecraftType::Player;
    float attack_cooldown = 0.0f;
    bool attacking = false;
    int hp = 10;
    bool alive = true;
};

struct Planet {
    float x, y, radius;
};
