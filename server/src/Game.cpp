#include "Game.hpp"

namespace rtype::server {

Game::Game()
{
    _players.fill(engine::ecs::NullEntity);
    _registry.addSystem(engine::systems::movement<rtype::components::Position, rtype::components::Velocity>);
    // TODO(gameplay): register the systems here (movement, spawn, shooting, collisions, damage...).
}

// TODO(gameplay): spawn the player's ship (NetworkId, Kind, Position, Velocity...) and remember it in _players.
void Game::onPlayerJoined(PlayerId player)
{
    //we create a new entity in the ECS registry
    auto entity = _registry.spawn_entity();
    //we add the components to the entity(position, vitesse, type)
    _registry.add_component<rtype::components::Position>(entity, {100.0f, 300.0f});
    _registry.add_component<rtype::components::Velocity>(entity, {0.0f, 0.0f});
    _registry.add_component<rtype::components::Kind>(entity, {rtype::components::EntityKind::Player});
    _players[player] = entity;
}

// TODO(gameplay): kill the player's ship and add an EntityDestroyed event.
void Game::onPlayerLeft(PlayerId /* player */) {}

// TODO(gameplay): turn the input flags into the ship's velocity, handle shooting.
void Game::onInput(PlayerId /* player */, std::uint8_t /* flags */) {}

// TODO(gameplay): run the systems (_registry.update) and increase _tick.
void Game::update(float dt)
{
    _registry.update(dt);
    _tick++;
}

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
