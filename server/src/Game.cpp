#include "Game.hpp"

namespace rtype::server {

Game::Game()
{
    _players.fill(engine::ecs::NullEntity);
    // TODO(gameplay): register the systems here (movement, spawn, shooting, collisions, damage...).
}

// TODO(gameplay): spawn the player's ship (NetworkId, Kind, Position, Velocity...) and remember it in _players.
void Game::onPlayerJoined([[maybe_unused]] PlayerId player) {}

// TODO(gameplay): kill the player's ship and add an EntityDestroyed event.
void Game::onPlayerLeft([[maybe_unused]] PlayerId player) {}

// TODO(gameplay): turn the input flags into the ship's velocity, handle shooting.
void Game::onInput([[maybe_unused]] PlayerId player, [[maybe_unused]] std::uint8_t flags) {}

// TODO(gameplay): run the systems (_registry.update) and increase _tick.
void Game::update([[maybe_unused]] float dt) {}

// TODO(gameplay): one EntityState per entity having NetworkId + Kind + Position.
protocol::Snapshot Game::snapshot()
{
    return {};
}

// TODO(gameplay): return the events produced since the last call and clear them.
std::vector<protocol::Message> Game::takeEvents()
{
    return {};
}

} // namespace rtype::server
