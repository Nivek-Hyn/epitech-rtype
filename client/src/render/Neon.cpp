#include "render/Neon.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace rtype::client::neon {

namespace {

constexpr float Tau = 2.f * std::numbers::pi_v<float>;

void triangle(sf::VertexArray &array, const sf::Transform &transform, sf::Vector2f a, sf::Vector2f b, sf::Vector2f c,
              sf::Color ca, sf::Color cb, sf::Color cc)
{
    array.append({transform.transformPoint(a), ca});
    array.append({transform.transformPoint(b), cb});
    array.append({transform.transformPoint(c), cc});
}

sf::Vector2f centroid(std::span<const sf::Vector2f> points)
{
    sf::Vector2f sum;
    for (const auto &point : points)
        sum += point;
    return sum / static_cast<float>(points.size());
}

} // namespace

Canvas::Canvas(sf::RenderTarget &target, sf::RenderTarget *glow, float glowAmount)
    : _target(target), _glow(glow), _glowAmount(glowAmount)
{
}

Canvas::~Canvas()
{
    flush();
}

sf::Color Canvas::tint(sf::Color color) const
{
    if (_flash <= 0.f)
        return color;
    const sf::Color white(255, 255, 255, color.a);
    return mix(color, white, std::min(_flash, 1.f));
}

void Canvas::quad(sf::VertexArray &array, sf::Vector2f a, sf::Vector2f b, float thickness, sf::Color color) const
{
    sf::Vector2f direction = b - a;
    const float length = std::hypot(direction.x, direction.y);
    if (length < 0.0001f)
        return;
    direction /= length;
    const float half = thickness / 2.f;
    const sf::Vector2f normal(-direction.y * half, direction.x * half);
    // Extending both ends by half the thickness closes the corners of a path.
    const sf::Vector2f start = a - direction * half;
    const sf::Vector2f end = b + direction * half;
    triangle(array, _transform, start + normal, end + normal, end - normal, color, color, color);
    triangle(array, _transform, start + normal, end - normal, start - normal, color, color, color);
}

void Canvas::fill(std::span<const sf::Vector2f> points, sf::Color color)
{
    if (points.size() < 3)
        return;
    const sf::Color shade = tint(color);
    const sf::Vector2f center = centroid(points);
    for (std::size_t i = 0; i < points.size(); ++i)
        triangle(_scene, _transform, center, points[i], points[(i + 1) % points.size()], shade, shade, shade);
}

void Canvas::fill(std::initializer_list<sf::Vector2f> points, sf::Color color)
{
    fill(std::span<const sf::Vector2f>(points.begin(), points.size()), color);
}

void Canvas::line(sf::Vector2f a, sf::Vector2f b, float thickness, sf::Color color)
{
    const sf::Color shade = tint(color);
    quad(_scene, a, b, thickness, shade);
    if (_glow && _glowAmount > 0.f)
        quad(_light, a, b, thickness * 1.6f, withAlpha(shade, _glowAmount * static_cast<float>(shade.a) / 255.f));
}

void Canvas::path(std::span<const sf::Vector2f> points, bool closed, float thickness, sf::Color color)
{
    if (points.size() < 2)
        return;
    for (std::size_t i = 0; i + 1 < points.size(); ++i)
        line(points[i], points[i + 1], thickness, color);
    if (closed && points.size() > 2)
        line(points.back(), points.front(), thickness, color);
}

void Canvas::path(std::initializer_list<sf::Vector2f> points, bool closed, float thickness, sf::Color color)
{
    path(std::span<const sf::Vector2f>(points.begin(), points.size()), closed, thickness, color);
}

void Canvas::shape(std::span<const sf::Vector2f> points, sf::Color fillColor, float thickness, sf::Color lineColor)
{
    fill(points, fillColor);
    path(points, true, thickness, lineColor);
}

void Canvas::shape(std::initializer_list<sf::Vector2f> points, sf::Color fillColor, float thickness,
                   sf::Color lineColor)
{
    shape(std::span<const sf::Vector2f>(points.begin(), points.size()), fillColor, thickness, lineColor);
}

void Canvas::circle(sf::Vector2f center, float radius, float thickness, sf::Color color, int segments)
{
    arc(center, radius, 0.f, Tau, thickness, color, segments);
}

void Canvas::ellipse(sf::Vector2f center, sf::Vector2f radii, float thickness, sf::Color color, int segments)
{
    sf::Vector2f previous = center + sf::Vector2f(radii.x, 0.f);
    for (int i = 1; i <= segments; ++i) {
        const float angle = Tau * static_cast<float>(i) / static_cast<float>(segments);
        const sf::Vector2f next = center + sf::Vector2f(std::cos(angle) * radii.x, std::sin(angle) * radii.y);
        line(previous, next, thickness, color);
        previous = next;
    }
}

void Canvas::arc(sf::Vector2f center, float radius, float from, float to, float thickness, sf::Color color,
                 int segments)
{
    sf::Vector2f previous = center + polar(from, radius);
    for (int i = 1; i <= segments; ++i) {
        const float angle = from + (to - from) * static_cast<float>(i) / static_cast<float>(segments);
        const sf::Vector2f next = center + polar(angle, radius);
        line(previous, next, thickness, color);
        previous = next;
    }
}

void Canvas::disc(sf::Vector2f center, float radius, sf::Color color, int segments)
{
    const sf::Color shade = tint(color);
    for (int i = 0; i < segments; ++i) {
        const float a = Tau * static_cast<float>(i) / static_cast<float>(segments);
        const float b = Tau * static_cast<float>(i + 1) / static_cast<float>(segments);
        triangle(_scene, _transform, center, center + polar(a, radius), center + polar(b, radius), shade, shade, shade);
    }
}

void Canvas::band(std::span<const sf::Vector2f> points, float baseY, sf::Color color)
{
    const sf::Color shade = tint(color);
    for (std::size_t i = 0; i + 1 < points.size(); ++i) {
        const sf::Vector2f a = points[i];
        const sf::Vector2f b = points[i + 1];
        triangle(_scene, _transform, a, b, {b.x, baseY}, shade, shade, shade);
        triangle(_scene, _transform, a, {b.x, baseY}, {a.x, baseY}, shade, shade, shade);
    }
}

void Canvas::gradient(sf::FloatRect rect, sf::Color top, sf::Color bottom)
{
    const sf::Vector2f a = rect.position;
    const sf::Vector2f b = a + sf::Vector2f(rect.size.x, 0.f);
    const sf::Vector2f c = a + rect.size;
    const sf::Vector2f d = a + sf::Vector2f(0.f, rect.size.y);
    triangle(_scene, _transform, a, b, c, tint(top), tint(top), tint(bottom));
    triangle(_scene, _transform, a, c, d, tint(top), tint(bottom), tint(bottom));
}

void Canvas::halo(sf::Vector2f center, float radius, sf::Color color)
{
    const sf::Color shade = tint(color);
    const sf::Color clear = withAlpha(shade, 0.f);
    constexpr int Segments = 24;
    for (int i = 0; i < Segments; ++i) {
        const float a = Tau * static_cast<float>(i) / Segments;
        const float b = Tau * static_cast<float>(i + 1) / Segments;
        triangle(_scene, _transform, center, center + polar(a, radius), center + polar(b, radius), shade, clear, clear);
        if (_glow)
            triangle(_light, _transform, center, center + polar(a, radius * 1.4f), center + polar(b, radius * 1.4f),
                     shade, clear, clear);
    }
}

void Canvas::flush()
{
    if (_scene.getVertexCount() > 0)
        _target.draw(_scene);
    if (_glow && _light.getVertexCount() > 0) {
        sf::RenderStates states;
        states.blendMode = sf::BlendAdd;
        _glow->draw(_light, states);
    }
    _scene.clear();
    _light.clear();
}

sf::Color withAlpha(sf::Color color, float alpha)
{
    color.a = static_cast<std::uint8_t>(std::clamp(alpha, 0.f, 1.f) * 255.f);
    return color;
}

sf::Color mix(sf::Color a, sf::Color b, float t)
{
    const auto channel = [t](std::uint8_t x, std::uint8_t y) {
        return static_cast<std::uint8_t>(std::lround(static_cast<float>(x) + (static_cast<float>(y) - x) * t));
    };
    return {channel(a.r, b.r), channel(a.g, b.g), channel(a.b, b.b), channel(a.a, b.a)};
}

sf::Color scaled(sf::Color color, float factor)
{
    const auto channel = [factor](std::uint8_t x) {
        return static_cast<std::uint8_t>(std::clamp(static_cast<float>(x) * factor, 0.f, 255.f));
    };
    return {channel(color.r), channel(color.g), channel(color.b), color.a};
}

sf::Vector2f polar(float angle, float radius)
{
    return {std::cos(angle) * radius, std::sin(angle) * radius};
}

std::vector<sf::Vector2f> regular(int sides, float radius, float rotation, sf::Vector2f center)
{
    std::vector<sf::Vector2f> points;
    points.reserve(static_cast<std::size_t>(sides));
    for (int i = 0; i < sides; ++i)
        points.push_back(center + polar(rotation + Tau * static_cast<float>(i) / static_cast<float>(sides), radius));
    return points;
}

} // namespace rtype::client::neon
