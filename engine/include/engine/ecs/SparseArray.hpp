#pragma once

#include "engine/ecs/Entity.hpp"

#include <cstddef>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace engine::ecs {

// Type-erased base so the Registry can erase a component of any type when an entity dies.
class ISparseArray {
public:
    virtual ~ISparseArray() = default;
    virtual void erase(Entity entity) = 0;
};

template <typename C> class SparseArray final : public ISparseArray {
public:
    template <typename... Args> C &emplace(Entity entity, Args &&...args)
    {
        if (entity >= _data.size())
            _data.resize(static_cast<std::size_t>(entity) + 1);
        return _data[entity].emplace(std::forward<Args>(args)...);
    }

    void erase(Entity entity) override
    {
        if (entity < _data.size())
            _data[entity].reset();
    }

    bool has(Entity entity) const
    {
        if (entity < _data.size())
            return _data[entity].has_value();
        else
            return false;
    }

    C *get(Entity entity)
    {
        if (entity >= _data.size() || !_data[entity].has_value())
            return nullptr;
        else
            return &(*_data[entity]);
    }
    const C *get(Entity entity) const
    {
        if (entity >= _data.size() || !_data[entity].has_value())
            return nullptr;
        else
            return &(*_data[entity]);
    }

    std::size_t size() const { return _data.size(); }

private:
    std::vector<std::optional<C>> _data;
};

} // namespace engine::ecs
