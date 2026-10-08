#include "engine/ecs/Registry.hpp"

namespace engine::ecs {

Entity Registry::spawn()
{
    throw std::logic_error("Registry::spawn not implemented");
}

void Registry::kill([[maybe_unused]] Entity entity)
{
    throw std::logic_error("Registry::kill not implemented");
}

bool Registry::alive([[maybe_unused]] Entity entity) const
{
    throw std::logic_error("Registry::alive not implemented");
}

void Registry::addSystem([[maybe_unused]] System system)
{
    throw std::logic_error("Registry::addSystem not implemented");
}

void Registry::update([[maybe_unused]] float dt)
{
    throw std::logic_error("Registry::update not implemented");
}

} // namespace engine::ecs
