#pragma once

#include "common/Constants.hpp"
#include "common/Protocol.hpp"
#include "engine/ecs/Registry.hpp"
#include "net/INetworkServer.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace rtype::server {

// The authoritative game simulation. It knows nothing about sockets:
// main.cpp feeds it network events and sends what it produces, so it can be tested without a network.
class Game {
public:
    Game();

    void onPlayerJoined(PlayerId player);
    void onPlayerLeft(PlayerId player);
    void onInput(PlayerId player, std::uint8_t flags);

    // Advances the simulation by one fixed step.
    void update(float dt);

    // Current state of every networked entity (split it if bigger than MaxEntitiesPerSnapshot).
    protocol::Snapshot snapshot();

    // Events produced since the last call (EntityDestroyed...), to broadcast.
    std::vector<protocol::Message> takeEvents();

    std::uint32_t tick() const { return _tick; }

private:
    engine::ecs::Entity spawnNetworked(protocol::EntityType type, std::uint8_t variant, float x, float y);
    void spawnEnemies(float dt);

    engine::ecs::Registry _registry;
    std::array<engine::ecs::Entity, MaxPlayers> _players;
    std::array<float, MaxPlayers> _shootCooldowns{};
    std::vector<protocol::Message> _events;
    std::uint32_t _tick = 0;
    std::uint32_t _nextNetId = 1;
    float _enemySpawnTimer = 0.f;
    std::uint32_t _enemiesSpawned = 0;
};

} // namespace rtype::server
