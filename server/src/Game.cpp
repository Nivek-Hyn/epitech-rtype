#include "Game.hpp"

#include "common/Components.hpp"

namespace rtype::server {

using engine::ecs::Entity;
using engine::ecs::NullEntity;
using engine::ecs::Registry;
using namespace rtype::components;

namespace {

constexpr float PlayerSpeed = 400.f; // world units per second

// Example system: every entity with a Position and a Velocity moves. (Issue: movement system)
void movementSystem(Registry &registry, float dt)
{
    registry.each<Position, Velocity>([dt](Entity, Position &pos, Velocity &vel) {
        pos.x += vel.x * dt;
        pos.y += vel.y * dt;
    });
}

} // namespace

Game::Game()
{
    _players.fill(NullEntity);
    _registry.addSystem(movementSystem);
    // TODO(gameplay): register the other systems here (spawn, shooting, collisions, damage...).
}

Entity Game::spawnNetworked(protocol::EntityType type, std::uint8_t variant, float x, float y)
{
    const Entity entity = _registry.spawn();
    _registry.emplace<NetworkId>(entity, _nextNetId++);
    _registry.emplace<Kind>(entity, type, variant);
    _registry.emplace<Position>(entity, x, y);
    _registry.emplace<Velocity>(entity);
    return entity;
}

void Game::onPlayerJoined(PlayerId player)
{
    if (player >= _players.size() || _players[player] != NullEntity)
        return;
    const float y = WorldHeight / 5.f * static_cast<float>(player + 1);
    _players[player] = spawnNetworked(protocol::EntityType::Player, player, 100.f, y);
}

void Game::onPlayerLeft(PlayerId player)
{
    if (player >= _players.size() || _players[player] == NullEntity)
        return;
    const Entity entity = _players[player];
    if (const auto *netId = _registry.get<NetworkId>(entity))
        _events.emplace_back(protocol::EntityDestroyed{netId->id, protocol::DestroyReason::Killed});
    _registry.kill(entity);
    _players[player] = NullEntity;
}

void Game::onInput(PlayerId player, std::uint8_t flags)
{
    if (player >= _players.size() || _players[player] == NullEntity)
        return;
    auto *vel = _registry.get<Velocity>(_players[player]);
    if (!vel)
        return;
    using namespace protocol::InputFlag;
    // -1, 0 or +1 on each axis depending on which keys are held.
    const auto axis = [flags](std::uint8_t negative, std::uint8_t positive) {
        return static_cast<float>(((flags & positive) != 0) - ((flags & negative) != 0));
    };
    vel->x = axis(Left, Right) * PlayerSpeed;
    vel->y = axis(Up, Down) * PlayerSpeed;
    // TODO(gameplay): shooting (flags & Shoot), screen bounds.
}

void Game::update(float dt)
{
    _registry.update(dt);
    ++_tick;
}

protocol::Snapshot Game::snapshot()
{
    protocol::Snapshot snap;
    snap.tick = _tick;
    _registry.each<NetworkId, Kind, Position>([&](Entity, NetworkId &id, Kind &kind, Position &pos) {
        snap.entities.push_back({id.id, kind.type, kind.variant, pos.x, pos.y});
    });
    return snap;
}

std::vector<protocol::Message> Game::takeEvents()
{
    return std::exchange(_events, {});
}

} // namespace rtype::server
