#pragma once

#include <unordered_map>
#include <vector>
#include <memory>
#include <typeindex>
#include <functional>
#include <cstdint>

using EntityID = std::uint32_t;
constexpr EntityID INVALID_ENTITY = 0;

/**
 * @brief Professional Entity-Component-System registry
 * 
 * This is a complete rewrite of the ECS system to be more performant,
 * type-safe, and maintainable. It uses modern C++ features and patterns.
 */
class ECSRegistry {
public:
    ECSRegistry();
    ~ECSRegistry();

    /**
     * @brief Create a new entity
     * @return Unique entity ID
     */
    EntityID CreateEntity();

    /**
     * @brief Destroy an entity and all its components
     * @param entity The entity to destroy
     */
    void DestroyEntity(EntityID entity);

    /**
     * @brief Add a component to an entity
     * @tparam T Component type
     * @param entity Target entity
     * @param component Component data
     */
    template<typename T>
    void AddComponent(EntityID entity, const T& component);

    /**
     * @brief Remove a component from an entity
     * @tparam T Component type
     * @param entity Target entity
     */
    template<typename T>
    void RemoveComponent(EntityID entity);

    /**
     * @brief Get a component from an entity
     * @tparam T Component type
     * @param entity Target entity
     * @return Pointer to component or nullptr if not found
     */
    template<typename T>
    T* GetComponent(EntityID entity);

    /**
     * @brief Get a const component from an entity
     * @tparam T Component type
     * @param entity Target entity
     * @return Const pointer to component or nullptr if not found
     */
    template<typename T>
    const T* GetComponent(EntityID entity) const;

    /**
     * @brief Check if an entity has a component
     * @tparam T Component type
     * @param entity Target entity
     * @return true if entity has component
     */
    template<typename T>
    bool HasComponent(EntityID entity) const;

    /**
     * @brief Get all entities with a specific component type
     * @tparam T Component type
     * @return Vector of entity IDs
     */
    template<typename T>
    std::vector<EntityID> GetEntitiesWithComponent() const;

    /**
     * @brief Iterate over all entities with a specific component
     * @tparam T Component type
     * @param callback Function to call for each entity-component pair
     */
    template<typename T>
    void ForEach(std::function<void(EntityID, T&)> callback);

private:
    // Component storage - maps component type to entity->component map
    std::unordered_map<std::type_index, std::unordered_map<EntityID, std::unique_ptr<void, void(*)(void*)>>> mComponents;
    
    // Entity management
    EntityID mNextEntityID;
    std::vector<EntityID> mDestroyedEntities;

    /**
     * @brief Get or create component storage for a type
     * @param type Component type index
     * @return Reference to component storage
     */
    auto GetComponentStorage(std::type_index type) -> std::unordered_map<EntityID, std::unique_ptr<void, void(*)(void*)>>&;

    /**
     * @brief Custom deleter for typed components
     * @tparam T Component type
     * @param ptr Pointer to delete
     */
    template<typename T>
    static void ComponentDeleter(void* ptr) {
        delete static_cast<T*>(ptr);
    }
};

// Template implementations

template<typename T>
void ECSRegistry::AddComponent(EntityID entity, const T& component) {
    auto& storage = GetComponentStorage(std::type_index(typeid(T)));
    storage.emplace(entity, std::unique_ptr<void, void(*)(void*)>(
        new T(component), 
        ComponentDeleter<T>
    ));
}

template<typename T>
void ECSRegistry::RemoveComponent(EntityID entity) {
    auto& storage = GetComponentStorage(std::type_index(typeid(T)));
    storage.erase(entity);
}

template<typename T>
T* ECSRegistry::GetComponent(EntityID entity) {
    auto& storage = GetComponentStorage(std::type_index(typeid(T)));
    auto it = storage.find(entity);
    return (it != storage.end()) ? static_cast<T*>(it->second.get()) : nullptr;
}

template<typename T>
const T* ECSRegistry::GetComponent(EntityID entity) const {
    auto typeIndex = std::type_index(typeid(T));
    auto it = mComponents.find(typeIndex);
    if (it == mComponents.end()) {
        return nullptr;
    }
    
    auto entityIt = it->second.find(entity);
    return (entityIt != it->second.end()) ? static_cast<const T*>(entityIt->second.get()) : nullptr;
}

template<typename T>
bool ECSRegistry::HasComponent(EntityID entity) const {
    return GetComponent<T>(entity) != nullptr;
}

template<typename T>
std::vector<EntityID> ECSRegistry::GetEntitiesWithComponent() const {
    std::vector<EntityID> entities;
    auto typeIndex = std::type_index(typeid(T));
    auto it = mComponents.find(typeIndex);
    
    if (it != mComponents.end()) {
        entities.reserve(it->second.size());
        for (const auto& pair : it->second) {
            entities.push_back(pair.first);
        }
    }
    
    return entities;
}

template<typename T>
void ECSRegistry::ForEach(std::function<void(EntityID, T&)> callback) {
    auto& storage = GetComponentStorage(std::type_index(typeid(T)));
    for (auto& pair : storage) {
        callback(pair.first, *static_cast<T*>(pair.second.get()));
    }
}
