#pragma once

#include "engine/ecs/Entity.hpp"

#include <cstddef>
#include <optional>
#include <stdexcept>
#include <vector>

namespace engine::ecs {

// Type-erased base so the Registry can erase a component of any type when an entity dies.
class ISparseArray {
public:
    virtual ~ISparseArray() = default;
    virtual void erase(Entity entity) = 0;
};

// Stores every component of type C, indexed by entity: slot [entity] is empty if the entity doesn't have one.
// See docs/adr/0006-ecs-sparse-arrays.md.
template <typename C> class SparseArray final : public ISparseArray {
public:
    // TODO(engine): store a C built from args at index entity (grow the storage if needed), return it.
    template <typename... Args> C &emplace([[maybe_unused]] Entity entity, [[maybe_unused]] Args &&...args)
    {
        throw std::logic_error("SparseArray::emplace not implemented");
    }

    // TODO(engine): empty the slot of entity. Must do nothing if entity is out of range.
    void erase([[maybe_unused]] Entity entity) override
    {
        throw std::logic_error("SparseArray::erase not implemented");
    }

    // TODO(engine): true if entity has a component of this type.
    [[nodiscard]] bool has([[maybe_unused]] Entity entity) const
    {
        throw std::logic_error("SparseArray::has not implemented");
    }

    // TODO(engine): pointer to the component of entity, or nullptr if it has none.
    [[nodiscard]] C *get([[maybe_unused]] Entity entity) { throw std::logic_error("SparseArray::get not implemented"); }
    [[nodiscard]] const C *get([[maybe_unused]] Entity entity) const
    {
        throw std::logic_error("SparseArray::get not implemented");
    }

    // TODO(engine): number of slots (highest entity index stored + 1).
    [[nodiscard]] std::size_t size() const { throw std::logic_error("SparseArray::size not implemented"); }

private:
    std::vector<std::optional<C>> _data;
};

} // namespace engine::ecs
