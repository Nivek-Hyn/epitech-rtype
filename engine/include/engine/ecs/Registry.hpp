#pragma once

#include "engine/ecs/Entity.hpp"
#include "engine/ecs/SparseArray.hpp"

#include <functional>
#include <memory>
#include <stdexcept>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace engine::ecs {

// Owns all entities, their components, and the systems that process them.
//
//   Registry registry;
//   Entity ship = registry.spawn();
//   registry.emplace<Position>(ship, 10.f, 20.f);
//   registry.each<Position, Velocity>([](Entity entity, Position &position, Velocity &velocity) { ... });
//
// See docs/adr/0006-ecs-sparse-arrays.md. Expected behavior is specified by tests/test_registry.cpp.
class Registry {
public:
    using System = std::function<void(Registry &, float dt)>;

    // Return a new entity id, reusing a killed entity's id when available. NullEntity is reserved.
    Entity spawn();
    // Remove every component of entity and make its id reusable. Does nothing if not alive.
    void kill(Entity entity);
    // True if entity was spawned and not killed.
    bool alive(Entity entity) const;

    // TODO(engine): the SparseArray storing every component of type C (create it on first use).
    template <typename C> SparseArray<C> &components()
    {
        throw std::logic_error("Registry::components not implemented");
    }

    // TODO(engine): add a component C built from args to entity, return it.
    template <typename C, typename... Args> C &emplace(Entity /* entity */, Args &&.../* args */)
    {
        throw std::logic_error("Registry::emplace not implemented");
    }

    // TODO(engine): remove the component C of entity.
    template <typename C> void remove(Entity /* entity */)
    {
        throw std::logic_error("Registry::remove not implemented");
    }

    // TODO(engine): pointer to the component C of entity, or nullptr.
    template <typename C> C *get(Entity /* entity */) { throw std::logic_error("Registry::get not implemented"); }

    // TODO(engine): true if entity has a component C.
    template <typename C> bool has(Entity /* entity */) { throw std::logic_error("Registry::has not implemented"); }

    // TODO(engine): call func(entity, C1&, C2&, ...) for every entity that has ALL the listed components.
    template <typename... Components, typename Function> void each(Function && /* func */)
    {
        throw std::logic_error("Registry::each not implemented");
    }

    // TODO(engine): systems run in the order they were added, each one receiving dt.
    void addSystem(System system);
    void update(float dt);

private:
    // Suggested storage, free to change: only the public functions above are the contract.
    std::unordered_map<std::type_index, std::unique_ptr<ISparseArray>> _componentArrays;
    std::vector<bool> _alive;
    std::vector<Entity> _freeIds;
    std::vector<System> _systems;
};

} // namespace engine::ecs
