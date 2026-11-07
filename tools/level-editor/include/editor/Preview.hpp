#pragma once

#include "editor/Level.hpp"

#include "engine/ecs/Registry.hpp"

#include <cstddef>

namespace editor {

struct Position {
    float x = 0.f;
    float y = 0.f;
};

struct Velocity {
    float x = 0.f;
    float y = 0.f;
};

// Index of the placement in Level::enemies, to draw the enemy like on the timeline.
struct Source {
    std::size_t index = 0;
};

// Plays a level with the engine: each enemy appears on the right edge at its time and moves left in a
// straight line at its speed. Movement patterns and shots stay on the server: this is only a timing check.
class Preview {
public:
    static constexpr float WorldWidth = 1920.f;

    explicit Preview(const Level &level);

    void step(float timeStepSeconds);
    float time() const { return _time; }
    engine::ecs::Registry &registry() { return _registry; }

private:
    void spawnSystem(engine::ecs::Registry &registry);

    const Level &_level;
    engine::ecs::Registry _registry;
    std::size_t _next = 0;           // first placement of _order not spawned yet
    std::vector<std::size_t> _order; // placements sorted by time
    float _time = 0.f;
};

} // namespace editor
