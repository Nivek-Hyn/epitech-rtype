#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>

namespace rtype {

// World coordinates: origin at the top-left, x to the right, y down, in "world units".
// The world is always 1920x1080 units; the client scales it to the window size.
inline constexpr float WorldWidth = 1920.f;
inline constexpr float WorldHeight = 1080.f;

// Side of a destructible tile, in world units.
inline constexpr float ObstacleSize = 72.f;

// Simulation (server and client) runs at a fixed rate.
inline constexpr double TickRate = 60.0;

inline constexpr std::uint16_t DefaultPort = 4242;
inline constexpr std::size_t MaxPlayers = 4;

// A client that sent nothing for this long is considered disconnected.
inline constexpr auto ClientTimeout = std::chrono::seconds(5);

// Client removes an entity that wasn't in any snapshot for this long (in case EntityDestroyed was lost).
inline constexpr auto EntityStaleTimeout = std::chrono::milliseconds(500);

} // namespace rtype
