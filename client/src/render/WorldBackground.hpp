#pragma once

#include <SFML/Graphics.hpp>

#include <cstddef>
#include <vector>

namespace rtype::client {

// The background of a world, drawn by code: a sky gradient, two layers of stars scrolling at different speeds
// (parallax) and the scenery of the world. World 0 is the orbital belt (a gas giant and its rings), world 1 is Mars
// (dunes and a pale sun), world 2 is the singularity (a black hole and its accretion disk).
class WorldBackground {
public:
    static constexpr std::size_t WorldCount = 3;

    WorldBackground();

    // The world of a level: three levels per world, the infinite mode loops over them.
    void setWorld(std::size_t world) { _world = world % WorldCount; }
    std::size_t world() const { return _world; }
    void update(float dt);
    void draw(sf::RenderTarget &target) const;

private:
    struct Star {
        sf::Vector2f position;
        float size;
        float brightness;
    };

    void drawSky(sf::RenderTarget &target) const;
    void drawStars(sf::RenderTarget &target, const std::vector<Star> &stars, float speed) const;
    void drawOrbitalBelt(sf::RenderTarget &target) const;
    void drawMars(sf::RenderTarget &target) const;
    void drawDunes(sf::RenderTarget &target, float baseY, float height, float speed, sf::Color color) const;
    void drawSingularity(sf::RenderTarget &target) const;

    std::size_t _world = 0;
    float _time = 0.f;
    std::vector<Star> _farStars;
    std::vector<Star> _nearStars;
};

} // namespace rtype::client
