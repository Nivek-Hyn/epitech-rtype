#pragma once

#include "Game.hpp"
#include "common/Constants.hpp"
#include "net/INetworkServer.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

namespace rtype::server {

// How a server is started: by r-type_server from its arguments, or by a client hosting a game.
struct ServerSettings {
    std::uint16_t port = DefaultPort;
    Game::GameMode mode = Game::GameMode::Campaign;
    Game::Difficulty difficulty = Game::Difficulty::Normal;
    std::string level;          // a single level instead of the campaign (empty: the campaign)
    std::size_t startLevel = 0; // first campaign level, from 0
    int snapshotEvery = SnapshotEveryTicks;
};

// Applies the settings. Every level the game may load is read now: a broken file stops the start with
// std::runtime_error instead of crashing the server in the middle of a game.
void configureGame(Game &game, const ServerSettings &settings);

// Runs the game until `running` becomes false: network events, one fixed step, snapshots and events.
// A step that throws is reported and skipped, so one bad tick never stops the server. `everySecond` is called
// about once per second (for statistics); it may be empty.
void runServer(Game &game, INetworkServer &network, const ServerSettings &settings, const std::atomic<bool> &running,
               const std::function<void()> &everySecond = {});

} // namespace rtype::server
