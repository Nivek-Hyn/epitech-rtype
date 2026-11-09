#pragma once

#include "engine/ecs/Registry.hpp"

#include <functional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace engine::ecs {

/// @brief Entity templates described by data: a prefab is a list of (component name, component data).
///
/// The game registers each component type under a name, with a function that reads it from its data
/// format (JSON, text...). The engine does not know that format: Data is any copyable type.
/// @code
/// engine::ecs::Prefabs<nlohmann::json> prefabs;
/// prefabs.registerComponent<Health>("Health", [](const nlohmann::json &data) { return Health{data.value("hp", 1)}; });
/// prefabs.add("grunt", {{"Health", {{"hp", 3}}}});
/// engine::ecs::Entity enemy = prefabs.spawn(registry, "grunt"); // has a Health of 3
/// @endcode
template <typename Data> class Prefabs {
public:
    using Components = std::vector<std::pair<std::string, Data>>;

    /// @brief Makes the component C available to prefabs under the given name.
    template <typename C> void registerComponent(const std::string &name, std::function<C(const Data &)> read)
    {
        _readers[name] = [read = std::move(read)](const Data &data) -> Adder {
            return
                [component = read(data)](Registry &registry, Entity entity) { registry.emplace<C>(entity, component); };
        };
    }

    /// @brief Adds or replaces a prefab. The data is read now, so a mistake is reported here and not in the
    /// middle of a game. Throws std::invalid_argument for an unregistered component name, or what the
    /// reading function throws; the prefab is then not added.
    void add(const std::string &name, const Components &components)
    {
        std::vector<Adder> adders;
        for (const auto &[component, data] : components) {
            const auto reader = _readers.find(component);
            if (reader == _readers.end())
                throw std::invalid_argument("Unknown component '" + component + "' in prefab '" + name + "'");
            adders.push_back(reader->second(data));
        }
        _prefabs[name] = std::move(adders);
    }

    bool contains(const std::string &name) const { return _prefabs.contains(name); }

    /// @brief Adds every component of the prefab to an existing entity, replacing those it already has.
    /// Throws std::out_of_range if the prefab is unknown.
    void applyTo(Registry &registry, Entity entity, const std::string &name) const
    {
        for (const Adder &add : _prefabs.at(name))
            add(registry, entity);
    }

    /// @brief Creates a new entity with every component of the prefab.
    /// Throws std::out_of_range if the prefab is unknown, before creating anything.
    Entity spawn(Registry &registry, const std::string &name) const
    {
        const std::vector<Adder> &adders = _prefabs.at(name);
        const Entity entity = registry.spawn();
        for (const Adder &add : adders)
            add(registry, entity);
        return entity;
    }

private:
    // Adds one already read component to an entity.
    using Adder = std::function<void(Registry &, Entity)>;

    std::unordered_map<std::string, std::function<Adder(const Data &)>> _readers;
    std::unordered_map<std::string, std::vector<Adder>> _prefabs;
};

} // namespace engine::ecs
