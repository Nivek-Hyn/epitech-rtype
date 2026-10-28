#include <engine/ecs/Registry.hpp>
#include <engine/systems/Movement.hpp>

#include <cstdlib>

struct Position {
    float x = 0.f;
    float y = 0.f;
};

struct Velocity {
    float x = 0.f;
    float y = 0.f;
};

int main()
{
    engine::ecs::Registry registry;
    const engine::ecs::Entity entity = registry.spawn();
    registry.emplace<Position>(entity);
    registry.emplace<Velocity>(entity, 10.f, 0.f);
    registry.addSystem(engine::systems::movement<Position, Velocity>);
    registry.update(0.5f);

    const Position *position = registry.get<Position>(entity);
    if (position == nullptr || position->x != 5.f || position->y != 0.f)
        return EXIT_FAILURE;
    return EXIT_SUCCESS;
}
