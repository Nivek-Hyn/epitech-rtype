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

    if (_alive.size() >= static_cast<std::size_t>(NullEntity))
        throw std::overflow_error("Registry has no entity ids left");

    const Entity entity = static_cast<Entity>(_alive.size());
    _alive.push_back(true);
    return entity;
}

void Registry::kill(Entity entity)
{
    if (!alive(entity))
        return;

    for (auto &entry : _componentArrays)
        entry.second->erase(entity);

    _freeIds.push_back(entity);
    _alive[entity] = false;
}

bool Registry::alive(Entity entity) const
{
    return entity < _alive.size() && _alive[entity];
}

void Registry::addSystem(System /* system */)
{
    throw std::logic_error("Registry::addSystem not implemented");
}

void Registry::update(float /* dt */)
{
    throw std::logic_error("Registry::update not implemented");
}

} // namespace engine::ecs
