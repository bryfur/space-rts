#pragma once
#include <vector>
#include <unordered_map>
#include <functional>
#include <cstdint>
#include <cmath>
#include <typeindex>
#include <any>

enum class AudioEventType { Pew, Boom, Beep };

using EntityID = std::uint32_t;

enum class SpacecraftType { Player, Enemy };

struct Position {
    float x = 0.0F, y = 0.0F;
};

struct Velocity {
    float dx = 0.0F, dy = 0.0F;
};

struct Health {
    int hp = 10;
    bool alive = true;
};

struct SpacecraftTag {
    SpacecraftType type = SpacecraftType::Player;
    float angle = 0.0F;
    bool selected = false;
    float dest_x = 0.0F, dest_y = 0.0F;
    bool moving = false;
    float attack_cooldown = 0.0F;
    bool attacking = false;
};

struct ProjectileTag {
    EntityID owner = 0;
    EntityID target = 0;
    float life = 1.0F;
    bool active = true;
};

struct PlanetTag {
    float radius = 0.1F;
};

class ECSRegistry {
public:
    EntityID nextID = 1;
    std::vector<EntityID> entities;
    std::vector<AudioEventType> audioEvents;
    void updateSystems(float dt);

    template<typename T>
    auto getComponents() -> std::unordered_map<EntityID, T>& {
        auto idx = std::type_index(typeid(T));
        if (!componentMaps.contains(idx)) {
            componentMaps[idx] = std::unordered_map<EntityID, T>{};
        }
        return std::any_cast<std::unordered_map<EntityID, T>&>(componentMaps[idx]);
    }

    template<typename T>
    void addComponent(EntityID entityId, const T& component) {
        getComponents<T>()[entityId] = component;
    }

    template<typename T>
    void removeComponent(EntityID entityId) {
        getComponents<T>().erase(entityId);
    }

    template<typename T>
    auto getComponent(EntityID entityId) -> T* {
        auto& map = getComponents<T>();
        auto mapIt = map.find(entityId);
        if (mapIt != map.end()) {
            return &mapIt->second;
        }
        return nullptr;
    }

    auto createEntity() -> EntityID;

private:
    std::unordered_map<std::type_index, std::any> componentMaps;
};

