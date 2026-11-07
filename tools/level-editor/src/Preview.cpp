#include "editor/Preview.hpp"

#include "engine/systems/Movement.hpp"

#include <algorithm>
#include <numeric>
#include <vector>

namespace editor {

namespace {

// Removes enemies that left the screen. Ids are collected first: killing inside each is not allowed.
void cleanupSystem(engine::ecs::Registry &registry, float)
{
    std::vector<engine::ecs::Entity> gone;
    registry.each<Position>([&gone](engine::ecs::Entity entity, Position &position) {
        if (position.x < -200.f)
            gone.push_back(entity);
    });
    for (engine::ecs::Entity entity : gone)
        registry.kill(entity);
}

} // namespace

Preview::Preview(const Level &level) : _level(level), _order(level.enemies.size())
{
    std::iota(_order.begin(), _order.end(), std::size_t{0});
    std::stable_sort(_order.begin(), _order.end(), [&level](std::size_t first, std::size_t second) {
        return level.enemies[first].time < level.enemies[second].time;
    });

    // Same order as on the server: spawn, then move, then remove what left the screen.
    _registry.addSystem([this](engine::ecs::Registry &registry, float) { spawnSystem(registry); });
    _registry.addSystem(engine::systems::movement<Position, Velocity>);
    _registry.addSystem(cleanupSystem);
}

void Preview::step(float timeStepSeconds)
{
    _time += timeStepSeconds;
    _registry.update(timeStepSeconds);
}

void Preview::spawnSystem(engine::ecs::Registry &registry)
{
    for (; _next < _order.size() && _level.enemies[_order[_next]].time <= _time; _next++) {
        const Placement &placement = _level.enemies[_order[_next]];
        const engine::ecs::Entity enemy = registry.spawn();
        registry.emplace<Position>(enemy, WorldWidth, placement.y());
        registry.emplace<Velocity>(enemy, -placement.fields.value("speed", 100.f), 0.f);
        registry.emplace<Source>(enemy, _order[_next]);
    }
}

} // namespace editor
