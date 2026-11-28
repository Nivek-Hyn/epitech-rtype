#include "render/Effects.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

namespace rtype::client {

namespace {

constexpr std::size_t MaxParticles = 3000;

sf::Color withAlpha(sf::Color color, float alpha)
{
    color.a = static_cast<std::uint8_t>(std::clamp(alpha, 0.f, 1.f) * static_cast<float>(color.a));
    return color;
}

sf::Color mix(sf::Color a, sf::Color b, float t)
{
    auto channel = [t](std::uint8_t x, std::uint8_t y) {
        return static_cast<std::uint8_t>(static_cast<float>(x) + (static_cast<float>(y) - static_cast<float>(x)) * t);
    };
    return {channel(a.r, b.r), channel(a.g, b.g), channel(a.b, b.b), channel(a.a, b.a)};
}

} // namespace

float Effects::random(float low, float high)
{
    return std::uniform_real_distribution<float>(low, high)(_random);
}

void Effects::add(Kind kind, sf::Vector2f position, sf::Vector2f velocity, float duration, float size, sf::Color color)
{
    if (_particles.size() >= MaxParticles)
        return;
    _particles.push_back({kind, position, velocity, 0.f, duration, size, color});
}

// A shot that hits: a few fast sparks thrown back, and a small flash.
void Effects::hit(sf::Vector2f position, sf::Color color)
{
    add(Kind::Flash, position, {}, 0.08f, 22.f, sf::Color(255, 255, 255, 230));
    for (int i = 0; i < 7; ++i) {
        const float angle = random(2.2f, 4.1f);
        const float speed = random(220.f, 520.f);
        add(Kind::Spark, position, {std::cos(angle) * speed, std::sin(angle) * speed}, random(0.12f, 0.28f),
            random(2.f, 3.5f), mix(color, sf::Color::White, 0.4f));
    }
}

void Effects::burst(sf::Vector2f position, sf::Color color, float size)
{
    add(Kind::Flash, position, {}, 0.12f, 70.f * size, sf::Color(255, 250, 230, 255));
    add(Kind::Ring, position, {}, 0.38f, 90.f * size, mix(color, sf::Color::White, 0.5f));
    const int sparks = static_cast<int>(26.f * size);
    for (int i = 0; i < sparks; ++i) {
        const float angle = random(0.f, 2.f * std::numbers::pi_v<float>);
        const float speed = random(180.f, 640.f) * std::sqrt(size);
        add(Kind::Spark, position, {std::cos(angle) * speed, std::sin(angle) * speed}, random(0.25f, 0.6f),
            random(2.f, 4.f), mix(color, sf::Color(255, 240, 200), random(0.2f, 0.8f)));
    }
    const int embers = static_cast<int>(14.f * size);
    for (int i = 0; i < embers; ++i) {
        const float angle = random(0.f, 2.f * std::numbers::pi_v<float>);
        const float speed = random(40.f, 200.f) * std::sqrt(size);
        add(Kind::Ember, position, {std::cos(angle) * speed, std::sin(angle) * speed}, random(0.5f, 1.1f),
            random(4.f, 9.f) * size, mix(color, sf::Color(255, 150, 40), 0.6f));
    }
    const int smoke = static_cast<int>(8.f * size);
    for (int i = 0; i < smoke; ++i) {
        const float angle = random(0.f, 2.f * std::numbers::pi_v<float>);
        const float speed = random(20.f, 90.f);
        add(Kind::Smoke, position + sf::Vector2f(random(-10.f, 10.f), random(-10.f, 10.f)) * size,
            {std::cos(angle) * speed - 60.f, std::sin(angle) * speed}, random(0.7f, 1.3f), random(14.f, 26.f) * size,
            sf::Color(40, 36, 44, 150));
    }
}

void Effects::explosion(sf::Vector2f position, sf::Color color, float size)
{
    burst(position, color, size);
}

// A boss dies in a chain of blasts over a second and a half, then one big one, and the screen flashes.
void Effects::bossExplosion(sf::Vector2f position)
{
    for (int i = 0; i < 9; ++i)
        _delayed.push_back({static_cast<float>(i) * 0.15f,
                            position + sf::Vector2f(random(-130.f, 130.f), random(-90.f, 90.f)), random(0.8f, 1.4f)});
    _delayed.push_back({1.45f, position, 3.2f});
}

void Effects::pickup(sf::Vector2f position, sf::Color color)
{
    add(Kind::Ring, position, {}, 0.45f, 70.f, color);
    for (int i = 0; i < 16; ++i) {
        const float angle = static_cast<float>(i) / 16.f * 2.f * std::numbers::pi_v<float>;
        add(Kind::Spark, position, {std::cos(angle) * 260.f, std::sin(angle) * 260.f}, 0.3f, 3.f, color);
    }
}

// Exhaust behind a ship: a few small embers a frame that drift left and fade.
void Effects::engine(sf::Vector2f position, sf::Color color, float dt)
{
    _engineCarry += dt * 70.f;
    while (_engineCarry >= 1.f) {
        _engineCarry -= 1.f;
        add(Kind::Ember, position + sf::Vector2f(0.f, random(-3.f, 3.f)), {random(-420.f, -260.f), random(-25.f, 25.f)},
            random(0.10f, 0.22f), random(2.5f, 4.5f), color);
    }
}

void Effects::update(float dt)
{
    for (auto &delayed : _delayed) {
        delayed.delay -= dt;
        if (delayed.delay <= 0.f) {
            burst(delayed.position, sf::Color(255, 170, 60), delayed.size);
            if (delayed.size > 3.f)
                _flash = 1.f;
        }
    }
    std::erase_if(_delayed, [](const Delayed &delayed) { return delayed.delay <= 0.f; });
    for (auto &particle : _particles) {
        particle.life += dt;
        particle.position += particle.velocity * dt;
        const float drag = particle.kind == Kind::Smoke ? 1.2f : (particle.kind == Kind::Spark ? 3.f : 2.2f);
        particle.velocity *= std::max(0.f, 1.f - drag * dt);
    }
    std::erase_if(_particles, [](const Particle &particle) { return particle.life >= particle.duration; });
    _flash = std::max(0.f, _flash - dt * 2.2f);
}

void Effects::clear()
{
    _particles.clear();
    _delayed.clear();
    _flash = 0.f;
}

float Effects::flash() const
{
    return _flash;
}

// A round dot that fades to nothing at its edge: every particle is this texture, stretched and tinted.
const sf::Texture &Effects::softDot() const
{
    if (!_softDot) {
        constexpr unsigned Size = 64;
        sf::Image image({Size, Size}, sf::Color::Transparent);
        for (unsigned y = 0; y < Size; ++y)
            for (unsigned x = 0; x < Size; ++x) {
                const float dx = (static_cast<float>(x) + 0.5f) / Size * 2.f - 1.f;
                const float dy = (static_cast<float>(y) + 0.5f) / Size * 2.f - 1.f;
                const float d = std::min(1.f, std::sqrt(dx * dx + dy * dy));
                const float a = std::pow(1.f - d, 1.8f);
                image.setPixel({x, y}, sf::Color(255, 255, 255, static_cast<std::uint8_t>(a * 255.f)));
            }
        _softDot.emplace();
        if (!_softDot->loadFromImage(image))
            _softDot.reset();
        else
            _softDot->setSmooth(true);
    }
    static const sf::Texture empty;
    return _softDot ? *_softDot : empty;
}

void Effects::draw(sf::RenderTarget &target, sf::RenderTarget *glow) const
{
    sf::VertexArray smoke(sf::PrimitiveType::Triangles);
    sf::VertexArray light(sf::PrimitiveType::Triangles);
    auto quad = [](sf::VertexArray &array, sf::Vector2f center, sf::Vector2f along, float width, sf::Color color) {
        const float length = std::hypot(along.x, along.y);
        const sf::Vector2f dir = length > 0.001f ? along / length : sf::Vector2f(1.f, 0.f);
        const sf::Vector2f side(-dir.y * width, dir.x * width);
        const sf::Vector2f a = center - along - side, b = center + along - side, c = center + along + side,
                           d = center - along + side;
        const sf::Vector2f ta(0.f, 0.f), tb(64.f, 0.f), tc(64.f, 64.f), td(0.f, 64.f);
        const std::array<std::pair<sf::Vector2f, sf::Vector2f>, 6> corners{
            {{a, ta}, {b, tb}, {c, tc}, {a, ta}, {c, tc}, {d, td}}};
        for (const auto &[p, uv] : corners)
            array.append(sf::Vertex{p, color, uv});
    };
    std::vector<sf::CircleShape> rings;
    for (const auto &particle : _particles) {
        const float t = particle.life / particle.duration;
        switch (particle.kind) {
        case Kind::Spark: {
            const sf::Vector2f streak = particle.velocity * 0.022f + sf::Vector2f(1.5f, 0.f);
            quad(light, particle.position, streak, particle.size * (1.f - t * 0.6f) * 1.2f,
                 withAlpha(particle.color, 1.f - t * t));
            break;
        }
        case Kind::Ember: {
            const float size = particle.size * (1.f - t) * 1.8f;
            quad(light, particle.position, {size, 0.f}, size, withAlpha(particle.color, 1.f - t));
            break;
        }
        case Kind::Smoke: {
            const float size = particle.size * (0.6f + t) * 1.6f;
            quad(smoke, particle.position, {size, 0.f}, size, withAlpha(particle.color, (1.f - t) * 0.8f));
            break;
        }
        case Kind::Flash: {
            const float size = particle.size * (0.8f + t * 0.9f) * 1.6f;
            quad(light, particle.position, {size, 0.f}, size, withAlpha(particle.color, 1.f - t));
            break;
        }
        case Kind::Ring: {
            const float radius = particle.size * (0.2f + t * 0.8f);
            sf::CircleShape ring(radius, 48);
            ring.setOrigin({radius, radius});
            ring.setPosition(particle.position);
            ring.setFillColor(sf::Color::Transparent);
            ring.setOutlineThickness(4.f * (1.f - t) + 1.f);
            ring.setOutlineColor(withAlpha(particle.color, 1.f - t));
            rings.push_back(ring);
            break;
        }
        }
    }
    sf::RenderStates soft;
    soft.texture = &softDot();
    target.draw(smoke, soft);
    sf::RenderStates additive(sf::BlendAdd);
    additive.texture = &softDot();
    target.draw(light, additive);
    for (const auto &ring : rings)
        target.draw(ring, sf::RenderStates(sf::BlendAdd));
    if (glow) {
        glow->draw(light, additive);
        for (const auto &ring : rings)
            glow->draw(ring, sf::RenderStates(sf::BlendAdd));
    }
}

} // namespace rtype::client
