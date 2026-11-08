#pragma once

#include "ResourceManager.hpp"
#include "common/Protocol.hpp"

#include <SFML/Graphics.hpp>

#include <cstdint>
#include <initializer_list>
#include <optional>

namespace rtype::client {

// A short explosion animation where an entity was destroyed.
struct Explosion {
    sf::Vector2f position;
    float timer{0.f};
    float maxDuration{0.35f};
};

// Draws what the server sends: one small function per entity type and variant.
// The variant comes from the Kind of the entity's prefab in assets/config/entities.json.
class EntityRenderer {
public:
    explicit EntityRenderer(ResourceManager &resources) : _resources(resources) {}

    // tilt is only used for ships: from -2 (diving) to 2 (climbing).
    void draw(sf::RenderWindow &window, const protocol::EntityState &entity, float time, float tilt,
              std::optional<std::uint8_t> localPlayer) const;
    void drawExplosion(sf::RenderWindow &window, const Explosion &explosion) const;

private:
    void drawPlayer(sf::RenderWindow &window, const protocol::EntityState &entity, float time, float tilt,
                    std::optional<std::uint8_t> localPlayer) const;
    void drawPlayerTag(sf::RenderWindow &window, const protocol::EntityState &entity,
                       std::optional<std::uint8_t> localPlayer) const;
    void drawPlayerMissile(sf::RenderWindow &window, const protocol::EntityState &entity, float time) const;
    void drawEnemyMissile(sf::RenderWindow &window, const protocol::EntityState &entity, float time) const;
    void drawBonus(sf::RenderWindow &window, const protocol::EntityState &entity, float time) const;
    void drawObstacle(sf::RenderWindow &window, const protocol::EntityState &entity, float time) const;
    void drawEnemy(sf::RenderWindow &window, const protocol::EntityState &entity, float time) const;
    void drawScout(sf::RenderWindow &window, const protocol::EntityState &entity, float time) const;

    // The first loaded texture among the names, or the generic enemy texture.
    const sf::Texture &texture(std::initializer_list<const char *> names) const;

    ResourceManager &_resources;
};

} // namespace rtype::client
