#pragma once

#include "engine/ecs/Entity.hpp"
#include "engine/ecs/SparseArray.hpp"

#include <algorithm>
#include <functional>
#include <memory>
#include <tuple>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

namespace engine::ecs {

// Owns all entities, their components, and the systems that process them.
//
//   Registry reg;
//   Entity ship = reg.spawn();
//   reg.emplace<Position>(ship, 10.f, 20.f);
//   reg.each<Position, Velocity>([](Entity e, Position &p, Velocity &v) { ... });
//
// Note: don't emplace a component of a type you are currently iterating over in each(),
// it may reallocate the storage. Spawning, killing and removing are fine.
class Registry {
public:
    using System = std::function<void(Registry &, float dt)>;

    Entity spawn();
    void kill(Entity entity);
    [[nodiscard]] bool alive(Entity entity) const;

    template <typename C> SparseArray<C> &components()
    {
        auto &slot = _arrays[std::type_index(typeid(C))];
        if (!slot)
            slot = std::make_unique<SparseArray<C>>();
        return static_cast<SparseArray<C> &>(*slot);
    }

    template <typename C, typename... Args> C &emplace(Entity entity, Args &&...args)
    {
        return components<C>().emplace(entity, std::forward<Args>(args)...);
    }

    template <typename C> void remove(Entity entity) { components<C>().erase(entity); }

    template <typename C> [[nodiscard]] C *get(Entity entity) { return components<C>().get(entity); }

    template <typename C> [[nodiscard]] bool has(Entity entity) { return components<C>().has(entity); }

    // Calls func(entity, C1&, C2&, ...) for every entity that has all the listed components.
    template <typename... Cs, typename F> void each(F &&func)
    {
        static_assert(sizeof...(Cs) > 0, "each() needs at least one component type");
        std::tuple<SparseArray<Cs> &...> arrays(components<Cs>()...);
        const std::size_t count = std::min({std::get<SparseArray<Cs> &>(arrays).size()...});

        for (std::size_t i = 0; i < count; ++i) {
            const auto entity = static_cast<Entity>(i);
            if ((std::get<SparseArray<Cs> &>(arrays).has(entity) && ...))
                func(entity, *std::get<SparseArray<Cs> &>(arrays).get(entity)...);
        }
    }

    // Systems run in the order they were added.
    void addSystem(System system);
    void update(float dt);

private:
    std::unordered_map<std::type_index, std::unique_ptr<ISparseArray>> _arrays;
    std::vector<bool> _alive;
    std::vector<Entity> _freeIds;
    std::vector<System> _systems;
};

} // namespace engine::ecs
