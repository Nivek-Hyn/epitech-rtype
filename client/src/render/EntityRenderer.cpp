#include "render/EntityRenderer.hpp"

#include "common/Constants.hpp"
#include "render/Looks.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <string>
#include <vector>

namespace rtype::client {

namespace {

using neon::Canvas;
using neon::polar;
using neon::withAlpha;
using protocol::EntityState;
using protocol::EntityType;
using V = sf::Vector2f;

constexpr float Pi = std::numbers::pi_v<float>;
constexpr float Outline = 2.6f;
constexpr float Detail = 1.5f;

sf::Transform place(V center, float scale = 1.f, float degrees = 0.f, V stretch = {1.f, 1.f})
{
    sf::Transform transform;
    transform.translate(center);
    transform.rotate(sf::degrees(degrees));
    transform.scale({scale * stretch.x, scale * stretch.y});
    return transform;
}

float pulse(float time, float speed, float low = 0.f, float high = 1.f)
{
    return low + (high - low) * (0.5f + 0.5f * std::sin(time * speed));
}

// A red eye: what makes an enemy read as an enemy, whatever its world.
void eye(Canvas &canvas, V center, float radius, float time)
{
    canvas.halo(center, radius * 3.2f, withAlpha(looks::Danger, pulse(time, 7.f, 0.35f, 0.7f)));
    canvas.disc(center, radius, looks::Danger);
    canvas.disc(center, radius * 0.45f, looks::DangerHot);
}

// ---------------------------------------------------------------- players

void drawShip(Canvas &canvas, const EntityState &ship, float time, float tilt)
{
    const sf::Color color = looks::playerColor(ship.variant);
    const float lean = std::clamp(tilt, -2.f, 2.f);
    canvas.setTransform(
        place({ship.x + 28.f, ship.y + 15.f}, 1.15f, -lean * 3.5f, {1.f, 1.f - std::abs(lean) * 0.08f}));

    // Hull: a dart pointing right, two swept wings, the engine at the back.
    canvas.shape({{36, 0},
                  {14, -5},
                  {-2, -7},
                  {-14, -17},
                  {-24, -17},
                  {-17, -6},
                  {-26, -5},
                  {-26, 5},
                  {-17, 6},
                  {-24, 17},
                  {-14, 17},
                  {-2, 7},
                  {14, 5}},
                 looks::Black, Outline, looks::Light);
    canvas.line({-3, -8}, {-14, -15}, 2.f, color);
    canvas.line({-3, 8}, {-14, 15}, 2.f, color);
    canvas.line({14, 0}, {30, 0}, Detail, withAlpha(looks::Light, 0.5f));
    // Cockpit in the player's colour.
    canvas.fill({{17, 0}, {8, -3.6f}, {-2, -3.2f}, {-2, 3.2f}, {8, 3.6f}}, color);
    // Engine.
    const float flame = pulse(time, 38.f, 0.8f, 1.15f);
    canvas.halo({-27, 0}, 13.f * flame, withAlpha(color, 0.75f));
    canvas.disc({-27, 0}, 3.4f, looks::Light);
}

// ---------------------------------------------------------------- shots

void drawPlayerShot(Canvas &canvas, const EntityState &shot, float time)
{
    if (shot.variant == 1) { // laser
        canvas.setTransform(place({shot.x + 28.f, shot.y + 9.f}));
        const sf::Color pink(255, 90, 220);
        canvas.halo({0, 0}, 30.f, withAlpha(pink, 0.45f));
        canvas.line({-28, 0}, {28, 0}, 7.f, pink);
        canvas.line({-26, 0}, {26, 0}, 2.5f, looks::Light);
        return;
    }
    canvas.setTransform(place({shot.x + 18.f, shot.y + 12.f}));
    const sf::Color blue(120, 220, 255);
    canvas.halo({6, 0}, 16.f, withAlpha(blue, pulse(time, 30.f, 0.4f, 0.6f)));
    canvas.line({-14, 0}, {14, 0}, 5.f, blue);
    canvas.line({-10, 0}, {14, 0}, 2.f, looks::Light);
}

// Every enemy shot is red with a white-hot centre; its shape tells its pattern.
void drawEnemyShot(Canvas &canvas, const EntityState &shot, float time)
{
    const V center{shot.x + 18.f, shot.y + 12.f};
    const float local = time + static_cast<float>(shot.netId % 7) * 0.15f;
    canvas.setTransform(place(center));
    switch (shot.variant) {
    case 1: // dart, fast and straight
        canvas.halo({0, 0}, 18.f, withAlpha(looks::Danger, 0.5f));
        canvas.shape({{-14, 0}, {2, -5}, {12, 0}, {2, 5}}, looks::Danger, Detail, looks::DangerHot);
        canvas.line({-10, 0}, {6, 0}, 2.f, looks::DangerHot);
        break;
    case 2: { // wave
        canvas.halo({0, 0}, 18.f, withAlpha(looks::Danger, 0.5f));
        std::array<V, 7> wave;
        for (std::size_t i = 0; i < wave.size(); ++i) {
            const float x = -12.f + 4.f * static_cast<float>(i);
            wave[i] = {x, std::sin(local * 14.f + x * 0.5f) * 4.f};
        }
        canvas.path(wave, false, 3.f, looks::Danger);
        canvas.disc({-12, wave[0].y}, 4.f, looks::DangerHot);
        break;
    }
    case 3: { // spinning star
        canvas.setTransform(place(center, 1.f, local * 400.f));
        canvas.halo({0, 0}, 20.f, withAlpha(looks::Danger, 0.5f));
        std::vector<V> star;
        for (int i = 0; i < 8; ++i)
            star.push_back(polar(Pi / 4.f * static_cast<float>(i), i % 2 == 0 ? 11.f : 4.f));
        canvas.shape(star, looks::Danger, Detail, looks::DangerHot);
        break;
    }
    case 4: { // heavy orb
        const float size = pulse(local, 15.f, 0.9f, 1.1f);
        canvas.halo({0, 0}, 28.f * size, withAlpha(looks::Danger, 0.6f));
        canvas.disc({0, 0}, 11.f * size, looks::Danger);
        canvas.circle({0, 0}, 11.f * size, 2.f, looks::DangerHot);
        canvas.disc({0, 0}, 5.f * size, looks::DangerHot);
        break;
    }
    default: // small orb
        canvas.halo({0, 0}, 17.f, withAlpha(looks::Danger, pulse(local, 20.f, 0.45f, 0.65f)));
        canvas.disc({0, 0}, 6.5f, looks::Danger);
        canvas.disc({0, 0}, 3.f, looks::DangerHot);
        break;
    }
}

// ---------------------------------------------------------------- bonuses and obstacles

// A turning diamond with an icon, so bonuses can be told apart without colour: spread, laser, extra life.
void drawBonus(Canvas &canvas, const EntityState &bonus, float time)
{
    static const std::array<sf::Color, 3> colors{sf::Color(80, 200, 255), sf::Color(255, 90, 220),
                                                 sf::Color(90, 235, 120)};
    const sf::Color color = colors[std::min<std::size_t>(bonus.variant, 2)];
    const V center{bonus.x + 16.f, bonus.y + 16.f};
    canvas.setTransform(place(center, pulse(time, 6.f, 0.95f, 1.08f)));
    canvas.halo({0, 0}, 34.f, withAlpha(color, 0.35f));
    canvas.setTransform(place(center, 1.f, time * 90.f));
    canvas.shape({{0, -19}, {19, 0}, {0, 19}, {-19, 0}}, looks::Black, Outline, color);
    canvas.path({{0, -24}, {24, 0}, {0, 24}, {-24, 0}}, true, 1.f, withAlpha(color, 0.5f));
    canvas.setTransform(place(center));
    switch (bonus.variant) {
    case 0: // spread: three shots fanning out
        canvas.disc({-8, 0}, 2.6f, looks::Light);
        canvas.line({-3, 0}, {9, 0}, 2.4f, looks::Light);
        canvas.line({-3, -2}, {7, -8}, 2.4f, looks::Light);
        canvas.line({-3, 2}, {7, 8}, 2.4f, looks::Light);
        break;
    case 1: // laser: one long beam
        canvas.line({-9, 0}, {9, 0}, 4.f, looks::Light);
        break;
    default: // life: a cross
        canvas.line({-7, 0}, {7, 0}, 3.f, looks::Light);
        canvas.line({0, -7}, {0, 7}, 3.f, looks::Light);
        break;
    }
}

void drawObstacle(Canvas &canvas, const EntityState &rock, float time)
{
    const float local = time + static_cast<float>(rock.netId % 13) * 0.31f;
    const V center{rock.x + ObstacleSize / 2.f, rock.y + ObstacleSize / 2.f};
    const std::size_t world = std::min<std::size_t>(rock.variant, 2);
    const auto &palette = looks::worldPalette(world);
    if (world == 0) { // a torn solar panel, tumbling
        canvas.setTransform(place(center, 1.f, local * 40.f, {1.f, std::cos(local * 1.7f) * 0.6f + 0.4f}));
        canvas.shape({{-30, -18}, {24, -22}, {32, 14}, {6, 20}, {-4, 12}, {-26, 18}}, looks::Black, Outline,
                     palette.accent);
        for (const float x : {-14.f, 2.f, 18.f})
            canvas.line({x, -20}, {x + 3.f, 17}, Detail, palette.deep);
        canvas.line({-28, 0}, {30, -3}, Detail, palette.deep);
        return;
    }
    if (world == 1) { // a Martian rock
        canvas.setTransform(place(center, 1.f, local * 25.f));
        canvas.shape({{-32, -6}, {-22, -26}, {2, -33}, {24, -24}, {33, -2}, {26, 22}, {4, 32}, {-20, 27}, {-33, 12}},
                     looks::Black, Outline, palette.accent);
        canvas.circle({-8, -8}, 7.f, Detail, palette.deep);
        canvas.circle({12, 10}, 5.f, Detail, palette.deep);
        canvas.line({-22, 16}, {-6, 8}, Detail, palette.deep);
        return;
    }
    // A crystal shard from the singularity, catching the light.
    canvas.setTransform(place(center, 1.f, -20.f + std::sin(local * 1.3f) * 25.f));
    canvas.shape({{-34, 0}, {-12, -13}, {20, -11}, {34, 0}, {20, 11}, {-12, 13}}, looks::Black, Outline,
                 palette.accent);
    canvas.path({{-34, 0}, {-12, -2}, {20, 0}, {34, 0}}, false, Detail, palette.deep);
    canvas.line({-12, -13}, {-12, 13}, Detail, palette.deep);
    canvas.line({20, -11}, {20, 11}, Detail, palette.deep);
    canvas.halo({-12, -2}, 9.f, withAlpha(looks::Light, pulse(local, 2.3f, 0.f, 0.6f)));
}

// ---------------------------------------------------------------- enemies of the Orbital belt (they face left)

void drawScout(Canvas &canvas, V center, float time, const looks::Palette &palette)
{
    canvas.setTransform(place(center));
    const float open = std::abs(std::cos(time * 2.6f)) * 0.75f + 0.25f;
    for (const float side : {-1.f, 1.f}) {
        canvas.line({2, side * 8.f}, {2, side * 13.f}, Detail, palette.accent);
        const float near = 13.f;
        const float far = 13.f + 14.f * open;
        canvas.shape({{-8, side * near}, {12, side * near}, {12, side * far}, {-8, side * far}}, looks::Black, Detail,
                     palette.accent);
        canvas.line({2, side * near}, {2, side * far}, 1.f, palette.deep);
    }
    canvas.shape({{-20, 0}, {0, -10}, {16, -4}, {16, 4}, {0, 10}}, looks::Black, Outline, palette.accent);
    canvas.line({16, -3}, {24, 0}, Detail, palette.accent);
    canvas.line({16, 3}, {24, 0}, Detail, palette.accent);
    eye(canvas, {-9, 0}, 3.2f, time);
}

void drawProbe(Canvas &canvas, V center, float time, const looks::Palette &palette)
{
    canvas.setTransform(place(center));
    for (int i = 0; i < 3; ++i) {
        const float angle = -0.55f + 0.55f * static_cast<float>(i);
        canvas.line(polar(angle, 14.f), polar(angle, 27.f), Detail, palette.accent);
        canvas.disc(polar(angle, 27.f), 2.2f, palette.accent);
    }
    canvas.disc({0, 0}, 15.f, looks::Black);
    canvas.circle({0, 0}, 15.f, Outline, palette.accent);
    canvas.ellipse({0, 0}, {29.f, 9.f * std::sin(time * 2.2f)}, Detail, palette.accent);
    canvas.circle({0, 0}, 9.f, Detail, palette.deep);
    eye(canvas, {0, 0}, 4.5f, time);
}

void drawRelay(Canvas &canvas, V center, float time, const looks::Palette &palette)
{
    canvas.setTransform(place(center));
    for (int i = 0; i < 6; ++i) {
        const float from = time * -1.2f + Pi / 3.f * static_cast<float>(i);
        canvas.arc({0, 0}, 25.f, from, from + 0.6f, Outline, palette.accent, 5);
    }
    canvas.shape(neon::regular(6, 18.f, time * 0.8f), looks::Black, Outline, palette.accent);
    canvas.path(neon::regular(6, 10.f, -time * 1.6f), true, Detail, palette.deep);
    eye(canvas, {0, 0}, 4.f, time);
}

// Battle damage of a boss, from 0 (intact) to 1: cracks of light from half its health, then sparks and smoke.
void wear(Canvas &canvas, float damage, float time, std::initializer_list<std::initializer_list<V>> cracks,
          std::initializer_list<V> sparks, const looks::Palette &palette)
{
    if (damage < 0.5f)
        return;
    const float strength = std::min(1.f, (damage - 0.5f) / 0.3f);
    for (const auto &crack : cracks)
        canvas.path(crack, false, Detail, withAlpha(looks::Light, 0.25f + 0.45f * strength));
    int index = 0;
    for (const V spark : sparks) {
        const float flicker =
            std::fmod(time * (5.f + static_cast<float>(index) * 1.7f) + static_cast<float>(index) * 0.37f, 1.f);
        if (flicker < 0.35f) {
            canvas.halo(spark, 10.f + 8.f * flicker, withAlpha(looks::Light, 0.8f * strength));
            canvas.line(spark, spark + polar(time * 40.f + static_cast<float>(index), 9.f), 1.5f, looks::Light);
        }
        if (damage > 0.75f) { // smoke rising from the wounds
            const float rise = std::fmod(time * 0.7f + static_cast<float>(index) * 0.31f, 1.f);
            canvas.circle(spark + V{10.f * rise, -34.f * rise}, 5.f + 12.f * rise, Detail,
                          withAlpha(palette.deep, 1.f - rise), 12);
        }
        ++index;
    }
}

// A tone of the world's colour inside a black shape: what gives the bosses their depth.
sf::Color plate(const looks::Palette &palette, float alpha = 0.32f)
{
    return withAlpha(palette.deep, alpha);
}

// The Carrier: a warship of the station, guns at the bow, a hangar that launches drones, a reactor in its heart.
void drawCarrier(Canvas &canvas, V center, float time, const looks::Palette &palette, float damage)
{
    canvas.setTransform(place(center));
    const sf::Color accent = palette.accent;
    // Engines: three nozzles, their flames flickering.
    for (const float y : {-26.f, 0.f, 26.f}) {
        const float flame = 22.f + 10.f * std::sin(time * 31.f + y) + 6.f * std::sin(time * 17.f + y * 0.3f);
        canvas.halo({140, y}, 34.f, withAlpha(accent, pulse(time + y, 9.f, 0.45f, 0.75f)));
        canvas.fill({{136, y - 8}, {136 + flame, y}, {136, y + 8}}, withAlpha(accent, 0.55f));
        canvas.line({136, y}, {136 + flame * 0.7f, y}, 3.f, looks::Light);
        canvas.shape({{118, y - 12}, {140, y - 9}, {140, y + 9}, {118, y + 12}}, looks::Black, Detail, accent);
    }
    // Radar mast and its turning dish.
    canvas.line({66, -78}, {66, -104}, Outline, accent);
    canvas.line({58, -92}, {74, -92}, Detail, palette.deep);
    canvas.ellipse({66, -106}, {24.f * std::cos(time * 1.6f), 6.f}, Outline, accent);
    canvas.halo({66, -106}, 6.f, withAlpha(looks::Danger, pulse(time, 4.f)));
    // The bridge tower, its windows lit.
    canvas.shape({{6, -48}, {28, -80}, {100, -80}, {116, -48}}, looks::Black, Outline, accent);
    canvas.fill({{30, -74}, {96, -74}, {104, -56}, {22, -56}}, plate(palette));
    for (int i = 0; i < 7; ++i) {
        const float x = 34.f + 9.5f * static_cast<float>(i);
        const bool lit = (static_cast<int>(time * 2.f) + i * 3) % 5 != 0;
        canvas.line({x, -68}, {x + 5.f, -68}, 2.4f, lit ? looks::Light : palette.deep);
    }
    // The hangar keel below, its door sliding open to let out a drone.
    canvas.shape({{-50, 48}, {-30, 84}, {84, 84}, {104, 48}}, looks::Black, Outline, accent);
    const float door = std::clamp(std::sin(time * 0.9f) * 1.6f, 0.f, 1.f);
    canvas.fill({{-14, 58}, {70, 58}, {70, 76}, {-14, 76}}, withAlpha(accent, 0.12f + 0.25f * door));
    canvas.line({-14 + 84.f * door, 58}, {-14 + 84.f * door, 76}, Detail, accent);
    canvas.shape({{-14 + 84.f * door, 58}, {70, 58}, {70, 76}, {-14 + 84.f * door, 76}}, looks::Black, Detail,
                 palette.deep);
    if (door > 0.2f) {
        canvas.halo({10, 67}, 16.f, withAlpha(looks::Light, 0.5f * door));
        canvas.path({{2, 67}, {12, 62}, {18, 67}, {12, 72}}, true, Detail, looks::Light);
    }
    // Hull, with its armour plates.
    const std::initializer_list<V> hull = {{-140, 0},  {-118, -28}, {-70, -46}, {0, -52},  {70, -48},
                                           {118, -40}, {130, -18},  {130, 18},  {118, 40}, {70, 48},
                                           {0, 52},    {-70, 46},   {-118, 28}};
    canvas.shape(hull, looks::Black, 3.2f, accent);
    canvas.fill({{-104, -24}, {-62, -38}, {-62, -10}, {-104, -8}}, plate(palette));
    canvas.fill({{-104, 24}, {-62, 38}, {-62, 10}, {-104, 8}}, plate(palette));
    canvas.fill({{60, -40}, {112, -34}, {120, -16}, {60, -16}}, plate(palette));
    canvas.fill({{60, 40}, {112, 34}, {120, 16}, {60, 16}}, plate(palette));
    canvas.path({{-118, -28}, {-62, -38}, {-62, 38}, {-118, 28}}, false, Detail, palette.deep);
    canvas.line({60, -48}, {60, 48}, Detail, palette.deep);
    canvas.line({-62, 0}, {-30, 0}, Detail, palette.deep);
    canvas.line({50, 0}, {124, 0}, Detail, palette.deep);
    // Vents.
    for (int i = 0; i < 4; ++i) {
        const float x = 72.f + 10.f * static_cast<float>(i);
        canvas.line({x, -10}, {x + 6, -10}, Detail, accent);
        canvas.line({x, 10}, {x + 6, 10}, Detail, accent);
    }
    // The reactor in its heart, behind a turning ring.
    for (int i = 0; i < 6; ++i) {
        const float from = time * 1.4f + Pi / 3.f * static_cast<float>(i);
        canvas.arc({10, 0}, 24.f, from, from + 0.7f, Outline, accent, 4);
    }
    canvas.circle({10, 0}, 17.f, Detail, palette.deep, 24);
    canvas.halo({10, 0}, 34.f, withAlpha(looks::Danger, pulse(time, 3.f, 0.35f, 0.7f)));
    canvas.disc({10, 0}, 9.f, looks::Danger);
    canvas.disc({10, 0}, 4.f, looks::DangerHot);
    // Running lights along the hull, one after the other.
    for (int i = 0; i < 6; ++i) {
        const float x = -90.f + 32.f * static_cast<float>(i);
        const bool on = static_cast<int>(time * 6.f) % 6 == i;
        canvas.disc({x, -44.f + std::abs(x) * 0.02f}, on ? 2.6f : 1.6f, on ? looks::Light : palette.deep, 8);
        canvas.disc({x, 44.f - std::abs(x) * 0.02f}, on ? 2.6f : 1.6f, on ? looks::Light : palette.deep, 8);
    }
    // A turret on the back of the hull, sweeping toward the front.
    canvas.setTransform(place(center + V{-34, -50}, 1.f, 180.f + 22.f * std::sin(time * 0.8f)));
    canvas.shape({{0, -4}, {30, -3}, {30, 3}, {0, 4}}, looks::Black, Detail, accent);
    canvas.setTransform(place(center));
    canvas.shape({{-48, -48}, {-42, -60}, {-26, -60}, {-20, -48}}, looks::Black, Detail, accent);
    // Three guns at the bow, where its shots come from; each recoils as it fires.
    for (const float y : {-30.f, 0.f, 30.f}) {
        const float recoil = 6.f * std::max(0.f, std::sin(time * 5.f + y * 0.11f));
        canvas.shape({{-150 + recoil, y - 5}, {-104, y - 7}, {-104, y + 7}, {-150 + recoil, y + 5}}, looks::Black,
                     Detail, accent);
        canvas.line({-138 + recoil, y - 5}, {-138 + recoil, y + 5}, Detail, palette.deep);
        eye(canvas, {-152 + recoil, y}, 4.f, time + y);
    }
    wear(canvas, damage, time,
         {{{-80, -40}, {-70, -26}, {-78, -14}}, {{84, 40}, {92, 26}, {86, 18}}, {{30, -52}, {38, -40}}},
         {{-74, -22}, {92, 24}, {40, -44}}, palette);
}

// ---------------------------------------------------------------- enemies of Mars

void drawDustStriker(Canvas &canvas, V center, float time, const looks::Palette &palette)
{
    canvas.setTransform(place(center, 1.05f));
    canvas.halo({28, 0}, 16.f, withAlpha(palette.accent, pulse(time, 25.f, 0.5f, 0.8f)));
    canvas.shape({{-32, 0}, {-8, -14}, {14, -32}, {22, -27}, {12, -9}, {26, 0}, {12, 9}, {22, 27}, {14, 32}, {-8, 14}},
                 looks::Black, Outline, palette.accent);
    canvas.line({-14, -6}, {8, -18}, Detail, palette.deep);
    canvas.line({-14, 6}, {8, 18}, Detail, palette.deep);
    canvas.line({-4, 0}, {20, 0}, Detail, palette.deep);
    eye(canvas, {-18, -4}, 2.6f, time);
    eye(canvas, {-18, 4}, 2.6f, time + 0.3f);
}

void drawGravityDiver(Canvas &canvas, V center, float time, const looks::Palette &palette)
{
    const float flex = std::sin(time * 6.f) * 3.f;
    canvas.setTransform(place(center));
    canvas.halo({20, 0}, 12.f, withAlpha(palette.accent, 0.6f));
    canvas.shape({{-28, 0}, {2, -8}, {24, -18 - flex}, {14, 0}, {24, 18 + flex}, {2, 8}}, looks::Black, Outline,
                 palette.accent);
    canvas.line({-16, 0}, {12, 0}, Detail, palette.deep);
    eye(canvas, {-14, 0}, 2.8f, time);
}

void drawHopper(Canvas &canvas, V center, float time, const looks::Palette &palette)
{
    canvas.setTransform(place(center));
    const float crouch = std::sin(time * 3.f);
    // Legs: hip, knee, foot; the knees bend as it lands and jumps.
    for (int i = 0; i < 4; ++i) {
        const float side = i < 2 ? -1.f : 1.f;
        const float x = (i % 2 == 0 ? -1.f : 1.f) * 16.f;
        const V hip{x * 0.6f, side * 12.f};
        const V knee{x * 1.6f, side * (30.f - crouch * 6.f)};
        const V foot{x * 1.2f, side * (46.f + crouch * 2.f)};
        canvas.path({hip, knee, foot}, false, Outline, palette.accent);
        canvas.disc(knee, 3.f, palette.accent);
    }
    canvas.shape({{-30, 0}, {-18, -18}, {14, -20}, {30, -6}, {30, 6}, {14, 20}, {-18, 18}}, looks::Black, Outline,
                 palette.accent);
    canvas.line({-6, -19}, {-6, 19}, Detail, palette.deep);
    canvas.line({12, -20}, {12, 20}, Detail, palette.deep);
    canvas.shape({{-40, -2}, {-30, -10}, {-24, 0}, {-30, 10}}, looks::Black, Detail, palette.accent);
    eye(canvas, {-32, 0}, 3.f, time);
}

// The Excavator: a mining machine of Mars on its tracks, a turning drill at the front, a furnace in its belly.
void drawExcavator(Canvas &canvas, V center, float time, const looks::Palette &palette, float damage)
{
    canvas.setTransform(place(center));
    const sf::Color accent = palette.accent;
    // Smoke stacks, their smoke rising.
    for (const float x : {14.f, 40.f}) {
        for (int i = 0; i < 3; ++i) {
            const float rise = std::fmod(time * 0.8f + static_cast<float>(i) / 3.f + x * 0.01f, 1.f);
            canvas.circle({x + 18.f * rise, -104.f - 40.f * rise}, 5.f + 12.f * rise, Detail,
                          withAlpha(palette.deep, 1.f - rise), 12);
        }
        canvas.shape({{x - 7, -66}, {x + 7, -66}, {x + 6, -100}, {x - 6, -100}}, looks::Black, Detail, accent);
        canvas.line({x - 7, -92}, {x + 7, -92}, Detail, palette.deep);
        canvas.halo({x, -102}, 12.f, withAlpha(accent, pulse(time + x, 6.f, 0.3f, 0.8f)));
    }
    // Tracks: a long tread with turning wheels and moving links.
    const std::initializer_list<V> tread = {{-70, 60}, {124, 60}, {140, 80}, {124, 102}, {-70, 102}, {-84, 80}};
    canvas.shape(tread, looks::Black, Outline, accent);
    for (int i = 0; i < 16; ++i) {
        const float x = -70.f + std::fmod(194.f / 16.f * static_cast<float>(i) - time * 40.f + 194.f, 194.f);
        canvas.line({x, 60}, {x, 66}, Detail, palette.deep);
        canvas.line({x, 96}, {x, 102}, Detail, palette.deep);
    }
    for (int i = 0; i < 6; ++i) {
        const V wheel{-56.f + 34.f * static_cast<float>(i), 81.f};
        canvas.circle(wheel, 12.f, Detail, accent, 16);
        for (int s = 0; s < 3; ++s)
            canvas.line(wheel, wheel + polar(-time * 3.3f + 2.f * Pi / 3.f * static_cast<float>(s), 11.f), Detail,
                        palette.deep);
    }
    // Body, with its armour plates.
    canvas.shape({{-58, -54}, {0, -66}, {92, -66}, {124, -46}, {134, -8}, {134, 56}, {-58, 56}}, looks::Black, 3.2f,
                 accent);
    canvas.fill({{-48, -46}, {-4, -56}, {-4, -24}, {-48, -24}}, plate(palette));
    canvas.fill({{96, -10}, {126, -6}, {126, 48}, {96, 48}}, plate(palette));
    canvas.line({-4, -64}, {-4, 54}, Detail, palette.deep);
    canvas.line({92, -64}, {92, 54}, Detail, palette.deep);
    canvas.line({-56, 40}, {132, 40}, Detail, palette.deep);
    for (int i = 0; i < 9; ++i) // rivets
        canvas.disc({-44.f + 20.f * static_cast<float>(i), 48.f}, 1.8f, accent, 6);
    // The cab, its window and a turning beacon.
    canvas.shape({{60, -66}, {72, -90}, {110, -90}, {120, -66}}, looks::Black, Outline, accent);
    canvas.fill({{74, -84}, {106, -84}, {112, -72}, {70, -72}}, withAlpha(accent, 0.2f));
    canvas.line({76, -78}, {104, -78}, 2.f, looks::Light);
    const float beacon = std::sin(time * 6.f);
    canvas.disc({92, -94}, 4.f, accent, 10);
    canvas.halo({92 + 10.f * beacon, -96}, 16.f, withAlpha(accent, 0.3f + 0.4f * std::abs(beacon)));
    // The furnace, glowing red behind its grille: the heart of the machine.
    canvas.shape({{10, -30}, {80, -30}, {80, 26}, {10, 26}}, looks::Black, Outline, accent);
    canvas.halo({45, -2}, 46.f, withAlpha(looks::Danger, pulse(time, 2.6f, 0.3f, 0.65f)));
    canvas.fill({{16, -24}, {74, -24}, {74, 20}, {16, 20}}, withAlpha(looks::Danger, pulse(time, 2.6f, 0.25f, 0.45f)));
    canvas.disc({45, -2}, 7.f, looks::DangerHot);
    for (int i = 0; i < 6; ++i) {
        const float x = 20.f + 10.f * static_cast<float>(i);
        canvas.line({x, -24}, {x, 20}, 2.4f, looks::Black);
        canvas.line({x, -24}, {x, 20}, 1.f, palette.deep);
    }
    // The arm that pushes the drill: a piston pumping.
    const float push = 8.f * std::sin(time * 2.2f);
    canvas.line({-10, -60}, {-40 + push, -64}, 4.f, accent);
    canvas.line({-40 + push, -64}, {-62 + push * 0.5f, -50}, 2.6f, accent);
    canvas.disc({-10, -60}, 4.f, palette.deep, 10);
    canvas.circle({-40 + push, -64}, 4.5f, Detail, accent, 10);
    // The drill: a collar, then a cone whose grooves run toward the white-hot tip.
    canvas.shape({{-74, -58}, {-56, -58}, {-56, 58}, {-74, 58}}, looks::Black, Outline, accent);
    for (const float y : {-40.f, -20.f, 0.f, 20.f, 40.f})
        canvas.disc({-65, y}, 2.f, accent, 6);
    canvas.shape({{-152, 0}, {-74, -50}, {-74, 50}}, looks::Black, 3.2f, accent);
    for (int i = 0; i < 6; ++i) {
        const float t = std::fmod(static_cast<float>(i) / 6.f + time * 1.8f, 1.f);
        const float x = -74.f - 78.f * t;
        const float h = 50.f * (1.f - t);
        canvas.fill({{x, -h}, {x - 7.f * (1.f - t), -h * 0.6f}, {x - 13.f * (1.f - t), h}, {x - 6.f * (1.f - t), h}},
                    plate(palette, 0.6f));
        canvas.line({x + 8.f * (1.f - t), -h}, {x - 10.f * (1.f - t), h}, Detail, accent);
    }
    canvas.halo({-152, 0}, 22.f, withAlpha(looks::Light, pulse(time, 30.f, 0.35f, 0.8f)));
    for (int i = 0; i < 5; ++i) { // sparks thrown by the tip
        const float life = std::fmod(time * 2.4f + static_cast<float>(i) * 0.2f, 1.f);
        const float angle = Pi + (static_cast<float>(i) - 2.f) * 0.5f;
        const V from = V{-152, 0} + polar(angle, 6.f + 30.f * life);
        canvas.line(from, from + polar(angle, 6.f), 1.5f, withAlpha(looks::Light, 1.f - life));
    }
    // Gun ports on the collar, where its shots come from.
    for (const float y : {-30.f, 0.f, 30.f})
        eye(canvas, {-80, y}, 4.f, time + y);
    // Dust thrown up by the tracks.
    for (int i = 0; i < 3; ++i) {
        const float life = std::fmod(time * 1.2f + static_cast<float>(i) / 3.f, 1.f);
        canvas.circle({140.f + 30.f * life, 96.f - 12.f * life}, 4.f + 10.f * life, Detail,
                      withAlpha(palette.deep, 1.f - life), 10);
    }
    wear(canvas, damage, time,
         {{{-30, -54}, {-22, -40}, {-30, -30}}, {{100, -4}, {110, 12}, {104, 24}}, {{120, -40}, {128, -26}}},
         {{-24, -38}, {108, 14}, {60, -60}}, palette);
}

// ---------------------------------------------------------------- enemies of the Singularity

void drawShifter(Canvas &canvas, V center, float time, const looks::Palette &palette)
{
    canvas.setTransform(place(center));
    const float seen = static_cast<int>(time * 11.f) % 9 == 0 ? 0.35f : 1.f; // it flickers out of space
    const sf::Color line = withAlpha(palette.accent, seen);
    const auto outer = neon::regular(4, 24.f, time * 0.9f);
    const auto inner = neon::regular(4, 12.f, -time * 1.4f);
    canvas.shape(outer, looks::Black, Outline, line);
    canvas.path(inner, true, Detail, line);
    for (std::size_t i = 0; i < 4; ++i)
        canvas.line(outer[i], inner[i], Detail, withAlpha(palette.deep, seen));
    eye(canvas, {0, 0}, 3.6f, time);
}

void drawVortex(Canvas &canvas, V center, float time, const looks::Palette &palette)
{
    canvas.setTransform(place(center));
    for (int i = 0; i < 12; ++i) {
        const float from = time * 0.7f + Pi / 6.f * static_cast<float>(i);
        canvas.arc({0, 0}, 38.f, from, from + 0.25f, Detail, palette.deep, 3);
    }
    canvas.setTransform(place(center, 1.f, time * 220.f));
    for (int i = 0; i < 3; ++i) {
        const float a = 2.f * Pi / 3.f * static_cast<float>(i);
        canvas.shape({polar(a - 0.25f, 9.f), polar(a + 0.1f, 32.f), polar(a + 0.55f, 26.f), polar(a + 0.45f, 9.f)},
                     looks::Black, Outline, palette.accent);
    }
    canvas.disc({0, 0}, 11.f, looks::Black);
    canvas.circle({0, 0}, 11.f, Outline, palette.accent);
    canvas.setTransform(place(center));
    eye(canvas, {0, 0}, 4.5f, time);
}

void drawFragment(Canvas &canvas, V center, float time, const looks::Palette &palette)
{
    canvas.setTransform(place(center));
    canvas.halo({0, 0}, 40.f, withAlpha(looks::Danger, pulse(time, 3.f, 0.15f, 0.35f)));
    const std::array<float, 3> angles{-0.4f, 0.9f, 2.6f};
    for (std::size_t i = 0; i < angles.size(); ++i) {
        const float angle = angles[i] + std::sin(time * 1.1f + static_cast<float>(i)) * 0.12f;
        canvas.setTransform(place(center, 1.f, angle * 180.f / Pi));
        canvas.shape({{4, 0}, {14, -9}, {40, -7}, {48, 0}, {40, 7}, {14, 9}}, looks::Black, Outline, palette.accent);
        canvas.line({14, 0}, {44, 0}, Detail, palette.deep);
    }
    canvas.setTransform(place(center));
    canvas.shape(neon::regular(6, 12.f, time), looks::Black, Outline, palette.accent);
    eye(canvas, {0, 0}, 5.f, time);
}

// The Core: what sits at the centre of the Singularity. Rings that should not hold together, shards on their orbits,
// petals that open on its red eye.
void drawCore(Canvas &canvas, V center, float time, const looks::Palette &palette, float damage)
{
    canvas.setTransform(place(center));
    const sf::Color accent = palette.accent;
    // The outer accretion ring, dashed and turning, and a second one turning the other way.
    for (int i = 0; i < 32; ++i) {
        const float from = time * 0.35f + 2.f * Pi / 32.f * static_cast<float>(i);
        canvas.arc({0, 0}, 110.f, from, from + 0.11f, Outline, i % 4 == 0 ? looks::Light : palette.deep, 2);
    }
    for (int i = 0; i < 12; ++i) {
        const float from = -time * 0.6f + 2.f * Pi / 12.f * static_cast<float>(i);
        canvas.arc({0, 0}, 96.f, from, from + 0.3f, Detail, accent, 4);
    }
    // Shards on their orbits, pulled in and pushed out.
    for (int i = 0; i < 8; ++i) {
        const float a = time * 0.8f + Pi / 4.f * static_cast<float>(i);
        const float r = 78.f + 8.f * std::sin(time * 1.7f + static_cast<float>(i));
        canvas.setTransform(place(center + polar(a, r), 1.f, a * 180.f / Pi));
        canvas.shape({{-13, 0}, {-2, -6}, {13, 0}, {-2, 6}}, looks::Black, Detail, accent);
        canvas.line({-6, 0}, {6, 0}, 1.f, palette.deep);
    }
    // Rings tilted at different angles.
    for (int i = 0; i < 3; ++i) {
        canvas.setTransform(place(center, 1.f, 60.f * static_cast<float>(i) + time * 12.f));
        canvas.ellipse({0, 0}, {72.f, 24.f * std::sin(time * 0.9f + static_cast<float>(i))}, Detail, accent);
    }
    // Petals around the body, opening and closing on the eye.
    const float open = pulse(time, 0.9f, 0.f, 1.f);
    for (int i = 0; i < 6; ++i) {
        canvas.setTransform(place(center, 1.f, 60.f * static_cast<float>(i) + time * 8.f));
        const float reach = 62.f + 14.f * open;
        canvas.shape({{40, -12}, {reach, -6 - 6.f * open}, {reach + 8.f, 0}, {reach, 6 + 6.f * open}, {40, 12}},
                     plate(palette, 0.5f), Detail, accent);
    }
    canvas.setTransform(place(center));
    canvas.disc({0, 0}, 54.f, looks::Black, 36);
    canvas.circle({0, 0}, 54.f, 3.4f, accent, 44);
    canvas.circle({0, 0}, 40.f, Detail, palette.deep, 36);
    for (int i = 0; i < 12; ++i) { // a lattice inside the shell
        const float a = 2.f * Pi / 12.f * static_cast<float>(i) - time * 0.3f;
        canvas.line(polar(a, 40.f), polar(a + 0.5f, 54.f), 1.f, palette.deep);
    }
    // The eye looks around, its lid narrowing as the petals close.
    const V look = polar(time * 0.6f, 9.f);
    canvas.halo(look, 48.f, withAlpha(looks::Danger, pulse(time, 4.f, 0.4f, 0.75f)));
    canvas.disc(look, 18.f, looks::Danger);
    canvas.disc(look, 7.f, looks::DangerHot);
    canvas.arc(look, 26.f, Pi * (1.1f + 0.15f * open), Pi * (1.9f - 0.15f * open), Detail, accent, 10);
    canvas.arc(look, 26.f, Pi * (0.1f + 0.15f * open), Pi * (0.9f - 0.15f * open), Detail, accent, 10);
    // Gun ports at its front, where its shots come from.
    for (const float y : {-30.f, 0.f, 30.f})
        eye(canvas, {-62.f + std::abs(y) * 0.35f, y}, 3.2f, time + y);
    wear(canvas, damage, time, {{{-30, -44}, {-20, -32}, {-28, -22}}, {{30, 40}, {38, 28}, {32, 18}}},
         {{-24, -30}, {34, 26}, {0, -50}}, palette);
}

void drawEnemy(Canvas &canvas, const EntityState &enemy, float time, float damage)
{
    const float local = time + static_cast<float>(enemy.netId % 11) * 0.23f;
    const auto &palette = looks::worldPalette(looks::worldOfEnemy(enemy.variant));
    if (looks::isBoss(enemy.variant)) {
        const V center{enemy.x + looks::BossCenterX, enemy.y + looks::BossCenterY};
        switch (enemy.variant) {
        case looks::Carrier:
        case looks::HeavyBoss:
            drawCarrier(canvas, center, local, palette, damage);
            break;
        case looks::Core:
            drawCore(canvas, center, local, palette, damage);
            break;
        default:
            drawExcavator(canvas, center, local, palette, damage);
            break;
        }
        return;
    }
    switch (enemy.variant) {
    case looks::Scout:
    case looks::Patrol:
        drawScout(canvas, {enemy.x + 32.f, enemy.y + 28.f}, local, palette);
        break;
    case looks::Probe:
    case looks::Wave:
        drawProbe(canvas, {enemy.x + 32.f, enemy.y + 28.f}, local, palette);
        break;
    case looks::Relay:
        drawRelay(canvas, {enemy.x + 28.f, enemy.y + 23.f}, local, palette);
        break;
    case looks::DustStriker:
    case looks::Striker:
        drawDustStriker(canvas, {enemy.x + 33.f, enemy.y + 36.f}, local, palette);
        break;
    case looks::GravityDiver:
    case looks::Diver:
        drawGravityDiver(canvas, {enemy.x + 28.f, enemy.y + 23.f}, local, palette);
        break;
    case looks::Hopper:
    case looks::OldHopper:
        drawHopper(canvas, {enemy.x + 45.f, enemy.y + 50.f}, local, palette);
        break;
    case looks::Shifter:
        drawShifter(canvas, {enemy.x + 32.f, enemy.y + 28.f}, local, palette);
        break;
    case looks::Fragment:
        drawFragment(canvas, {enemy.x + 36.f, enemy.y + 42.f}, local, palette);
        break;
    default:
        drawVortex(canvas, {enemy.x + 36.f, enemy.y + 42.f}, local, palette);
        break;
    }
}

} // namespace

sf::Vector2f EntityRenderer::enginePosition(const EntityState &ship)
{
    return {ship.x - 4.f, ship.y + 15.f};
}

void EntityRenderer::draw(sf::RenderTarget &window, const EntityState &entity, float time, float tilt,
                          std::optional<std::uint8_t> localPlayer, sf::RenderTarget *glow, float flash,
                          float damage) const
{
    {
        Canvas canvas(window, glow);
        canvas.setFlash(flash);
        switch (entity.type) {
        case EntityType::Player:
            drawShip(canvas, entity, time, tilt);
            break;
        case EntityType::PlayerMissile:
            canvas.setGlowAmount(0.9f);
            drawPlayerShot(canvas, entity, time);
            break;
        case EntityType::EnemyMissile:
            canvas.setGlowAmount(0.9f);
            drawEnemyShot(canvas, entity, time);
            break;
        case EntityType::Bonus:
            canvas.setGlowAmount(0.8f);
            drawBonus(canvas, entity, time);
            break;
        case EntityType::Obstacle:
            canvas.setGlowAmount(0.35f);
            drawObstacle(canvas, entity, time);
            break;
        case EntityType::Enemy:
            canvas.setGlowAmount(0.5f);
            drawEnemy(canvas, entity, time, damage);
            break;
        }
    }
    if (entity.type == EntityType::Player)
        drawPlayerTag(window, entity, localPlayer);
}

// "P1", "P2 (YOU)"... above the ship, in the player's color.
void EntityRenderer::drawPlayerTag(sf::RenderTarget &window, const EntityState &entity,
                                   std::optional<std::uint8_t> localPlayer) const
{
    if (!_resources.hasFont("default"))
        return;
    std::string tag = "P" + std::to_string(entity.variant + 1);
    if (localPlayer == entity.variant)
        tag += " (YOU)";
    sf::Text text(_resources.getFont("default"), tag, 15);
    text.setFillColor(looks::playerColor(entity.variant));
    const auto bounds = text.getLocalBounds();
    text.setOrigin({bounds.position.x + bounds.size.x / 2.f, bounds.position.y + bounds.size.y});
    text.setPosition({entity.x + 28.f, entity.y - 10.f});
    window.draw(text);
}

void EntityRenderer::drawExplosion(sf::RenderTarget &window, const Explosion &explosion, sf::RenderTarget *glow) const
{
    const float progress = std::clamp(explosion.timer / explosion.maxDuration, 0.f, 1.f);
    Canvas canvas(window, glow, 0.9f);
    canvas.setTransform(place({explosion.position.x + 30.f, explosion.position.y + 28.f}));
    canvas.circle({0, 0}, 10.f + 50.f * progress, 3.f * (1.f - progress) + 0.5f,
                  withAlpha(looks::Light, 1.f - progress), 32);
}

} // namespace rtype::client
