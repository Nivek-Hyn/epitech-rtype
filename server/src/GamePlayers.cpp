// Players: connection, controls, lives and respawn.

#include "Game.hpp"
#include "common/Components.hpp"
#include <algorithm>

namespace rtype::server {

namespace {

constexpr float PlayerSpawnX = 100.f;
constexpr float PlayerSpawnY = 300.f;
constexpr float PlayerMissileOffsetX = 30.f;

} // namespace

void Game::onPlayerJoined(PlayerId player)
{
    if (player >= MaxPlayers || _connected[player])
        return;
    _connected[player] = true;
    _lives[player] = _rules.playerLives;
    _respawnTimers[player] = 0.f;
    _invulnerableTimers[player] = 0.f;
    if (_state != protocol::GameState::Playing)
        return; // the game has ended: no ship until it restarts
    spawnPlayerShip(player);
}

void Game::onPlayerLeft(PlayerId player)
{
    if (player >= MaxPlayers)
        return;
    _connected[player] = false;
    _lives[player] = 0;
    // A finished game restarts from the beginning for the next players.
    if (!anyPlayerConnected() && _state != protocol::GameState::Playing)
        restartCampaign();
    removePlayerShip(player);
}

void Game::onInput(PlayerId player, std::uint8_t flags)
{
    if (player >= MaxPlayers || _players[player] == engine::ecs::NullEntity)
        return;
    applyMovementInput(player, flags);
    if (flags & protocol::InputFlag::Shoot)
        shootIfReady(player);
}

void Game::applyMovementInput(std::size_t player, std::uint8_t flags)
{
    const auto *control = _registry.get<rtype::components::PlayerControl>(_players[player]);
    const float speed = control ? control->speed : rtype::components::PlayerControl{}.speed;
    float vx = 0.0f;
    float vy = 0.0f;
    if (flags & protocol::InputFlag::Up)
        vy -= speed;
    if (flags & protocol::InputFlag::Down)
        vy += speed;
    if (flags & protocol::InputFlag::Left)
        vx -= speed;
    if (flags & protocol::InputFlag::Right)
        vx += speed;
    _registry.emplace<rtype::components::Velocity>(_players[player], vx, vy);
}

void Game::shootIfReady(std::size_t player)
{
    if (_shootCooldowns[player] > 0.f)
        return;
    const auto *position = _registry.get<rtype::components::Position>(_players[player]);
    if (!position)
        return;
    fire(player, position->x + PlayerMissileOffsetX, position->y);
}

void Game::spawnPlayerShip(std::size_t player)
{
    const auto variant = static_cast<std::uint8_t>(player);
    const auto ship = spawnPrefab("player_ship", PlayerSpawnX, PlayerSpawnY);
    _players[player] = ship ? *ship : spawnNetworked(protocol::EntityType::Player, variant, PlayerSpawnX, PlayerSpawnY);
    _registry.get<rtype::components::Kind>(_players[player])->variant = variant; // the player's color
}

void Game::removePlayerShip(std::size_t player)
{
    if (_players[player] == engine::ecs::NullEntity)
        return;
    const engine::ecs::Entity ship = _players[player];
    if (const auto *networkId = _registry.get<rtype::components::NetworkId>(ship))
        _events.push_back(protocol::EntityDestroyed{networkId->id, protocol::DestroyReason::Killed});
    _registry.kill(ship);
    _players[player] = engine::ecs::NullEntity;
    _shootCooldowns[player] = 0.f;
}

void Game::updatePlayerLives(float dt)
{
    if (_state != protocol::GameState::Playing)
        return;
    for (std::size_t player = 0; player < _players.size(); player++)
        updatePlayerRespawn(player, dt);
}

void Game::updatePlayerRespawn(std::size_t player, float dt)
{
    _invulnerableTimers[player] = std::max(0.f, _invulnerableTimers[player] - dt);
    if (!_connected[player] || _players[player] != engine::ecs::NullEntity || _lives[player] <= 0)
        return;
    _respawnTimers[player] -= dt;
    if (_respawnTimers[player] <= 0.f) {
        spawnPlayerShip(player);
        _invulnerableTimers[player] = _rules.invulnerability;
    }
}

bool Game::isInvulnerable(engine::ecs::Entity entity) const
{
    for (std::size_t player = 0; player < _players.size(); player++) {
        if (_players[player] == entity)
            return _invulnerableTimers[player] > 0.f;
    }
    return false;
}

bool Game::anyPlayerConnected() const
{
    return std::find(_connected.begin(), _connected.end(), true) != _connected.end();
}

bool Game::anyPlayerStillInGame() const
{
    for (std::size_t player = 0; player < _players.size(); player++) {
        if (_connected[player] && (_players[player] != engine::ecs::NullEntity || _lives[player] > 0))
            return true;
    }
    return false;
}

void Game::restoreLivesOnNewWorld()
{
    if (_state != protocol::GameState::Playing || _currentLevelIndex % _rules.levelsPerWorld != 0)
        return;
    // Back to the full count, not added: a player out of lives comes back after the respawn delay.
    for (std::size_t player = 0; player < _players.size(); player++) {
        if (_connected[player])
            _lives[player] = _rules.playerLives;
    }
}

} // namespace rtype::server
