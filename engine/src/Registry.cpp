#include "engine/ecs/Registry.hpp"

namespace engine::ecs {

Entity Registry::spawn()
{
    if (!_freeIds.empty()) {
        const Entity entity = _freeIds.back();
        _freeIds.pop_back();
        _alive[entity] = true;
        return entity;
    }
    _alive.push_back(true);
    return static_cast<Entity>(_alive.size() - 1);
}

void Registry::kill(Entity entity)
{
    if (!alive(entity))
        return;
    for (auto &[type, array] : _arrays)
        array->erase(entity);
    _alive[entity] = false;
    _freeIds.push_back(entity);
}

bool Registry::alive(Entity entity) const
{
    return entity < _alive.size() && _alive[entity];
}

void Registry::addSystem(System system)
{
    _systems.push_back(std::move(system));
}

void Registry::update(float dt)
{
    for (auto &system : _systems)
        system(*this, dt);
}

} // namespace engine::ecs
