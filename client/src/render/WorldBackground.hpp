#pragma once

#include <SFML/Graphics.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace rtype::client {

// The background of each world: layers of images listed in assets/config/backgrounds.json, each scrolling at its
// own speed (parallax). A "fill" layer covers the screen and repeats horizontally; another layer is placed at its
// position and comes back on the right once it has left the screen. Changing world fades from one to the other.
class WorldBackground {
public:
    explicit WorldBackground(const std::string &configPath = "assets/config/backgrounds.json");

    // The world of a level: three levels per world, the infinite mode loops over them.
    void setWorld(std::size_t world);
    std::size_t world() const { return _world; }
    void update(float dt);
    void draw(sf::RenderTarget &target) const;

private:
    struct Layer {
        std::shared_ptr<sf::Texture> texture;
        bool fill = false;
        sf::Vector2f position;
        float scale = 1.f;
        float speed = 0.f; // world units per second, to the left
    };

    void drawWorld(sf::RenderTarget &target, std::size_t world, std::uint8_t alpha) const;
    void drawFill(sf::RenderTarget &target, const Layer &layer, std::uint8_t alpha) const;
    void drawProp(sf::RenderTarget &target, const Layer &layer, std::uint8_t alpha) const;

    std::vector<std::vector<Layer>> _worlds;
    std::size_t _world = 0;
    std::size_t _previous = 0; // the world fading out
    float _fade = 1.f;         // from 0 (just changed) to 1 (only the current world)
    float _time = 0.f;
};

} // namespace rtype::client
