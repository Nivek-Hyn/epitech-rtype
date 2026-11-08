#include "engine/ecs/Registry.hpp"

namespace engine::ecs {

Entity Registry::spawn()
{
    if (!_freeIds.empty()) {
        const std::uint32_t slot = _freeIds.back();
        _freeIds.pop_back();
        _alive[slot] = true;
        return makeEntity(slot, _generations[slot]);
    }

    // The last slot is reserved: with the highest generation it would be NullEntity.
    if (_alive.size() >= EntityIndexMask)
        throw std::overflow_error("Registry has no entity ids left");

    const auto slot = static_cast<std::uint32_t>(_alive.size());
    _alive.push_back(true);
    _generations.push_back(0);
    return makeEntity(slot, 0);
}

void Registry::kill(Entity entity)
{
    if (!alive(entity))
        return;

    for (auto &entry : _componentArrays)
        entry.second->erase(entity);

    const std::uint32_t slot = indexOf(entity);
    _alive[slot] = false;
    _generations[slot] = (_generations[slot] + 1) & 0xFFu; // old handles to this slot are now stale
    _freeIds.push_back(slot);
}

bool Registry::alive(Entity entity) const
{
    const std::uint32_t slot = indexOf(entity);
    return entity != NullEntity && slot < _alive.size() && _alive[slot] && _generations[slot] == generationOf(entity);
}

void Registry::addSystem(System system)
{
    if (!system)
        throw std::invalid_argument("Cannot register an empty system");
    _systems.push_back(std::move(system));
}

void Registry::update(float timeStepSeconds)
{
    for (auto &system : _systems)
        system(*this, timeStepSeconds);
}

} // namespace engine::ecs
