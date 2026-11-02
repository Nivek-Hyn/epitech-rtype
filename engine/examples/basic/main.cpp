#include <engine/ecs/Registry.hpp>
#include <engine/systems/Movement.hpp>

#include <cstdlib>
#include <iostream>
#include <vector>

struct Position {
    float x = 0.f;
    float y = 0.f;
};

struct Velocity {
    float x = 0.f;
    float y = 0.f;
};

// Defined by the consumer, without changing any engine header.
struct Lifetime {
    float remainingSeconds = 0.f;
};

void lifetimeSystem(engine::ecs::Registry &registry, float timeStepSeconds)
{
    std::vector<engine::ecs::Entity> expiredEntities;
    registry.each<Lifetime>([&](engine::ecs::Entity entity, Lifetime &lifetime) {
        lifetime.remainingSeconds -= timeStepSeconds;
        if (lifetime.remainingSeconds <= 0.f)
            expiredEntities.push_back(entity);
    });
    // Destroy entities only after each has finished using the component references.
    for (engine::ecs::Entity entity : expiredEntities)
        registry.kill(entity);
}

int main()
{
    engine::ecs::Registry registry;
    const engine::ecs::Entity entity = registry.spawn();
    registry.emplace<Position>(entity);
    registry.emplace<Velocity>(entity, 10.f, 0.f);

    const engine::ecs::Entity temporaryEntity = registry.spawn();
    registry.emplace<Position>(temporaryEntity);
    registry.emplace<Velocity>(temporaryEntity, 4.f, 0.f);
    registry.emplace<Lifetime>(temporaryEntity, 1.f);

    registry.addSystem(engine::systems::movement<Position, Velocity>);
    registry.addSystem(lifetimeSystem);
    registry.update(0.5f);

    const Position *position = registry.get<Position>(entity);
    const Position *temporaryPosition = registry.get<Position>(temporaryEntity);
    const Lifetime *lifetime = registry.get<Lifetime>(temporaryEntity);
    if (position == nullptr || position->x != 5.f || position->y != 0.f || temporaryPosition == nullptr ||
        temporaryPosition->x != 2.f || lifetime == nullptr || lifetime->remainingSeconds != 0.5f) {
        std::cerr << "Movement or lifetime update failed\n";
        return EXIT_FAILURE;
    }

    registry.update(0.5f);
    // Obtain fresh pointers after systems ran; temporaryEntity's components have been destroyed.
    position = registry.get<Position>(entity);
    if (!registry.alive(entity) || position == nullptr || position->x != 10.f || registry.alive(temporaryEntity) ||
        registry.get<Position>(temporaryEntity) != nullptr || registry.get<Velocity>(temporaryEntity) != nullptr ||
        registry.get<Lifetime>(temporaryEntity) != nullptr) {
        std::cerr << "Expiration did not preserve the permanent entity and remove the temporary entity\n";
        return EXIT_FAILURE;
    }

    std::cout << "Custom component and system: movement, lifetime and deferred destruction passed\n";
    return EXIT_SUCCESS;
}
