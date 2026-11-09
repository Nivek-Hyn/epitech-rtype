#include "render/EntityRenderer.hpp"

#include "common/Constants.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>

namespace rtype::client {

namespace {

using protocol::EntityState;
using protocol::EntityType;

const std::array<sf::Color, 4> PlayerColors = {sf::Color(0, 230, 255), sf::Color(255, 170, 40), sf::Color(80, 255, 120),
                                               sf::Color(240, 110, 255)};

sf::Sprite makeSprite(const sf::Texture &texture, const sf::IntRect &rect, sf::Vector2f scale, sf::Vector2f position)
{
    sf::Sprite sprite(texture);
    sprite.setTextureRect(rect);
    sprite.setScale(scale);
    sprite.setPosition(position);
    return sprite;
}

// Sprite column of the ship: 2 is level, 3-4 climbing, 0-1 diving.
int shipColumn(float tilt)
{
    if (tilt > 1.1f)
        return 4;
    if (tilt > 0.25f)
        return 3;
    if (tilt < -1.1f)
        return 0;
    if (tilt < -0.25f)
        return 1;
    return 2;
}

void drawThruster(sf::RenderTarget &window, const EntityState &entity, float time, int column)
{
    const int flameCycle = static_cast<int>(time * 24.f) % 3;
    sf::CircleShape glow(flameCycle == 0 ? 3.5f : (flameCycle == 1 ? 5.2f : 4.2f));
    glow.setFillColor(flameCycle == 1 ? sf::Color(100, 220, 255, 230) : sf::Color(0, 160, 255, 175));
    float y = entity.y + 13.f;
    if (column == 4 || column == 3)
        y = entity.y + 15.f;
    else if (column == 0 || column == 1)
        y = entity.y + 11.f;
    glow.setPosition({entity.x - 5.f, y});
    window.draw(glow);
}

// Each enemy shot variant: texture rectangle, color, origin and scale, some animated.
void drawEnemyShot(sf::RenderTarget &window, const ResourceManager &resources, const EntityState &entity, float time)
{
    const sf::Texture &bullet = resources.getTexture("bullet");
    sf::Sprite sprite(bullet);
    switch (entity.variant) {
    case 0:
        sprite = makeSprite(resources.getTexture("enemy_bullet"),
                            sf::IntRect({(static_cast<int>(time * 12.f) % 4) * 17, 1}, {16, 14}), {2.5f, 2.5f}, {});
        sprite.setOrigin({8.f, 7.f});
        break;
    case 1: {
        const float pulse = 1.0f + 0.15f * std::sin(time * 20.f);
        sprite = makeSprite(bullet, sf::IntRect({232, 103}, {16, 12}), {2.6f * pulse, 2.2f}, {});
        sprite.setColor(sf::Color(255, 60, 60));
        sprite.setOrigin({8.f, 6.f});
        break;
    }
    case 2:
        sprite = makeSprite(bullet, sf::IntRect({101, 3}, {32, 14}), {2.0f, 1.8f + 0.35f * std::sin(time * 18.f)}, {});
        sprite.setColor(sf::Color(210, 90, 255));
        sprite.setOrigin({16.f, 7.f});
        break;
    case 3:
        sprite = makeSprite(bullet, sf::IntRect({205, 24}, {27, 18}), {2.0f, 2.0f}, {});
        sprite.setColor(sf::Color(80, 255, 80));
        sprite.setOrigin({13.f, 9.f});
        sprite.setRotation(sf::degrees(time * 360.f));
        break;
    default: {
        const float pulse = 1.0f + 0.12f * std::sin(time * 15.f);
        sprite = makeSprite(bullet, sf::IntRect({167, 22}, {32, 24}), {2.4f * pulse, 2.4f * pulse}, {});
        sprite.setColor(sf::Color(255, 130, 20));
        sprite.setOrigin({16.f, 12.f});
        break;
    }
    }
    sprite.setPosition({entity.x, entity.y});
    window.draw(sprite);
}

// Tiles of r-typesheet21: space debris, Martian rock, singularity shard.
void drawObstacleSprite(sf::RenderTarget &window, const sf::Texture &texture, const EntityState &entity, float time)
{
    sf::IntRect rect({1 + (static_cast<int>(time * 9.f) % 16) * 34, 363}, {32, 32});
    float size = 32.f;
    if (entity.variant == 1) {
        rect = sf::IntRect({28 + (static_cast<int>(time * 6.f) % 8) * 65, 140}, {62, 46});
        size = 52.f;
    } else if (entity.variant == 2) {
        rect = sf::IntRect({134 + (static_cast<int>(time * 8.f) % 8) * 34, 261}, {32, 32});
    }
    const float scale = ObstacleSize / size;
    sf::Sprite sprite = makeSprite(texture, rect, {scale, scale}, {entity.x, entity.y});
    if (entity.variant == 2)
        sprite.setColor(sf::Color(220, 150, 255)); // violet singularity glow
    window.draw(sprite);
}

// Without the obstacle texture, a colored square with an outline.
void drawObstacleTile(sf::RenderTarget &window, const EntityState &entity)
{
    static const std::array<sf::Color, 3> colors = {sf::Color(120, 135, 150), sf::Color(165, 85, 50),
                                                    sf::Color(120, 70, 190)};
    sf::RectangleShape tile({ObstacleSize - 4.f, ObstacleSize - 4.f});
    tile.setPosition({entity.x + 2.f, entity.y + 2.f});
    tile.setFillColor(colors[entity.variant % colors.size()]);
    tile.setOutlineColor(sf::Color(235, 235, 245));
    tile.setOutlineThickness(2.f);
    window.draw(tile);
}

// One animation frame of an enemy variant other than the scout (variant 1), which also has an engine glow.
struct EnemyLook {
    const char *texture;
    sf::IntRect rect;
    float scale;
    float offsetY = 0.f;
};

EnemyLook enemyLook(std::uint8_t variant, int frame)
{
    static const std::array<sf::IntRect, 4> probe = {sf::IntRect({1, 2}, {16, 32}), sf::IntRect({19, 2}, {15, 32}),
                                                     sf::IntRect({36, 2}, {15, 32}), sf::IntRect({19, 2}, {15, 32})};
    static const std::array<sf::IntRect, 4> hopper = {sf::IntRect({2, 1}, {47, 56}), sf::IntRect({52, 1}, {56, 56}),
                                                      sf::IntRect({116, 1}, {49, 56}), sf::IntRect({52, 1}, {56, 56})};
    static const std::array<sf::IntRect, 2> boss = {sf::IntRect({2, 2}, {176, 144}), sf::IntRect({191, 2}, {176, 144})};
    const int relay = (frame / 2) % 16;
    switch (variant) {
    case 0:
        return {"enemy_standard", sf::IntRect({(frame % 8) * 33, 0}, {33, 36}), 2.0f};
    case 2:
        return {"enemy_orbital_probe", probe[static_cast<std::size_t>((frame / 2) % 4)], 2.2f};
    case 3: // solar relay: 16-frame rotation over two rows
        return {"enemy_solar_relay", sf::IntRect({(relay % 8) * 33 + 1, relay < 8 ? 2 : 36}, {32, 32}), 2.2f};
    case 4:
        return {"enemy_interceptor", sf::IntRect({((frame / 2) % 5) * 65 + 9, 9}, {48, 48}), 1.8f};
    case 5: // heavy war cruiser
        return {"enemy_boss_heavy", sf::IntRect({0, 0}, {130, 50}), 2.2f, -20.f};
    case 7: // Martian hopper
        return {"enemy_tentacle", hopper[static_cast<std::size_t>((frame / 2) % 4)], 1.8f};
    default:
        return {"enemy_boss", boss[static_cast<std::size_t>((frame / 6) % 2)], 1.6f, -60.f};
    }
}

} // namespace

void EntityRenderer::draw(sf::RenderTarget &window, const EntityState &entity, float time, float tilt,
                          std::optional<std::uint8_t> localPlayer) const
{
    switch (entity.type) {
    case EntityType::Player:
        drawPlayer(window, entity, time, tilt, localPlayer);
        break;
    case EntityType::PlayerMissile:
        drawPlayerMissile(window, entity, time);
        break;
    case EntityType::EnemyMissile:
        drawEnemyMissile(window, entity, time);
        break;
    case EntityType::Bonus:
        drawBonus(window, entity, time);
        break;
    case EntityType::Obstacle:
        drawObstacle(window, entity, time);
        break;
    case EntityType::Enemy:
        drawEnemy(window, entity, time);
        break;
    }
}

void EntityRenderer::drawPlayer(sf::RenderTarget &window, const EntityState &entity, float time, float tilt,
                                std::optional<std::uint8_t> localPlayer) const
{
    const int column = shipColumn(tilt);
    const int row = static_cast<int>(entity.variant % 5);
    window.draw(makeSprite(_resources.getTexture("player"), sf::IntRect({column * 33, row * 17}, {33, 17}), {2.f, 2.f},
                           {entity.x, entity.y}));
    drawThruster(window, entity, time, column);
    drawPlayerTag(window, entity, localPlayer);
}

// "P1", "P2 (YOU)"... above the ship, in the player's color.
void EntityRenderer::drawPlayerTag(sf::RenderTarget &window, const EntityState &entity,
                                   std::optional<std::uint8_t> localPlayer) const
{
    std::string tag = "P" + std::to_string(entity.variant + 1);
    if (localPlayer == entity.variant)
        tag += " (YOU)";
    sf::Text text(_resources.getFont("default"), tag, 15);
    text.setFillColor(PlayerColors[entity.variant % 4]);
    const auto bounds = text.getLocalBounds();
    text.setOrigin({bounds.size.x / 2.f, bounds.size.y});
    text.setPosition({entity.x + 33.f, entity.y - 8.f});
    window.draw(text);
}

// Variant 1 = laser_missile (entities.json): longer and magenta.
void EntityRenderer::drawPlayerMissile(sf::RenderTarget &window, const EntityState &entity, float time) const
{
    const bool laser = entity.variant == 1;
    const sf::IntRect rect =
        static_cast<int>(time * 18.f) % 2 == 0 ? sf::IntRect({249, 102}, {16, 12}) : sf::IntRect({266, 102}, {16, 12});
    sf::Sprite sprite = makeSprite(_resources.getTexture("bullet"), rect,
                                   laser ? sf::Vector2f{3.5f, 1.5f} : sf::Vector2f{2.f, 2.f}, {entity.x, entity.y});
    sprite.setColor(laser ? sf::Color(255, 90, 220) : sf::Color(120, 230, 255));
    window.draw(sprite);
}

void EntityRenderer::drawEnemyMissile(sf::RenderTarget &window, const EntityState &entity, float time) const
{
    // Shots of the same wave do not pulse in sync.
    drawEnemyShot(window, _resources, entity, time + static_cast<float>(entity.netId % 7) * 0.15f);
}

// Colored orb with a letter, so bonuses can be told apart without color: S spread, L laser, + life.
void EntityRenderer::drawBonus(sf::RenderTarget &window, const EntityState &entity, float time) const
{
    static const std::array<std::pair<sf::Color, const char *>, 3> looks = {
        {{sf::Color(80, 200, 255), "S"}, {sf::Color(255, 90, 220), "L"}, {sf::Color(90, 230, 120), "+"}}};
    const auto &[color, letter] = looks[entity.variant % looks.size()];
    const sf::Vector2f center{entity.x + 16.f, entity.y + 16.f};
    const float radius = 16.f * (1.f + 0.1f * std::sin(time * 8.f));

    sf::CircleShape orb(radius);
    orb.setOrigin({radius, radius});
    orb.setPosition(center);
    orb.setFillColor(color);
    orb.setOutlineColor(sf::Color::White);
    orb.setOutlineThickness(2.f);
    window.draw(orb);

    sf::Text label(_resources.getFont("default"), letter, 20);
    label.setFillColor(sf::Color::Black);
    const auto bounds = label.getLocalBounds();
    label.setOrigin({bounds.position.x + bounds.size.x / 2.f, bounds.position.y + bounds.size.y / 2.f});
    label.setPosition(center);
    window.draw(label);
}

void EntityRenderer::drawObstacle(sf::RenderTarget &window, const EntityState &entity, float time) const
{
    const float localTime = time + static_cast<float>(entity.netId % 13) * 0.31f;
    if (_resources.hasTexture("obstacles"))
        drawObstacleSprite(window, _resources.getTexture("obstacles"), entity, localTime);
    else
        drawObstacleTile(window, entity);
}

void EntityRenderer::drawEnemy(sf::RenderTarget &window, const EntityState &entity, float time) const
{
    const float localTime = time + static_cast<float>(entity.netId % 11) * 0.23f;
    if (entity.variant == 1) {
        drawScout(window, entity, localTime);
        return;
    }
    const EnemyLook look = enemyLook(entity.variant, static_cast<int>(localTime * 9.f));
    const sf::Texture &image = entity.variant == 3 ? texture({look.texture, "enemy_diver"}) : texture({look.texture});
    window.draw(makeSprite(image, look.rect, {look.scale, look.scale}, {entity.x, entity.y + look.offsetY}));
}

// Scout ship facing the players, with its engine glow behind it.
void EntityRenderer::drawScout(sf::RenderTarget &window, const EntityState &entity, float time) const
{
    window.draw(
        makeSprite(texture({"enemy_patrol"}), sf::IntRect({1, 2}, {32, 28}), {2.0f, 2.0f}, {entity.x, entity.y}));
    const float pulse = 1.0f + 0.2f * std::sin(time * 18.f);
    sf::CircleShape glow(3.5f * pulse);
    glow.setOrigin({3.5f, 3.5f});
    glow.setFillColor(sf::Color(255, 130, 40, 210));
    glow.setPosition({entity.x + 58.f, entity.y + 26.f});
    window.draw(glow);
}

void EntityRenderer::drawExplosion(sf::RenderTarget &window, const Explosion &explosion) const
{
    static const std::array<sf::IntRect, 6> frames = {
        sf::IntRect({72, 296}, {32, 24}),  sf::IntRect({109, 296}, {32, 24}), sf::IntRect({146, 296}, {32, 24}),
        sf::IntRect({354, 296}, {32, 24}), sf::IntRect({391, 296}, {32, 24}), sf::IntRect({428, 296}, {32, 24})};
    const float progress = explosion.timer / explosion.maxDuration;
    const std::size_t frame = std::clamp(static_cast<std::size_t>(progress * 6.f), std::size_t{0}, std::size_t{5});
    sf::Sprite sprite = makeSprite(_resources.getTexture("bullet"), frames[frame], {2.8f, 2.8f},
                                   {explosion.position.x + 25.f, explosion.position.y + 20.f});
    sprite.setOrigin({16.f, 12.f});
    window.draw(sprite);
}

const sf::Texture &EntityRenderer::texture(std::initializer_list<const char *> names) const
{
    for (const char *name : names) {
        if (_resources.hasTexture(name))
            return _resources.getTexture(name);
    }
    return _resources.getTexture("enemy");
}

} // namespace rtype::client
