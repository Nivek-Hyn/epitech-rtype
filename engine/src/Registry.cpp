#include "engine/ecs/Registry.hpp"

namespace engine::ecs {

Entity Registry::spawn()
{
    throw std::logic_error("Registry::spawn not implemented");
}

void Registry::kill(Entity /* entity */)
{
    throw std::logic_error("Registry::kill not implemented");
}

bool Registry::alive(Entity /* entity */) const
{
    throw std::logic_error("Registry::alive not implemented");
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
