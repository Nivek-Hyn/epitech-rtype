#pragma once

#include "engine/ecs/Entity.hpp"
#include "engine/ecs/SparseArray.hpp"

#include <functional>
#include <memory>
#include <stdexcept>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <utility>
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
    using System = std::function<void(Registry &, float timeStepSeconds)>;

    // Return a new entity id, reusing a killed entity's id when available. NullEntity is reserved.
    Entity spawn();
    // Remove every component of entity and make its id reusable. Does nothing if not alive.
    void kill(Entity entity);
    // True if entity was spawned and not killed.
    bool alive(Entity entity) const;

    // The SparseArray storing every component of type C (create it on first use).
    template <typename C> SparseArray<C> &components()
    {
        if (auto *storage = findComponents<C>())
            return *storage;

        auto storage = std::make_unique<SparseArray<C>>();
        SparseArray<C> &result = *storage;
        _componentArrays.emplace(std::type_index(typeid(C)), std::move(storage));
        return result;
    }

    // Add or replace a component on a live entity. Reject dead or unknown ids.
    template <typename C, typename... Args> C &emplace(Entity entity, Args &&...args)
    {
        if (!alive(entity))
            throw std::invalid_argument("Cannot add a component to an entity that is not alive");
        return components<C>().emplace(entity, std::forward<Args>(args)...);
    }

    // Remove the component C of entity. Missing components are ignored.
    template <typename C> void remove(Entity entity)
    {
        if (auto *storage = findComponents<C>())
            storage->erase(entity);
    }

    // Borrow the component C of a live entity, or return nullptr.
    template <typename C> C *get(Entity entity)
    {
        if (!alive(entity))
            return nullptr;
        if (auto *storage = findComponents<C>())
            return storage->get(entity);
        return nullptr;
    }

    // True if a live entity has a component C.
    template <typename C> bool has(Entity entity) { return get<C>(entity) != nullptr; }

    // Call func(entity, C1&, C2&, ...) for each live entity with ALL the listed components.
    // Callbacks must not retain references or invalidate them by growing a component storage.
    template <typename... Components, typename Function> void each(Function &&func)
    {
        const std::size_t entityCount = _alive.size();
        for (std::size_t i = 0; i < entityCount; i++) {
            const Entity entity = static_cast<Entity>(i);
            if (alive(entity) && (has<Components>(entity) && ...))
                func(entity, *get<Components>(entity)...);
        }
    }

    // Register non-empty systems before update; do not add systems from a running system.
    void addSystem(System system);
    // Run systems in registration order, passing this Registry and timeStepSeconds in seconds.
    void update(float timeStepSeconds);

private:
    // Look up an existing storage without creating one. Each key C owns a SparseArray<C>.
    template <typename C> SparseArray<C> *findComponents()
    {
        auto storageEntry = _componentArrays.find(std::type_index(typeid(C)));
        if (storageEntry == _componentArrays.end())
            return nullptr;
        return static_cast<SparseArray<C> *>(storageEntry->second.get());
    }

    // Suggested storage, free to change: only the public functions above are the contract.
    std::unordered_map<std::type_index, std::unique_ptr<ISparseArray>> _componentArrays;
    std::vector<bool> _alive;
    std::vector<Entity> _freeIds;
    std::vector<System> _systems;
};

} // namespace engine::ecs
