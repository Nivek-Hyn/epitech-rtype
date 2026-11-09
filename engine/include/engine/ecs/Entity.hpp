#pragma once

#include <cstdint>
#include <limits>

namespace engine::ecs {

/// @brief Identifier of an entity. An entity is only a handle: all its data lives in components.
///
/// The low 24 bits are the index of its storage slot; the high 8 bits are a generation, increased each time the
/// slot is freed. A handle kept after its entity was killed therefore no longer matches the entity that reuses the
/// slot: Registry::alive returns false and Registry::get returns nullptr for it. After 256 reuses of the same slot,
/// the generation wraps around.
using Entity = std::uint32_t;

/// @brief Reserved value meaning "no entity". Registry::spawn never returns it.
inline constexpr Entity NullEntity = std::numeric_limits<Entity>::max();

inline constexpr std::uint32_t EntityIndexBits = 24;
inline constexpr std::uint32_t EntityIndexMask = (1u << EntityIndexBits) - 1;

/// @brief Storage slot of the entity, used to index component arrays.
constexpr std::uint32_t indexOf(Entity entity)
{
    return entity & EntityIndexMask;
}

/// @brief How many times the slot of the entity had been freed when the entity was created (modulo 256).
constexpr std::uint32_t generationOf(Entity entity)
{
    return entity >> EntityIndexBits;
}

constexpr Entity makeEntity(std::uint32_t index, std::uint32_t generation)
{
    return ((generation & 0xFFu) << EntityIndexBits) | (index & EntityIndexMask);
}

} // namespace engine::ecs
