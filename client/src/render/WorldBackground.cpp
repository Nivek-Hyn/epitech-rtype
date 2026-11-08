#include "render/WorldBackground.hpp"

#include "common/Constants.hpp"

#include <array>
#include <cmath>
#include <random>

namespace rtype::client {

namespace {

struct Palette {
    sf::Color skyTop;
    sf::Color skyBottom;
    sf::Color star;
};

const std::array<Palette, WorldBackground::WorldCount> Palettes = {{
    {sf::Color(6, 10, 32), sf::Color(10, 44, 70), sf::Color(190, 225, 255)},  // orbital belt
    {sf::Color(26, 8, 10), sf::Color(110, 42, 22), sf::Color(255, 220, 190)}, // Mars
    {sf::Color(2, 0, 8), sf::Color(38, 8, 58), sf::Color(225, 190, 255)},     // singularity
}};

std::vector<sf::Vector2f> randomPoints(std::size_t count, unsigned int seed)
{
    std::mt19937 random(seed);
    std::uniform_real_distribution<float> x(0.f, WorldWidth);
    std::uniform_real_distribution<float> y(0.f, WorldHeight);
    std::vector<sf::Vector2f> points;
    for (std::size_t i = 0; i < count; i++)
        points.push_back({x(random), y(random)});
    return points;
}

sf::Color withAlpha(sf::Color color, std::uint8_t alpha)
{
    color.a = alpha;
    return color;
}

void drawCircle(sf::RenderTarget &target, sf::Vector2f center, float radius, sf::Color color,
                sf::Vector2f scale = {1.f, 1.f})
{
    sf::CircleShape circle(radius, 90);
    circle.setOrigin({radius, radius});
    circle.setPosition(center);
    circle.setScale(scale);
    circle.setFillColor(color);
    target.draw(circle);
}

void drawRing(sf::RenderTarget &target, sf::Vector2f center, float radius, float thickness, sf::Color color,
              sf::Vector2f scale, float degrees = 0.f)
{
    sf::CircleShape ring(radius, 120);
    ring.setOrigin({radius, radius});
    ring.setPosition(center);
    ring.setScale(scale);
    ring.setRotation(sf::degrees(degrees));
    ring.setFillColor(sf::Color::Transparent);
    ring.setOutlineThickness(thickness);
    ring.setOutlineColor(color);
    target.draw(ring);
}

} // namespace

WorldBackground::WorldBackground()
{
    std::mt19937 random(7);
    std::uniform_real_distribution<float> unit(0.f, 1.f);
    for (const auto &point : randomPoints(140, 11))
        _farStars.push_back({point, 1.f + unit(random), 0.35f + 0.4f * unit(random)});
    for (const auto &point : randomPoints(45, 23))
        _nearStars.push_back({point, 2.f + 1.5f * unit(random), 0.6f + 0.4f * unit(random)});
}

void WorldBackground::update(float dt)
{
    _time += dt;
}

void WorldBackground::draw(sf::RenderTarget &target) const
{
    drawSky(target);
    drawStars(target, _farStars, 18.f);
    if (_world == 0)
        drawOrbitalBelt(target);
    else if (_world == 1)
        drawMars(target);
    else
        drawSingularity(target);
    drawStars(target, _nearStars, 70.f);
}

void WorldBackground::drawSky(sf::RenderTarget &target) const
{
    const Palette &palette = Palettes[_world];
    sf::VertexArray sky(sf::PrimitiveType::TriangleStrip, 4);
    sky[0] = sf::Vertex{{0.f, 0.f}, palette.skyTop};
    sky[1] = sf::Vertex{{WorldWidth, 0.f}, palette.skyTop};
    sky[2] = sf::Vertex{{0.f, WorldHeight}, palette.skyBottom};
    sky[3] = sf::Vertex{{WorldWidth, WorldHeight}, palette.skyBottom};
    target.draw(sky);
}

// Stars scroll to the left; the near layer faster than the far one, which gives depth.
void WorldBackground::drawStars(sf::RenderTarget &target, const std::vector<Star> &stars, float speed) const
{
    const sf::Color color = Palettes[_world].star;
    sf::VertexArray points(sf::PrimitiveType::Triangles);
    for (const Star &star : stars) {
        const float x = std::fmod(star.position.x - _time * speed * star.size, WorldWidth) + WorldWidth;
        const sf::Vector2f p{std::fmod(x, WorldWidth), star.position.y};
        const float twinkle = 0.75f + 0.25f * std::sin(_time * 3.f + star.position.y);
        const sf::Color c = withAlpha(color, static_cast<std::uint8_t>(255.f * star.brightness * twinkle));
        const float s = star.size;
        points.append({{p.x - s, p.y}, c});
        points.append({{p.x, p.y - s}, c});
        points.append({{p.x + s, p.y}, c});
        points.append({{p.x - s, p.y}, c});
        points.append({{p.x, p.y + s}, c});
        points.append({{p.x + s, p.y}, c});
    }
    target.draw(points);
}

// A banded gas giant in the top right, its tilted rings, and a slow belt of rocks crossing the screen.
void WorldBackground::drawOrbitalBelt(sf::RenderTarget &target) const
{
    const sf::Vector2f planet{1640.f, 300.f};
    drawCircle(target, planet, 330.f, sf::Color(60, 140, 190, 40)); // atmosphere glow
    drawCircle(target, planet, 280.f, sf::Color(40, 95, 150));
    for (int band = 0; band < 5; band++)
        drawCircle(target, planet + sf::Vector2f{0.f, -150.f + band * 70.f}, 230.f - band * 18.f,
                   sf::Color(70, 150, 200, 35), {1.f, 0.18f});
    drawCircle(target, planet + sf::Vector2f{-80.f, -90.f}, 150.f, sf::Color(255, 255, 255, 18)); // light
    drawRing(target, planet, 420.f, 10.f, sf::Color(170, 210, 230, 110), {1.f, 0.22f}, -12.f);
    drawRing(target, planet, 470.f, 4.f, sf::Color(170, 210, 230, 60), {1.f, 0.22f}, -12.f);

    std::mt19937 random(3);
    std::uniform_real_distribution<float> size(6.f, 22.f);
    for (const auto &point : randomPoints(26, 5)) {
        const float x = std::fmod(point.x - _time * 35.f + WorldWidth * 2.f, WorldWidth + 60.f) - 30.f;
        drawCircle(target, {x, 620.f + std::fmod(point.y, 380.f)}, size(random), sf::Color(55, 70, 85, 200));
    }
}

// A pale sun, then two layers of dunes scrolling at different speeds.
void WorldBackground::drawMars(sf::RenderTarget &target) const
{
    drawCircle(target, {380.f, 230.f}, 150.f, sf::Color(255, 210, 160, 30));
    drawCircle(target, {380.f, 230.f}, 85.f, sf::Color(255, 225, 190, 200));
    drawDunes(target, 820.f, 90.f, 25.f, sf::Color(95, 38, 22));
    drawDunes(target, 930.f, 70.f, 60.f, sf::Color(140, 58, 30));
}

void WorldBackground::drawDunes(sf::RenderTarget &target, float baseY, float height, float speed, sf::Color color) const
{
    sf::VertexArray dunes(sf::PrimitiveType::TriangleStrip);
    for (float x = 0.f; x <= WorldWidth + 20.f; x += 20.f) {
        const float t = (x + _time * speed) * 0.004f;
        const float y = baseY - height * (0.6f * std::sin(t) + 0.4f * std::sin(t * 2.3f + 1.f));
        dunes.append({{x, y}, color});
        dunes.append({{x, WorldHeight}, color});
    }
    target.draw(dunes);
}

// A black hole on the right: a glowing accretion disk turning around a perfectly black center.
void WorldBackground::drawSingularity(sf::RenderTarget &target) const
{
    const sf::Vector2f center{1450.f, 470.f};
    drawCircle(target, center, 330.f, sf::Color(120, 40, 160, 18));
    const std::array<sf::Color, 4> disk = {sf::Color(255, 170, 60, 150), sf::Color(240, 110, 70, 120),
                                           sf::Color(190, 70, 200, 100), sf::Color(110, 60, 220, 70)};
    for (std::size_t i = 0; i < disk.size(); i++) {
        const float radius = 190.f + static_cast<float>(i) * 55.f;
        const float wobble = std::sin(_time * (1.2f - static_cast<float>(i) * 0.2f)) * 2.f; // the disk seems to turn
        drawRing(target, center, radius, 14.f - static_cast<float>(i) * 2.f, disk[i], {1.f, 0.32f}, -18.f);
        drawRing(target, center, radius * 0.98f, 3.f, withAlpha(disk[i], 60), {1.f, 0.32f}, -18.f + wobble);
    }
    drawRing(target, center, 132.f, 6.f, sf::Color(255, 200, 120, 160), {1.f, 1.f}); // light bent around the hole
    drawCircle(target, center, 125.f, sf::Color::Black);
}

} // namespace rtype::client
