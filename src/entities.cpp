
#include "entities.h"
#include "spacecraft.h"
#include "projectile.h"
#include "planet.h"

void ECSRegistry::updateSystems(float dt) {
    updateSpacecrafts(*this, dt);
    updateProjectiles(*this, dt);
    updatePlanets(*this, dt);
}

auto ECSRegistry::createEntity() -> EntityID {
    EntityID id = nextID++;
    entities.push_back(id);
    return id;
}
