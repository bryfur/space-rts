#pragma once

// Forward declaration
class ECSRegistry;

/**
 * @brief Base class for all systems
 * 
 * Provides a common interface for all game systems. Systems are responsible
 * for updating entities with specific components each frame.
 */
class SystemBase {
public:
    virtual ~SystemBase() = default;
    
    /**
     * @brief Initialize the system
     * @return true if initialization succeeded
     */
    virtual bool Initialize() = 0;
    
    /**
     * @brief Update the system
     * @param deltaTime Time since last update in seconds
     */
    virtual void Update(float deltaTime) = 0;
    
    /**
     * @brief Shutdown the system and cleanup resources
     */
    virtual void Shutdown() = 0;

protected:
    /**
     * @brief Constructor for derived systems
     * @param registry Reference to the ECS registry
     */
    explicit SystemBase(ECSRegistry& registry) : mRegistry(registry) {}
    
    /// Reference to the ECS registry for component access
    ECSRegistry& mRegistry;
};
