#pragma once

#include <SFML/Graphics.hpp>

#include <cstdint>
#include <initializer_list>
#include <span>
#include <vector>

namespace rtype::client::neon {

// Every picture of the game is drawn from two things: black shapes, which hide what is behind them, and lines of
// light. The lines are also drawn on the glow layer (when there is one), so the Neon and Arcade styles make them
// shine; in Classic they are simply sharp lines.
//
// Shapes are given in a local frame (for example a ship pointing right, centred on 0,0) and placed with a
// transform. Calls are batched and sent to the graphics card by flush() (or by the destructor).
class Canvas {
public:
    // glowAmount: how much of each line is copied to the glow layer, from 0 (none) to 1.
    Canvas(sf::RenderTarget &target, sf::RenderTarget *glow, float glowAmount = 0.6f);
    ~Canvas();
    Canvas(const Canvas &) = delete;
    Canvas &operator=(const Canvas &) = delete;

    void setTransform(const sf::Transform &transform) { _transform = transform; }
    const sf::Transform &transform() const { return _transform; }
    // Scales every colour drawn from now on toward white (hit flash), from 0 (as is) to 1 (white).
    void setFlash(float flash) { _flash = flash; }
    void setGlowAmount(float amount) { _glowAmount = amount; }

    // A filled polygon, drawn as a fan from its centre: fine for convex and star-shaped outlines.
    void fill(std::span<const sf::Vector2f> points, sf::Color color);
    void fill(std::initializer_list<sf::Vector2f> points, sf::Color color);
    // A line of light; thickness in local units.
    void line(sf::Vector2f a, sf::Vector2f b, float thickness, sf::Color color);
    void path(std::span<const sf::Vector2f> points, bool closed, float thickness, sf::Color color);
    void path(std::initializer_list<sf::Vector2f> points, bool closed, float thickness, sf::Color color);
    // A black polygon outlined with light.
    void shape(std::initializer_list<sf::Vector2f> points, sf::Color fillColor, float thickness, sf::Color lineColor);
    void shape(std::span<const sf::Vector2f> points, sf::Color fillColor, float thickness, sf::Color lineColor);
    void circle(sf::Vector2f center, float radius, float thickness, sf::Color color, int segments = 28);
    void ellipse(sf::Vector2f center, sf::Vector2f radii, float thickness, sf::Color color, int segments = 32);
    void arc(sf::Vector2f center, float radius, float from, float to, float thickness, sf::Color color,
             int segments = 16);
    void disc(sf::Vector2f center, float radius, sf::Color color, int segments = 20);
    // Fills between a line going left to right and the horizontal line at baseY (ground, ceiling, ridges).
    void band(std::span<const sf::Vector2f> points, float baseY, sf::Color color);
    // A rectangle whose colour goes from top to bottom (light near a horizon).
    void gradient(sf::FloatRect rect, sf::Color top, sf::Color bottom);
    // A soft round light: bright centre fading out, drawn on the scene and the glow layer.
    void halo(sf::Vector2f center, float radius, sf::Color color);

    void flush();

private:
    sf::Color tint(sf::Color color) const;
    void quad(sf::VertexArray &array, sf::Vector2f a, sf::Vector2f b, float thickness, sf::Color color) const;

    sf::RenderTarget &_target;
    sf::RenderTarget *_glow;
    float _glowAmount;
    float _flash = 0.f;
    sf::Transform _transform;
    sf::VertexArray _scene{sf::PrimitiveType::Triangles};
    sf::VertexArray _light{sf::PrimitiveType::Triangles};
};

// Small helpers for building shapes.
sf::Color withAlpha(sf::Color color, float alpha);
sf::Color mix(sf::Color a, sf::Color b, float t);
sf::Color scaled(sf::Color color, float factor);
sf::Vector2f polar(float angle, float radius);
std::vector<sf::Vector2f> regular(int sides, float radius, float rotation = 0.f, sf::Vector2f center = {});

} // namespace rtype::client::neon
