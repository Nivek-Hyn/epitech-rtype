#pragma once

#include "engine/ecs/Entity.hpp"
#include "engine/ecs/SparseArray.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <stdexcept>
#include <tuple>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <utility>
#include <vector>

namespace engine::ecs {

/// @brief Owns all entities, their components, and the systems that process them.
///
/// @code
/// Registry registry;
/// Entity ship = registry.spawn();
/// registry.emplace<Position>(ship, 10.f, 20.f);
/// registry.each<Position, Velocity>([](Entity entity, Position &position, Velocity &velocity) { ... });
/// @endcode
///
/// See docs/adr/0006-ecs-sparse-arrays.md. Expected behavior is specified by tests/test_registry.cpp.
class Registry {
public:
    /// @brief A system: a function called by update() with this Registry and the step duration in seconds.
    using System = std::function<void(Registry &, float timeStepSeconds)>;

    /// @brief Create an entity.
    ///
    /// @return A new id, or the id of the last killed entity when one is available. Never NullEntity.
    /// @throws std::overflow_error if every id except NullEntity is already used.
    Entity spawn();
    /// @brief Remove every component of @p entity and make its id reusable. Does nothing if it is not alive.
    void kill(Entity entity);
    /// @brief True if @p entity was spawned and not killed since.
    bool alive(Entity entity) const;

    /// @brief The storage of every component of type @p C, created on first use.
    template <typename C> SparseArray<C> &components()
    {
        if (auto *storage = findComponents<C>())
            return *storage;

        auto storage = std::make_unique<SparseArray<C>>();
        SparseArray<C> &result = *storage;
        _componentArrays.emplace(std::type_index(typeid(C)), std::move(storage));
        return result;
    }

    /// @brief Add or replace the component @p C of a live entity.
    ///
    /// @param entity Owner of the component.
    /// @param args Arguments forwarded to the constructor of @p C.
    /// @return The new component.
    /// @throws std::invalid_argument if @p entity is not alive.
    template <typename C, typename... Args> C &emplace(Entity entity, Args &&...args)
    {
        if (!alive(entity))
            throw std::invalid_argument("Cannot add a component to an entity that is not alive");
        return components<C>().emplace(entity, std::forward<Args>(args)...);
    }

    /// @brief Remove the component @p C of @p entity. Does nothing if it has none.
    template <typename C> void remove(Entity entity)
    {
        if (auto *storage = findComponents<C>())
            storage->erase(entity);
    }

    /// @brief Borrow the component @p C of a live entity.
    ///
    /// @return A pointer to the component, or `nullptr` if the entity is not alive or has no @p C.
    template <typename C> C *get(Entity entity)
    {
        if (!alive(entity))
            return nullptr;
        if (auto *storage = findComponents<C>())
            return storage->get(entity);
        return nullptr;
    }

    /// @brief True if @p entity is alive and has a component @p C.
    template <typename C> bool has(Entity entity) { return get<C>(entity) != nullptr; }

    /// @brief Call `func(entity, C1 &, C2 &, ...)` for each live entity having ALL the listed components.
    ///
    /// The callback must not keep the references, nor invalidate them by adding components of the visited
    /// types or killing entities: collect entities first, or use EventBus::enqueue.
    ///
    /// @tparam Components Component types an entity must have to be visited.
    /// Each storage is looked up once per call, not once per entity; a missing type means nothing to visit.
    template <typename... Components, typename Function> void each(Function &&func)
    {
        const std::tuple<SparseArray<Components> *...> storages{findComponents<Components>()...};
        if ((!std::get<SparseArray<Components> *>(storages) || ...))
            return;
        const std::size_t slotCount = _alive.size(); // entities created by the callback are not visited
        for (std::size_t slot = 0; slot < slotCount; slot++) {
            if (!_alive[slot])
                continue;
            const Entity entity = makeEntity(static_cast<std::uint32_t>(slot), _generations[slot]);
            if ((std::get<SparseArray<Components> *>(storages)->has(entity) && ...))
                func(entity, *std::get<SparseArray<Components> *>(storages)->get(entity)...);
        }
    }

    /// @brief Register a system, run by update() after the systems added before it.
    ///
    /// @throws std::invalid_argument if @p system is empty.
    /// @warning Do not add a system from inside a running system.
    void addSystem(System system);
    /// @brief Run every system once, in registration order.
    ///
    /// @param timeStepSeconds Duration of the simulated step, passed to each system.
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
    std::vector<bool> _alive;                // by slot
    std::vector<std::uint32_t> _generations; // by slot: increased each time the slot is freed
    std::vector<std::uint32_t> _freeIds;     // free slots, reused last freed first
    std::vector<System> _systems;
};

} // namespace engine::ecs
