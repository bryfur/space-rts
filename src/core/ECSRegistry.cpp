#include "ECSRegistry.h"
#include <algorithm>

ECSRegistry::ECSRegistry() 
    : mNextEntityID(1) // Start from 1, 0 is INVALID_ENTITY
{
}

ECSRegistry::~ECSRegistry() {
    // Systems will be destroyed automatically by unique_ptr
    // Components will be destroyed by their custom deleters
}

EntityID ECSRegistry::CreateEntity() {
    EntityID newEntity = mNextEntityID++;
    return newEntity;
}

void ECSRegistry::DestroyEntity(EntityID entity) {
    if (entity == INVALID_ENTITY) {
        return;
    }

    // Remove all components for this entity
    for (auto& [typeIndex, storage] : mComponents) {
        storage.erase(entity);
    }

    mDestroyedEntities.push_back(entity);
}

auto ECSRegistry::GetComponentStorage(std::type_index type) -> std::unordered_map<EntityID, std::unique_ptr<void, void(*)(void*)>>& {
    return mComponents[type];
}
