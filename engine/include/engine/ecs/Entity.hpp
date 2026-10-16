#pragma once

#include <cstdint>
#include <limits>

namespace engine::ecs {

/// @brief Identifier of an entity. An entity is only an index: all its data lives in components.
using Entity = std::uint32_t;

/// @brief Reserved value meaning "no entity". Registry::spawn never returns it.
inline constexpr Entity NullEntity = std::numeric_limits<Entity>::max();

} // namespace engine::ecs
