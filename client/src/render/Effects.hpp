#pragma once

#include <SFML/Graphics.hpp>

#include <cstdint>
#include <optional>
#include <random>
#include <vector>

namespace rtype::client {

// Short-lived particles that make hits and explosions readable: sparks, embers, smoke, shock rings and flashes.
// They are purely visual: the server never hears about them.
class Effects {
public:
    void hit(sf::Vector2f position, sf::Color color);
    void explosion(sf::Vector2f position, sf::Color color, float size);
    void bossExplosion(sf::Vector2f position);
    void pickup(sf::Vector2f position, sf::Color color);
    void engine(sf::Vector2f position, sf::Color color, float dt);

    void update(float dt);
    void clear();
    // Draws on the frame; glow (when not null) receives the bright parts again so that they shine.
    void draw(sf::RenderTarget &target, sf::RenderTarget *glow) const;
    // How white the screen flashes now, from 0 to 1 (a boss dying).
    float flash() const;

private:
    enum class Kind : std::uint8_t { Spark, Ember, Smoke, Ring, Flash };
    struct Particle {
        Kind kind;
        sf::Vector2f position;
        sf::Vector2f velocity;
        float life;
        float duration;
        float size;
        sf::Color color;
    };
    struct Delayed {
        float delay;
        sf::Vector2f position;
        float size;
    };

    float random(float low, float high);
    void add(Kind kind, sf::Vector2f position, sf::Vector2f velocity, float duration, float size, sf::Color color);
    void burst(sf::Vector2f position, sf::Color color, float size);

    const sf::Texture &softDot() const;

    std::vector<Particle> _particles;
    std::vector<Delayed> _delayed;
    std::mt19937 _random{1234};
    float _flash = 0.f;
    float _engineCarry = 0.f;
    mutable std::optional<sf::Texture> _softDot;
};

} // namespace rtype::client
