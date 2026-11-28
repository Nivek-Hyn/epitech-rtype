#pragma once

#include "ResourceManager.hpp"
#include "common/Protocol.hpp"
#include "render/Neon.hpp"

#include <SFML/Graphics.hpp>

#include <cstdint>
#include <optional>

namespace rtype::client {

// A short flash of light where an entity was destroyed.
struct Explosion {
    sf::Vector2f position;
    float timer{0.f};
    float maxDuration{0.35f};
};

// Draws what the server sends: one small function per entity type and variant, all drawn in code from black shapes
// and lines of light (see render/Neon.hpp). The variant comes from the Kind of the entity's prefab in
// assets/config/entities.json; render/Looks.hpp names the enemy variants.
class EntityRenderer {
public:
    explicit EntityRenderer(ResourceManager &resources) : _resources(resources) {}

    // tilt is only used for ships: from -2 (diving) to 2 (climbing). glow: the glow layer of the Neon styles, or
    // nullptr. flash: from 0 to 1, how white the entity is drawn (an enemy just hit). damage: from 0 to 1, how worn
    // a boss looks (cracks from half its health, then sparks and smoke).
    void draw(sf::RenderTarget &window, const protocol::EntityState &entity, float time, float tilt,
              std::optional<std::uint8_t> localPlayer, sf::RenderTarget *glow = nullptr, float flash = 0.f,
              float damage = 0.f) const;
    void drawExplosion(sf::RenderTarget &window, const Explosion &explosion, sf::RenderTarget *glow = nullptr) const;

    // Where the engine of a ship is, for its exhaust trail.
    static sf::Vector2f enginePosition(const protocol::EntityState &ship);

private:
    void drawPlayerTag(sf::RenderTarget &window, const protocol::EntityState &entity,
                       std::optional<std::uint8_t> localPlayer) const;

    ResourceManager &_resources;
};

} // namespace rtype::client
