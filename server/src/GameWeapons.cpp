// Player weapons and bonuses: weapons.json says which prefabs to use, entities.json describes them.

#include "Game.hpp"
#include "common/Components.hpp"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <random>

namespace rtype::server {

namespace {

constexpr int MaxLives = 5;

// Turns the velocity of the prefab by the given angle, keeping its speed.
void aim(rtype::components::Velocity &velocity, float degrees)
{
    const float speed = std::hypot(velocity.x, velocity.y);
    const float radians = degrees * std::numbers::pi_v<float> / 180.f;
    velocity = {speed * std::cos(radians), speed * std::sin(radians)};
}

} // namespace

// One missile per angle of the player's current weapon.
void Game::fire(std::size_t player, float x, float y)
{
    const WeaponDefinition &weapon = _arsenal.weapon(_weapons[player]);
    for (const float angle : weapon.angles) {
        if (const auto missile = spawnPrefab(weapon.missile, x, y))
            aim(*_registry.get<rtype::components::Velocity>(*missile), angle);
    }
    _shootCooldowns[player] = weapon.cooldown;
}

// Called once the collisions are resolved: creating entities while iterating is not allowed.
void Game::dropBonuses(const std::vector<std::pair<float, float>> &positions)
{
    if (_arsenal.dropPrefabs.empty())
        return;
    std::bernoulli_distribution drops(std::clamp(_arsenal.dropChance, 0.f, 1.f));
    std::uniform_int_distribution<std::size_t> pick(0, _arsenal.dropPrefabs.size() - 1);
    for (const auto &[x, y] : positions) {
        if (drops(_random))
            spawnPrefab(_arsenal.dropPrefabs[pick(_random)], x, y);
    }
}

// The player whose ship touches the box, if any.
std::optional<std::size_t> Game::playerTouching(const engine::physics::Aabb &box)
{
    for (std::size_t player = 0; player < _players.size(); player++) {
        if (_players[player] != engine::ecs::NullEntity && engine::physics::overlaps(box, hitArea(_players[player])))
            return player;
    }
    return std::nullopt;
}

// A player touching a bonus gets its effect; the bonus disappears.
void Game::collectBonuses()
{
    std::vector<std::pair<engine::ecs::Entity, std::size_t>> collected;
    _registry.each<rtype::components::BonusEffect>(
        [this, &collected](engine::ecs::Entity bonus, const rtype::components::BonusEffect &) {
            if (const auto player = playerTouching(hitArea(bonus)))
                collected.emplace_back(bonus, *player);
        });
    for (const auto &[bonus, player] : collected)
        giveBonus(bonus, player);
}

void Game::giveBonus(engine::ecs::Entity bonus, std::size_t player)
{
    const auto &effect = *_registry.get<rtype::components::BonusEffect>(bonus);
    if (!effect.weapon.empty())
        _weapons[player] = effect.weapon;
    _lives[player] = std::min(MaxLives, _lives[player] + effect.lives);
    if (const auto *networkId = _registry.get<rtype::components::NetworkId>(bonus))
        _events.push_back(protocol::EntityDestroyed{networkId->id, protocol::DestroyReason::Collected});
    _registry.kill(bonus);
}

} // namespace rtype::server
