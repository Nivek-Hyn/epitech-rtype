#pragma once

#include <cstdint>
#include <limits>

namespace engine::ecs {

// An entity is only an index. All its data lives in components.
using Entity = std::uint32_t;

inline constexpr Entity NullEntity = std::numeric_limits<Entity>::max();

} // namespace engine::ecs
