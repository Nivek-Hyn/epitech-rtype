#include "render/WorldBackground.hpp"

#include "common/Constants.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <map>

namespace rtype::client {

namespace {

using Json = nlohmann::json;

// Each image is loaded once, even if several layers use it, and repeats so that it can scroll forever.
std::shared_ptr<sf::Texture> loadTexture(const std::string &path,
                                         std::map<std::string, std::shared_ptr<sf::Texture>> &cache)
{
    if (const auto found = cache.find(path); found != cache.end())
        return found->second;
    auto texture = std::make_shared<sf::Texture>();
    if (!texture->loadFromFile(path)) {
        std::cerr << "Warning: background image " << path << " not found\n";
        texture.reset();
    } else {
        texture->setSmooth(true);
        texture->setRepeated(true);
    }
    cache[path] = texture;
    return texture;
}

} // namespace

WorldBackground::WorldBackground(const std::string &configPath)
{
    std::ifstream file(configPath);
    const Json json = file ? Json::parse(file, nullptr, false) : Json();
    if (!json.is_object()) {
        std::cerr << "Warning: no backgrounds in " << configPath << '\n';
        return;
    }
    std::map<std::string, std::shared_ptr<sf::Texture>> cache;
    for (const Json &world : json.value("worlds", Json::array())) {
        std::vector<Layer> layers;
        for (const Json &layer : world.value("layers", Json::array())) {
            auto texture = loadTexture(layer.value("image", ""), cache);
            if (texture)
                layers.push_back({texture,
                                  layer.value("fill", false),
                                  layer.value("repeat_x", false),
                                  {layer.value("x", 0.f), layer.value("y", 0.f)},
                                  layer.value("scale", 1.f),
                                  layer.value("speed", 0.f)});
        }
        _worlds.push_back(std::move(layers));
    }
}

namespace {

constexpr float FadeSeconds = 1.5f;

} // namespace

void WorldBackground::setWorld(std::size_t world)
{
    if (_worlds.empty() || world % _worlds.size() == _world)
        return;
    _previous = _world;
    _world = world % _worlds.size();
    _fade = 0.f;
}

void WorldBackground::update(float dt)
{
    _time += dt;
    _fade = std::min(1.f, _fade + dt / FadeSeconds);
}

// While a world change fades, the new world is drawn over the old one, more and more opaque.
void WorldBackground::draw(sf::RenderTarget &target) const
{
    if (_worlds.empty())
        return;
    if (_fade < 1.f)
        drawWorld(target, _previous, 255);
    drawWorld(target, _world, static_cast<std::uint8_t>(255.f * _fade));
}

void WorldBackground::drawWorld(sf::RenderTarget &target, std::size_t world, std::uint8_t alpha) const
{
    for (const Layer &layer : _worlds[world]) {
        if (layer.fill)
            drawFill(target, layer, alpha);
        else if (layer.repeatX)
            drawRepeatX(target, layer, alpha);
        else
            drawProp(target, layer, alpha);
    }
}

// Scaled to cover the screen; the texture repeats, so scrolling only moves the part shown.
void WorldBackground::drawFill(sf::RenderTarget &target, const Layer &layer, std::uint8_t alpha) const
{
    const sf::Vector2f size(layer.texture->getSize());
    const float scale = std::max(WorldWidth / size.x, WorldHeight / size.y);
    const float rawOffset = std::fmod(_time * layer.speed / scale, size.x);
    const int offset = static_cast<int>(rawOffset < 0.f ? rawOffset + size.x : rawOffset);
    sf::Sprite sprite(*layer.texture);
    sprite.setTextureRect(
        sf::IntRect({offset, 0}, {static_cast<int>(std::ceil(WorldWidth / scale)), static_cast<int>(size.y)}));
    sprite.setScale({scale, scale});
    sprite.setColor(sf::Color(255, 255, 255, alpha));
    target.draw(sprite);
}

// Repeats horizontally across the screen at layer.position.y (ground, ceiling, mountain ranges).
void WorldBackground::drawRepeatX(sf::RenderTarget &target, const Layer &layer, std::uint8_t alpha) const
{
    const sf::Vector2f size(layer.texture->getSize());
    const float scale = layer.scale > 0.f ? layer.scale : 1.f;
    const float scaledWidth = size.x * scale;
    const float scaledOffset = std::fmod(_time * layer.speed - layer.position.x, scaledWidth);
    const float effectiveOffset = scaledOffset < 0.f ? scaledOffset + scaledWidth : scaledOffset;
    const int textureOffset = static_cast<int>(effectiveOffset / scale);

    sf::Sprite sprite(*layer.texture);
    sprite.setTextureRect(
        sf::IntRect({textureOffset, 0}, {static_cast<int>(std::ceil(WorldWidth / scale)) + 2, static_cast<int>(size.y)}));
    sprite.setScale({scale, scale});
    sprite.setPosition({0.f, layer.position.y});
    sprite.setColor(sf::Color(255, 255, 255, alpha));
    target.draw(sprite);
}

// Moves left from its position and comes back on the right once it has left the screen.
void WorldBackground::drawProp(sf::RenderTarget &target, const Layer &layer, std::uint8_t alpha) const
{
    const float width = static_cast<float>(layer.texture->getSize().x) * layer.scale;
    const float span = WorldWidth + width;
    const float x = std::fmod(layer.position.x + width - _time * layer.speed, span);
    sf::Sprite sprite(*layer.texture);
    sprite.setScale({layer.scale, layer.scale});
    sprite.setPosition({(x < 0.f ? x + span : x) - width, layer.position.y});
    sprite.setColor(sf::Color(255, 255, 255, alpha));
    target.draw(sprite);
}

} // namespace rtype::client
