#include "render/WorldBackground.hpp"

#include "common/Constants.hpp"
#include "render/Looks.hpp"
#include "render/Neon.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <vector>

namespace rtype::client {

namespace {

using neon::Canvas;
using neon::withAlpha;
using V = sf::Vector2f;

constexpr float Pi = std::numbers::pi_v<float>;
constexpr float FadeDuration = 1.2f;
constexpr float WallSpeed = 75.f; // the walls of every world scroll at the speed of the game
constexpr float GlowAmount = 0.28f;

// A number from 0 to 1 that only depends on (i, salt): the same star, rock or tooth on every frame.
float hash(std::int64_t i, std::uint32_t salt)
{
    auto x = static_cast<std::uint32_t>(i * 0x9E3779B1ll) ^ (salt * 0x85EBCA77u);
    x ^= x >> 15;
    x *= 0x2C1B3C6Du;
    x ^= x >> 12;
    x *= 0x297A2D39u;
    x ^= x >> 15;
    return static_cast<float>(x & 0xFFFFFFu) / static_cast<float>(0x1000000);
}

// A smooth hilly line from -1 to 1, along the scrolled distance u.
float ridge(float u, float seed)
{
    return 0.5f * std::sin(u * 0.0021f + seed) + 0.3f * std::sin(u * 0.0053f + seed * 1.7f) +
           0.2f * std::sin(u * 0.0131f + seed * 2.3f);
}

sf::Color fade(sf::Color color, float alpha)
{
    return withAlpha(color, static_cast<float>(color.a) / 255.f * alpha);
}

// Points of a line sampled every step along the scrolled distance, so that they do not shimmer while scrolling.
template <typename Height> std::vector<V> scrolledLine(float offset, float step, Height height)
{
    std::vector<V> points;
    const float first = std::floor(offset / step) * step;
    for (float u = first; u - offset <= WorldWidth + step; u += step)
        points.push_back({u - offset, height(u)});
    return points;
}

// Teeth hanging from the ceiling (down = 1) or standing on the ground (down = -1), each from a seed.
void teeth(Canvas &canvas, float offset, float baseY, float down, float minHeight, float maxHeight, sf::Color fill,
           sf::Color line, sf::Color detail, std::uint32_t salt, float lean)
{
    constexpr float Slot = 70.f;
    const auto first = static_cast<std::int64_t>(std::floor(offset / Slot)) - 1;
    const auto last = static_cast<std::int64_t>(std::floor((offset + WorldWidth) / Slot)) + 1;
    for (std::int64_t i = first; i <= last; ++i) {
        const float left = static_cast<float>(i) * Slot - offset - hash(i, salt) * 20.f;
        const float width = Slot + 18.f + hash(i, salt + 1) * 40.f;
        const float height = minHeight + (maxHeight - minHeight) * hash(i, salt + 2) * hash(i, salt + 3);
        const float tipX = left + width * (0.5f + (hash(i, salt + 4) - 0.5f) * lean);
        const V a{left, baseY};
        const V tip{tipX, baseY + down * height};
        const V b{left + width, baseY};
        canvas.fill({a, tip, b}, fill);
        canvas.path({a, tip, b}, false, 2.2f, line);
        canvas.line({tipX, baseY}, tip + V(0.f, -down * height * 0.25f), 1.2f, detail);
    }
}

} // namespace

void WorldBackground::setWorld(std::size_t world)
{
    world %= WorldCount;
    if (world == _world)
        return;
    _previous = _world;
    _world = world;
    _fade = 0.f;
}

void WorldBackground::update(float dt)
{
    _time += dt;
    _fade = std::min(1.f, _fade + dt / FadeDuration);
}

void WorldBackground::draw(sf::RenderTarget &target, sf::RenderTarget *glow) const
{
    Canvas canvas(target, glow, GlowAmount);
    if (_fade < 1.f)
        drawWorld(canvas, _previous, 1.f);
    drawWorld(canvas, _world, _fade < 1.f ? _fade : 1.f);
}

void WorldBackground::drawWorld(Canvas &canvas, std::size_t world, float alpha) const
{
    canvas.fill({{0.f, 0.f}, {WorldWidth, 0.f}, {WorldWidth, WorldHeight}, {0.f, WorldHeight}},
                fade(looks::Black, alpha));
    drawStars(canvas, alpha, world == 2 ? 1.f : 0.4f);
    switch (world) {
    case 0:
        drawOrbitalBelt(canvas, alpha);
        break;
    case 1:
        drawMars(canvas, alpha);
        break;
    default:
        drawSingularity(canvas, alpha);
        break;
    }
}

void WorldBackground::drawStars(Canvas &canvas, float alpha, float twinkle) const
{
    static constexpr float Speeds[3] = {6.f, 16.f, 38.f};
    for (int depth = 0; depth < 3; ++depth) {
        const float speed = Speeds[depth];
        const float size = 0.9f + 0.55f * static_cast<float>(depth);
        for (int i = 0; i < 90; ++i) {
            const std::int64_t id = depth * 1000 + i;
            float x = hash(id, 11) * WorldWidth - _time * speed;
            x = std::fmod(x, WorldWidth);
            if (x < 0.f)
                x += WorldWidth;
            const float y = hash(id, 12) * WorldHeight;
            const float shine = 0.35f + 0.25f * static_cast<float>(depth) +
                                twinkle * 0.3f * std::sin(_time * (1.f + hash(id, 13) * 3.f) + hash(id, 14) * 6.f);
            canvas.disc({x, y}, size, fade(withAlpha(looks::Light, shine), alpha), 6);
        }
    }
}

// World 1: a blue planet above the orbit, a belt of debris below it, and the girders of a station.
void WorldBackground::drawOrbitalBelt(Canvas &canvas, float alpha) const
{
    const auto &palette = looks::worldPalette(0);
    const sf::Color accent = fade(palette.accent, alpha);
    const sf::Color deep = fade(palette.deep, alpha);

    // The planet, its night side black, its rim lit.
    const V planet{1380.f, -1020.f};
    constexpr float Radius = 1300.f;
    canvas.halo(planet + V(0.f, Radius - 40.f), 520.f, fade(withAlpha(palette.accent, 0.10f), alpha));
    canvas.disc(planet, Radius, fade(looks::Black, alpha), 96);
    canvas.arc(planet, Radius + 9.f, 0.15f, Pi - 0.15f, 7.f, fade(withAlpha(palette.deep, 0.8f), alpha), 64);
    canvas.arc(planet, Radius, 0.1f, Pi - 0.1f, 2.6f, accent, 72);
    // Lines of latitude, turning slowly.
    for (int i = 1; i <= 4; ++i) {
        const float r = Radius - 70.f * static_cast<float>(i) * static_cast<float>(i);
        canvas.arc(planet, r, 0.35f + 0.05f * static_cast<float>(i), Pi - 0.35f, 1.2f,
                   fade(withAlpha(palette.deep, 0.7f), alpha), 48);
    }
    for (int i = 0; i < 9; ++i) {
        const float u = std::fmod(static_cast<float>(i) / 9.f + _time * 0.004f, 1.f);
        const float x = planet.x + (u - 0.5f) * 2.f * Radius * 0.92f;
        const float bottom = planet.y + std::sqrt(std::max(0.f, Radius * Radius - (x - planet.x) * (x - planet.x)));
        canvas.line({x, bottom - 4.f}, {x + (x - planet.x) * 0.06f, bottom - 140.f}, 1.f,
                    fade(withAlpha(palette.deep, 0.5f), alpha));
    }

    // The belt: debris drifting on a slanted band.
    for (int i = 0; i < 150; ++i) {
        const float speed = 22.f + hash(i, 21) * 30.f;
        float x = std::fmod(hash(i, 22) * (WorldWidth + 200.f) - _time * speed, WorldWidth + 200.f);
        if (x < -100.f)
            x += WorldWidth + 200.f;
        const float y = 640.f + hash(i, 23) * 190.f + (x - WorldWidth / 2.f) * -0.08f;
        const float length = 3.f + hash(i, 24) * 9.f;
        const float angle = _time * (hash(i, 25) - 0.5f) * 3.f + hash(i, 26) * 6.f;
        canvas.line({x, y}, {x + std::cos(angle) * length, y + std::sin(angle) * length}, 1.4f,
                    hash(i, 27) < 0.15f ? accent : deep);
    }

    // Girders of the station, top and bottom.
    const float offset = _time * WallSpeed;
    for (const bool top : {true, false}) {
        const float rail = top ? 56.f : WorldHeight - 56.f;
        const float edge = top ? 0.f : WorldHeight;
        const float inner = top ? 8.f : WorldHeight - 8.f;
        canvas.fill({{0.f, edge}, {WorldWidth, edge}, {WorldWidth, rail}, {0.f, rail}}, fade(looks::Black, alpha));
        const auto truss = scrolledLine(offset, 48.f, [&](float u) {
            return static_cast<int>(std::lround(u / 48.f)) % 2 == 0 ? rail + (top ? -4.f : 4.f) : inner;
        });
        canvas.path(truss, false, 1.4f, deep);
        canvas.line({0.f, inner}, {WorldWidth, inner}, 1.4f, deep);
        canvas.line({0.f, rail}, {WorldWidth, rail}, 3.f, accent);
        // Posts with a warning light that blinks.
        const float first = std::floor(offset / 384.f) * 384.f;
        for (float u = first; u - offset < WorldWidth + 384.f; u += 384.f) {
            const float x = u - offset;
            canvas.line({x, edge}, {x, rail}, 2.4f, accent);
            const bool on = std::fmod(_time + u * 0.001f, 1.6f) < 0.5f;
            const V light{x, rail + (top ? 9.f : -9.f)};
            canvas.disc(light, 3.f, fade(on ? looks::Light : palette.deep, alpha), 8);
            if (on)
                canvas.halo(light, 16.f, fade(withAlpha(looks::Light, 0.4f), alpha));
        }
    }
}

// World 2: a red-orange horizon, two ranges of mountains, a moon, and a cave of rock above and below.
void WorldBackground::drawMars(Canvas &canvas, float alpha) const
{
    const auto &palette = looks::worldPalette(1);
    const sf::Color accent = fade(palette.accent, alpha);
    const sf::Color deep = fade(palette.deep, alpha);
    const sf::Color black = fade(looks::Black, alpha);

    canvas.gradient({{0.f, 420.f}, {WorldWidth, 330.f}}, fade(withAlpha(palette.accent, 0.f), alpha),
                    fade(withAlpha(palette.accent, 0.16f), alpha));

    // Phobos, with a few craters.
    const V moon{1500.f, 200.f};
    canvas.disc(moon, 58.f, black, 40);
    canvas.circle(moon, 58.f, 2.f, deep, 40);
    canvas.arc(moon, 58.f, -Pi * 0.35f, Pi * 0.55f, 2.6f, accent, 20);
    canvas.circle(moon + V(14.f, -12.f), 11.f, 1.2f, deep, 14);
    canvas.circle(moon + V(-18.f, 16.f), 7.f, 1.2f, deep, 12);

    // Far range, then near range with its contour lines.
    const auto far = scrolledLine(_time * 14.f, 22.f, [](float u) { return 690.f + ridge(u, 1.3f) * 80.f; });
    canvas.band(far, WorldHeight, black);
    canvas.path(far, false, 1.8f, deep);
    const auto near = scrolledLine(_time * 32.f, 18.f, [](float u) { return 830.f + ridge(u * 1.4f, 4.1f) * 70.f; });
    canvas.band(near, WorldHeight, black);
    for (int i = 1; i <= 3; ++i) {
        std::vector<V> contour = near;
        for (auto &point : contour)
            point.y += 22.f * static_cast<float>(i);
        canvas.path(contour, false, 1.f, fade(withAlpha(palette.deep, 0.9f - 0.2f * static_cast<float>(i)), alpha));
    }
    canvas.path(near, false, 2.4f, accent);

    // The cave: rock teeth hanging from the ceiling, boulders on the ground.
    const float offset = _time * WallSpeed;
    canvas.fill({{0.f, 0.f}, {WorldWidth, 0.f}, {WorldWidth, 26.f}, {0.f, 26.f}}, black);
    teeth(canvas, offset, 26.f, 1.f, 18.f, 95.f, black, accent, deep, 31, 0.5f);
    const auto ground = scrolledLine(offset, 26.f, [](float u) {
        const auto step = static_cast<std::int64_t>(std::floor(u / 26.f));
        return WorldHeight - 52.f - hash(step, 41) * 18.f - std::max(0.f, ridge(u * 3.f, 7.f)) * 34.f;
    });
    canvas.band(ground, WorldHeight, black);
    canvas.path(ground, false, 2.4f, accent);
}

// World 3: a black hole, its disc of light turning around it, and crystals growing from the walls.
void WorldBackground::drawSingularity(Canvas &canvas, float alpha) const
{
    const auto &palette = looks::worldPalette(2);
    const sf::Color accent = fade(palette.accent, alpha);
    const sf::Color deep = fade(palette.deep, alpha);
    const sf::Color black = fade(looks::Black, alpha);
    const V hole{1500.f, 330.f}; // high and to the right, away from where the boss fights

    canvas.halo(hole, 520.f, fade(withAlpha(palette.accent, 0.10f), alpha));
    // The disc: dashed rings, the inner ones faster. Back half first, then the hole, then the front half.
    const auto disc = [&](bool front) {
        for (int ring = 0; ring < 9; ++ring) {
            const float r = 150.f + 24.f * static_cast<float>(ring);
            const float speed = 0.9f / (1.f + 0.35f * static_cast<float>(ring));
            const int dashes = 18 + ring * 3;
            for (int d = 0; d < dashes; ++d) {
                float a = _time * speed + 2.f * Pi * static_cast<float>(d) / static_cast<float>(dashes);
                a = std::fmod(a, 2.f * Pi);
                const bool isFront = a < Pi;
                if (isFront != front)
                    continue;
                const float b = a + 0.09f;
                const V p{hole.x + std::cos(a) * r, hole.y + std::sin(a) * r * 0.24f};
                const V q{hole.x + std::cos(b) * r, hole.y + std::sin(b) * r * 0.24f};
                const bool white = (d + ring) % 7 == 0;
                canvas.line(p, q, ring < 3 ? 2.2f : 1.5f,
                            white ? fade(looks::Light, alpha)
                                  : (ring % 2 == 0 ? accent : fade(withAlpha(palette.accent, 0.55f), alpha)));
            }
        }
    };
    disc(false);
    // Light bent over the top of the hole.
    canvas.ellipse(hole, {128.f, 124.f}, 1.2f, deep, 48);
    canvas.arc(hole, 116.f, Pi * 1.08f, Pi * 1.92f, 2.f, accent, 24);
    canvas.disc(hole, 96.f, black, 48);
    canvas.circle(hole, 99.f, 2.2f, fade(looks::Light, alpha), 48);
    disc(true);

    // Crystals on the ceiling and the floor.
    const float offset = _time * WallSpeed;
    canvas.fill({{0.f, 0.f}, {WorldWidth, 0.f}, {WorldWidth, 22.f}, {0.f, 22.f}}, black);
    canvas.fill(
        {{0.f, WorldHeight - 22.f}, {WorldWidth, WorldHeight - 22.f}, {WorldWidth, WorldHeight}, {0.f, WorldHeight}},
        black);
    teeth(canvas, offset, 22.f, 1.f, 20.f, 110.f, black, accent, deep, 51, 1.4f);
    teeth(canvas, offset + 35.f, WorldHeight - 22.f, -1.f, 20.f, 110.f, black, accent, deep, 61, 1.4f);
}

} // namespace rtype::client
