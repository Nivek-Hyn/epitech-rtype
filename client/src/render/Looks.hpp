#pragma once

#include <SFML/Graphics/Color.hpp>

#include <array>
#include <cstdint>

namespace rtype::client::looks {

// The variant of an enemy (Kind in assets/config/entities.json) only chooses how it looks. Each world has its own
// enemies; 0 to 7 are the first enemies of the project, still used by the test level and the level editor.
enum Enemy : std::uint8_t {
    Striker = 0,
    Patrol = 1,
    Wave = 2,
    Diver = 3,
    Tentacle = 4,
    HeavyBoss = 5,
    OldBoss = 6,
    OldHopper = 7,
    // World 1, Orbital belt: satellites and stations.
    Scout = 10,
    Probe = 11,
    Relay = 12,
    // World 2, Mars: mining machines.
    DustStriker = 13,
    GravityDiver = 14,
    Hopper = 15,
    // World 3, Singularity: shapes that should not exist.
    Shifter = 16,
    Vortex = 17,
    Fragment = 18,
    // Bosses, one per world.
    Carrier = 30,
    Excavator = 31,
    Core = 32,
};

inline bool isBoss(std::uint8_t variant)
{
    return variant == HeavyBoss || variant == OldBoss || variant >= Carrier;
}

inline const char *bossName(std::uint8_t variant)
{
    switch (variant) {
    case Carrier:
        return "ORBITAL CARRIER";
    case Excavator:
        return "DEEP EXCAVATOR";
    case Core:
        return "SINGULARITY CORE";
    default:
        return "BATTLESHIP";
    }
}

// Where a boss is drawn, relative to its position (its hitbox is 280 x 222, raised by 60).
constexpr float BossCenterX = 140.f;
constexpr float BossCenterY = 51.f;

// The colours: black and white light everywhere, one colour per world, red only for what hurts.
struct Palette {
    sf::Color accent; // the world's colour: enemies, the edges of the world
    sf::Color deep;   // the same colour, darker: details, far layers
};

inline const Palette &worldPalette(std::size_t world)
{
    static const std::array<Palette, 3> palettes{{
        {{70, 150, 255}, {22, 52, 110}}, // Orbital belt: blue
        {{255, 112, 46}, {110, 40, 14}}, // Mars: orange
        {{176, 92, 255}, {64, 28, 112}}, // Singularity: violet
    }};
    return palettes[world % palettes.size()];
}

inline std::size_t worldOfEnemy(std::uint8_t variant)
{
    switch (variant) {
    case Scout:
    case Probe:
    case Relay:
    case Carrier:
    case Patrol:
    case Wave:
    case HeavyBoss:
        return 0;
    case DustStriker:
    case GravityDiver:
    case Hopper:
    case Excavator:
    case Striker:
    case Diver:
    case OldHopper:
    case OldBoss:
        return 1;
    default:
        return 2;
    }
}

const sf::Color Danger{255, 52, 72};      // enemy cores and shots
const sf::Color DangerHot{255, 214, 200}; // the white-hot centre of an enemy shot
const sf::Color Light{238, 244, 255};     // white light: hulls of the players, shots, text
const sf::Color Black{0, 0, 0};

inline sf::Color playerColor(std::uint8_t player)
{
    static const std::array<sf::Color, 4> colors{sf::Color(0, 230, 255), sf::Color(255, 176, 40),
                                                 sf::Color(80, 255, 120), sf::Color(240, 110, 255)};
    return colors[player % colors.size()];
}

} // namespace rtype::client::looks
