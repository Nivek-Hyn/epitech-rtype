#pragma once

#include "engine/ecs/Entity.hpp"

#include <cstddef>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace engine::ecs {

/// @brief Type-erased base so the Registry can erase a component of any type when an entity dies.
class ISparseArray {
public:
    virtual ~ISparseArray() = default;
    /// @brief Remove the component of @p entity, if any.
    virtual void erase(Entity entity) = 0;
};

/// @brief Storage of every component of type @p C, indexed by entity.
///
/// Slot `i` holds the component of entity `i`, or is empty. Access is a direct index, with no search.
/// Growing the array can move every component: do not keep pointers or references across insertions.
///
/// @tparam C Component type.
template <typename C> class SparseArray final : public ISparseArray {
public:
    /// @brief Construct a component for @p entity in place, replacing any existing one.
    ///
    /// @param entity Owner of the component; the array grows to `entity + 1` slots if needed.
    /// @param args Arguments forwarded to the constructor of @p C.
    /// @return The new component.
    template <typename... Args> C &emplace(Entity entity, Args &&...args)
    {
        const std::size_t slot = indexOf(entity);
        if (slot >= _data.size())
            _data.resize(slot + 1);
        return _data[slot].emplace(std::forward<Args>(args)...);
    }

    /// @brief Empty the slot of @p entity. Other slots keep their index.
    void erase(Entity entity) override
    {
        if (indexOf(entity) < _data.size())
            _data[indexOf(entity)].reset();
    }

    /// @brief True if @p entity has a component in this array.
    bool has(Entity entity) const { return indexOf(entity) < _data.size() && _data[indexOf(entity)].has_value(); }

    /// @brief Borrow the component of @p entity.
    ///
    /// @return A pointer to the component, or `nullptr` if @p entity has none. The array keeps ownership.
    C *get(Entity entity)
    {
        const std::size_t slot = indexOf(entity);
        if (slot >= _data.size() || !_data[slot].has_value())
            return nullptr;
        return &(*_data[slot]);
    }
    /// @copydoc get(Entity)
    const C *get(Entity entity) const
    {
        const std::size_t slot = indexOf(entity);
        if (slot >= _data.size() || !_data[slot].has_value())
            return nullptr;
        return &(*_data[slot]);
    }

    /// @brief Number of slots, including empty ones.
    std::size_t size() const { return _data.size(); }

private:
    std::vector<std::optional<C>> _data;
};

} // namespace engine::ecs
