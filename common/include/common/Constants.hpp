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

// The simulation runs 60 ticks per second but the server sends a snapshot only every this many ticks (30 per second):
// the client fills the gaps by interpolation. Fewer snapshots means less bandwidth, at the price of a longer delay
// before the client shows what happened. `r-type_server --snapshot-every N` changes it.
inline constexpr int SnapshotEveryTicks = 2;

// The client shows the world this many snapshot intervals in the past, to always have a snapshot on each side of
// the time being shown. Two keeps a single lost snapshot invisible.
inline constexpr double InterpolationDelaySnapshots = 2.0;

// The client sends Connect this often, and gives up (connection failed) after this many unanswered attempts.
inline constexpr auto ConnectRetryInterval = std::chrono::milliseconds(500);
inline constexpr int MaxConnectAttempts = 10;

// While connected, the client pings the server this often: it measures the latency and keeps the session alive.
inline constexpr auto PingInterval = std::chrono::seconds(1);

// Client removes an entity that wasn't in any snapshot for this long (in case EntityDestroyed was lost).
inline constexpr auto EntityStaleTimeout = std::chrono::milliseconds(500);

} // namespace rtype
