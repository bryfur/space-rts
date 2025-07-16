#pragma once

#include "../core/ECSRegistry.h"
#include "../core/SystemBase.h"
#include "../components/Components.h"

// Forward declarations
class AudioManager;

/**
 * @brief System for handling combat mechanics, shooting, and weapon systems
 */
class CombatSystem : public SystemBase {
public:
    explicit CombatSystem(ECSRegistry& registry);
    ~CombatSystem() override;

    // Set audio manager for sound effects
    void SetAudioManager(AudioManager* audioManager) { mAudioManager = audioManager; }

    // SystemBase interface
    bool Initialize() override;
    void Update(float deltaTime) override;
    void Shutdown() override;

    // Combat actions
    void FireWeapon(EntityID shooter, EntityID targetEntity, float targetX, float targetY);
    void ProcessAutomaticFiring(float deltaTime);

private:
    // Combat logic
    void UpdateWeaponCooldowns(float deltaTime);
    void ProcessEnemyAI(float deltaTime);
    void ProcessPlayerAutoAttack(float deltaTime);
    void CreateProjectile(EntityID shooter, float startX, float startY, 
                         float directionX, float directionY, float speed = 2.0F, EntityID targetEntity = INVALID_ENTITY);
    
    // AI helpers
    EntityID FindNearestTarget(EntityID attacker, float maxRange) const;
    EntityID FindNearestPlanet(EntityID attacker, float maxRange) const;
    float CalculateDistance(float x1, float y1, float x2, float y2) const;
    void CalculateDirection(float fromX, float fromY, float toX, float toY, 
                           float& dirX, float& dirY) const;
    
    // Advanced AI tactical analysis
    struct TacticalInfo {
        int nearbyPlayerShips;
        int nearbyEnemyShips;
        float nearestPlayerDistance;
        float nearestPlanetDistance;
        EntityID vulnerablePlanet;
        bool playerOverwhelmed;
        bool safeToAttackPlanet;
        // New group coordination fields
        int totalEnemyForces;
        int totalPlayerForces;
        bool shouldMassAttack;
        bool shouldSurround;
        EntityID primaryTarget;
    };
    
    // Group coordination structures
    struct GroupFormation {
        EntityID leader;
        std::vector<EntityID> members;
        EntityID target;
        float formationCenterX;
        float formationCenterY;
        enum FormationType { MASS_ATTACK, SURROUND, RETREAT, PATROL } type;
        float activationTime;
        bool isActive;
    };
    
    TacticalInfo AnalyzeTacticalSituation(EntityID enemy) const;
    EntityID FindMostVulnerablePlanet(EntityID attacker, float maxRange) const;
    EntityID FindWeakestTarget(EntityID attacker, float maxRange) const;
    bool ShouldRetreat(EntityID enemy, const TacticalInfo& tactical) const;
    bool ShouldAggressivelyAttackPlanet(EntityID enemy, const TacticalInfo& tactical) const;
    
    // Advanced group coordination methods
    void CoordinateGroupTactics(float deltaTime);
    bool ShouldInitiateMassAttack() const;
    bool ShouldInitiateSurroundManeuver(EntityID target) const;
    EntityID SelectStrategicTarget() const;
    void ExecuteMassAttack(const GroupFormation& formation);
    void ExecuteSurroundManeuver(const GroupFormation& formation);
    void AssignFormationPositions(GroupFormation& formation);
    std::vector<EntityID> GetAllEnemyUnits() const;
    float CalculateGroupCohesion() const;

    // Formation integration helpers
    const GroupFormation* GetActiveFormation(EntityID entity) const;
    void CalculateFormationPosition(const GroupFormation& formation, size_t unitIndex, float& posX, float& posY) const;
    bool ShouldPrioritizeFormationTarget(EntityID entity, const GroupFormation& formation) const;
    EntityID SelectBestTarget(EntityID entity, const GroupFormation* formation, bool engagementRangeOnly) const;

    // Unified AI state machine methods (formation-aware)
    Components::AIState UpdateAIStateMachine(EntityID entity, const Components::Spacecraft& spacecraft, const TacticalInfo& tactical);
    void ExecuteAIState(EntityID entity, Components::Spacecraft& spacecraft, const TacticalInfo& tactical);
    void ExecuteSearchState(EntityID entity, Components::Spacecraft& spacecraft, const TacticalInfo& tactical);
    void ExecuteApproachState(EntityID entity, Components::Spacecraft& spacecraft, const TacticalInfo& tactical);
    void ExecuteEngageState(EntityID entity, Components::Spacecraft& spacecraft, const TacticalInfo& tactical);
    void ExecuteRetreatState(EntityID entity, Components::Spacecraft& spacecraft, const TacticalInfo& tactical);
    void ExecuteRegroupState(EntityID entity, Components::Spacecraft& spacecraft, const TacticalInfo& tactical);
    bool HasHealthyAlliesNearby(EntityID entity, const TacticalInfo& tactical) const;
    EntityID FindHealthyAllyToHideBehind(EntityID entity, EntityID threat) const;
    EntityID FindBestRegroupTarget(EntityID entity) const;
    bool CanPickOffWeakTargets(EntityID entity, const TacticalInfo& tactical) const;
    void ApplyScreenBoundaries(float& posX, float& posY) const;
    void SetRetreatDestination(Components::Spacecraft& spacecraft, float currentX, float currentY, float targetX, float targetY);
    
    // Combat constants
    static constexpr float WEAPON_COOLDOWN = 1.0F; // seconds between shots
    static constexpr float PROJECTILE_SPEED = 1.0F;
    static constexpr float PROJECTILE_LIFETIME = 1.5F; // Reduced for shorter range
    static constexpr float AI_FIRING_RANGE = 0.5F; // Reduced to match projectile range
    static constexpr float AI_UPDATE_INTERVAL = 0.1F; // AI decision making frequency
    
    // Advanced AI tactical constants
    static constexpr float TACTICAL_ANALYSIS_RANGE = 0.8F; // Range for counting nearby units
    static constexpr float PLANET_ATTACK_RANGE = 0.6F; // Range for planet attacks
    static constexpr float RETREAT_THRESHOLD = 0.3F; // Health percentage to trigger retreat
    static constexpr int OVERWHELMING_RATIO = 2; // Enemy-to-player ratio for aggressive behavior
    static constexpr float VULNERABLE_PLANET_RANGE = 0.4F; // Range to consider planet "vulnerable"
    
    // Group coordination constants
    static constexpr int MIN_MASS_ATTACK_SIZE = 2; // Minimum units for mass attack
    static constexpr int MIN_SURROUND_SIZE = 3; // Minimum units for surround maneuver
    static constexpr float MASS_ATTACK_RANGE = 1.2F; // Range to coordinate mass attacks
    static constexpr float SURROUND_RADIUS = 0.3F; // Radius for surrounding formation
    static constexpr float GROUP_COORDINATION_INTERVAL = 1.0F; // Seconds between group coordination
    static constexpr float FORMATION_TOLERANCE = 0.1F; // Distance tolerance for formation positions
    
    // Screen boundary constants (assuming normalized coordinates from -1 to 1)
    static constexpr float SCREEN_BOUNDARY_MIN = -0.9F; // Leave small margin from edge
    static constexpr float SCREEN_BOUNDARY_MAX = 0.9F;  // Leave small margin from edge
    
    // Combat state
    float mAIUpdateTimer;
    
    // Group coordination state
    float mGroupCoordinationTimer;
    std::vector<GroupFormation> mActiveFormations;
    EntityID mCurrentStrategicTarget;
    bool mMassAttackInProgress;
    bool mSurroundInProgress;
    
    // Audio integration
    AudioManager* mAudioManager;
};
