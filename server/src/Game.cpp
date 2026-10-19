#include "Game.hpp"
#include "engine/systems/Movement.hpp"
#include "common/Components.hpp"

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
    engine::ecs::Entity entity = spawnNetworked(protocol::EntityType::Player, static_cast<std::uint8_t>(player), 100.0f, 300.0f);
    //_registry.add_component<rtype::components::Position>(entity, {100.0f, 300.0f});
    //_registry.add_component<rtype::components::Velocity>(entity, {0.0f, 0.0f});
    //_registry.add_component<rtype::components::Kind>(entity, {rtype::components::EntityKind::Player});
    _players[player] = entity;
}

// TODO(gameplay): kill the player's ship and add an EntityDestroyed event.
void Game::onPlayerLeft(PlayerId /* player */) {}

// TODO(gameplay): turn the input flags into the ship's velocity, handle shooting.
void Game::onInput(PlayerId player, std::uint8_t flags)
{
    //we get the entity associated with the player
    engine::ecs::Entity entity = _players[player];
    if (entity == engine::ecs::NullEntity)
        return;
    float speed = 200.0f;
    float vx = 0.0f;
    float vy = 0.0f;
    //we check the input flags and set the velocity accordingly
    if (flags & protocol::InputFlag::Up)
        vy -= speed;
    if (flags & protocol::InputFlag::Down)
        vy += speed;
    if (flags & protocol::InputFlag::Left)
        vx -= speed;
    if (flags & protocol::InputFlag::Right)
        vx += speed;
    //on met a jour (ou ajout) la vitesse du joueur
    _registry.emplace<rtype::components::Velocity>(entity, vx, vy);
    //shoot later don't forget !!!!!!
}

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
