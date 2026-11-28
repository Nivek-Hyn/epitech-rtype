#pragma once

#include <SFML/Graphics.hpp>

#include <cstddef>
#include <cstdint>

namespace rtype::client {

namespace neon {
class Canvas;
}

// The background of each world, drawn in code like everything else (see render/Neon.hpp): a sky of stars at three
// depths, the landmark of the world far away, and the walls of the level scrolling at the speed of the game.
// Everything is computed from the scrolled distance, so it never repeats visibly and has no seam. Changing world
// fades from one to the other.
class WorldBackground {
public:
    static constexpr std::size_t WorldCount = 3;

    // The world of a level: three levels per world, the infinite mode loops over them.
    void setWorld(std::size_t world);
    std::size_t world() const { return _world; }
    void update(float dt);
    // glow: the glow layer of the Neon styles, or nullptr.
    void draw(sf::RenderTarget &target, sf::RenderTarget *glow = nullptr) const;

private:
    void drawWorld(neon::Canvas &canvas, std::size_t world, float alpha) const;
    void drawStars(neon::Canvas &canvas, float alpha, float twinkle) const;
    void drawOrbitalBelt(neon::Canvas &canvas, float alpha) const;
    void drawMars(neon::Canvas &canvas, float alpha) const;
    void drawSingularity(neon::Canvas &canvas, float alpha) const;

    std::size_t _world = 0;
    std::size_t _previous = 0; // the world fading out
    float _fade = 1.f;         // from 0 (just changed) to 1 (only the current world)
    float _time = 0.f;         // also the scrolled distance: each layer moves at time * its speed
};

} // namespace rtype::client
