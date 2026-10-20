#include "Game.hpp"
#include "common/Components.hpp"
#include "engine/systems/Movement.hpp"

#include <algorithm>
#include <array>

namespace rtype::server {

namespace {

constexpr float EnemySpawnIntervalSeconds = 2.f;
constexpr float EnemySpeed = 100.f; // world units per second, towards the left
// Hitbox sizes match the client sprites (scaled x2): enemy 33x36.
constexpr float EnemyWidth = 66.f;
constexpr float EnemyHeight = 72.f;
// Rows where enemies appear, in turn. The first one is level with a new player's ship (y = 300).
constexpr std::array<float, 4> EnemyRows = {290.f, 150.f, 600.f, 450.f};

} // namespace

Game::Game()
{
    _players.fill(engine::ecs::NullEntity);
    _registry.addSystem(engine::systems::movement<rtype::components::Position, rtype::components::Velocity>);
}

// TODO(gameplay): spawn the player's ship (NetworkId, Kind, Position, Velocity...) and remember it in _players.
void Game::onPlayerJoined(PlayerId player)
{
    if (player >= MaxPlayers || _players[player] != engine::ecs::NullEntity)
        return;
    // we create a new entity in the ECS registry
    engine::ecs::Entity entity =
        spawnNetworked(protocol::EntityType::Player, static_cast<std::uint8_t>(player), 100.0f, 300.0f);
    //_registry.add_component<rtype::components::Position>(entity, {100.0f, 300.0f});
    //_registry.add_component<rtype::components::Velocity>(entity, {0.0f, 0.0f});
    //_registry.add_component<rtype::components::Kind>(entity, {rtype::components::EntityKind::Player});
    _players[player] = entity;
}

engine::ecs::Entity Game::spawnNetworked(protocol::EntityType type, std::uint8_t variant, float x, float y)
{
    engine::ecs::Entity entity = _registry.spawn();
    _registry.emplace<rtype::components::NetworkId>(entity, _nextNetId++);
    _registry.emplace<rtype::components::Kind>(entity, type, variant);
    _registry.emplace<rtype::components::Position>(entity, x, y);
    _registry.emplace<rtype::components::Velocity>(entity, 0.0f, 0.0f);
    return entity;
}

void Game::onPlayerLeft(PlayerId player)
{
    if (player >= MaxPlayers || _players[player] == engine::ecs::NullEntity)
        return;
    const engine::ecs::Entity entity = _players[player];
    const auto *networkId = _registry.get<rtype::components::NetworkId>(entity);
    if (networkId)
        _events.push_back(protocol::EntityDestroyed{networkId->id, protocol::DestroyReason::Killed});
    _registry.kill(entity);
    _players[player] = engine::ecs::NullEntity;
    _shootCooldowns[player] = 0.f;
}

// TODO(gameplay): turn the input flags into the ship's velocity, handle shooting.
void Game::onInput(PlayerId player, std::uint8_t flags)
{
    if (player >= MaxPlayers)
        return;
    // we get the entity associated with the player
    engine::ecs::Entity entity = _players[player];
    if (entity == engine::ecs::NullEntity)
        return;
    float speed = 200.0f;
    float vx = 0.0f;
    float vy = 0.0f;
    // we check the input flags and set the velocity accordingly
    if (flags & protocol::InputFlag::Up)
        vy -= speed;
    if (flags & protocol::InputFlag::Down)
        vy += speed;
    if (flags & protocol::InputFlag::Left)
        vx -= speed;
    if (flags & protocol::InputFlag::Right)
        vx += speed;
    // on met a jour (ou ajout) la vitesse du joueur
    _registry.emplace<rtype::components::Velocity>(entity, vx, vy);

    if ((flags & protocol::InputFlag::Shoot) && _shootCooldowns[player] <= 0.f) {
        auto *pos = _registry.get<rtype::components::Position>(entity);
        if (pos) {
            engine::ecs::Entity bullet = spawnNetworked(protocol::EntityType::PlayerMissile, 0, pos->x + 30.0f, pos->y);
            _registry.emplace<rtype::components::Velocity>(bullet, 400.0f, 0.0f);
            _shootCooldowns[player] = 0.25f;
        }
    }
}

void Game::update(float dt)
{
    spawnEnemies(dt);
    _registry.update(dt);
    for (float &cooldown : _shootCooldowns)
        cooldown = std::max(0.f, cooldown - dt);

    std::vector<std::pair<engine::ecs::Entity, std::uint32_t>> expiredMissiles;
    _registry.each<rtype::components::Position, rtype::components::Kind, rtype::components::NetworkId>(
        [&expiredMissiles](engine::ecs::Entity entity, const rtype::components::Position &position,
                           const rtype::components::Kind &kind, const rtype::components::NetworkId &networkId) {
            const bool missileOut =
                (kind.type == protocol::EntityType::PlayerMissile || kind.type == protocol::EntityType::EnemyMissile) &&
                (position.x > WorldWidth || position.x < 0.f || position.y > WorldHeight || position.y < 0.f);
            const bool enemyOut = kind.type == protocol::EntityType::Enemy && position.x < -EnemyWidth;
            if (missileOut || enemyOut)
                expiredMissiles.emplace_back(entity, networkId.id);
        });
    for (const auto &[entity, networkId] : expiredMissiles) {
        _registry.kill(entity);
        _events.push_back(protocol::EntityDestroyed{networkId, protocol::DestroyReason::OutOfBounds});
    }
    _tick++;
}

// TODO(gameplay): one EntityState per entity having NetworkId + Kind + Position.
protocol::Snapshot Game::snapshot()
{
    protocol::Snapshot snap;
    snap.tick = _tick;
    snap.state = protocol::GameState::Playing;
    snap.score = 0;

    _registry.each<rtype::components::Position, rtype::components::Kind, rtype::components::NetworkId>(
        [&snap](engine::ecs::Entity /*entity*/, const rtype::components::Position &pos,
                const rtype::components::Kind &kind, const rtype::components::NetworkId &netId) {
            protocol::EntityState state;
            state.netId = netId.id;
            state.type = kind.type;
            state.variant = kind.variant;
            state.x = pos.x;
            state.y = pos.y;
            snap.entities.push_back(state);
        });
    return snap;
}

std::vector<protocol::Message> Game::takeEvents()
{
    std::vector<protocol::Message> events;
    events.swap(_events);
    return events;
}

// Spawns an enemy on the right edge every EnemySpawnIntervalSeconds while at least one player is in the game.
void Game::spawnEnemies(float dt)
{
    const bool anyPlayer = std::any_of(_players.begin(), _players.end(),
                                       [](engine::ecs::Entity player) { return player != engine::ecs::NullEntity; });
    if (!anyPlayer) {
        _enemySpawnTimer = 0.f;
        return;
    }
    _enemySpawnTimer += dt;
    while (_enemySpawnTimer >= EnemySpawnIntervalSeconds) {
        _enemySpawnTimer -= EnemySpawnIntervalSeconds;
        const float y = EnemyRows[_enemiesSpawned % EnemyRows.size()];
        const engine::ecs::Entity enemy = spawnNetworked(protocol::EntityType::Enemy, 0, WorldWidth - EnemyWidth, y);
        _registry.emplace<rtype::components::Velocity>(enemy, -EnemySpeed, 0.f);
        _enemiesSpawned++;
    }
}

void checkCollisions(engine::ecs::Registry &registry, std::vector<protocol::Message> &events)
{
    registry.each<rtype::components::Position, rtype::components::Kind, rtype::components::NetworkId>(
        [&registry, &events](engine::ecs::Entity missileEntity, const rtype::components::Position &missilePos,
                             const rtype::components::Kind &missileKind,
                             const rtype::components::NetworkId &missileNetId) {
            if (missileKind.type != protocol::EntityType::PlayerMissile)
                return;
            registry.each<rtype::components::Position, rtype::components::Kind, rtype::components::NetworkId>(
                [&registry, &events, missileEntity, &missilePos, &missileNetId](
                    engine::ecs::Entity enemyEntity, const rtype::components::Position &enemyPos,
                    const rtype::components::Kind &enemyKind, const rtype::components::NetworkId &enemyNetId) {
                    if (enemyKind.type != protocol::EntityType::Enemy)
                        return;

                    // Définition simple d'une boîte de collision (AABB) d'une taille de 20x20 pixels par exemple
                    float boxSize = 20.0f;
                    bool collision = (missilePos.x < enemyPos.x + boxSize && missilePos.x + boxSize > enemyPos.x &&
                                      missilePos.y < enemyPos.y + boxSize && missilePos.y + boxSize > enemyPos.y);

                    if (collision) {
                        // 1. On détruit le missile
                        registry.kill(missileEntity);
                        protocol::EntityDestroyed missileDestroyed;
                        missileDestroyed.netId = missileNetId.id;
                        missileDestroyed.reason = protocol::DestroyReason::Killed;
                        events.push_back(missileDestroyed);

                        // 2. On détruit l'ennemi
                        registry.kill(enemyEntity);
                        protocol::EntityDestroyed enemyDestroyed;
                        enemyDestroyed.netId = enemyNetId.id;
                        enemyDestroyed.reason = protocol::DestroyReason::Killed;
                        events.push_back(enemyDestroyed);
                    }
                });
        });
}

} // namespace rtype::server