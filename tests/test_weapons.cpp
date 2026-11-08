#include "Game.hpp"
#include "Weapons.hpp"

#include <gtest/gtest.h>

#include <algorithm>

using namespace rtype;
using namespace rtype::server;

namespace {

long countOf(Game &game, protocol::EntityType type)
{
    const auto snap = game.snapshot();
    return std::count_if(snap.entities.begin(), snap.entities.end(),
                         [type](const auto &entity) { return entity.type == type; });
}

} // namespace

TEST(Weapons, ConfigurationNamesExistingMissileAndBonusPrefabs)
{
    const Arsenal arsenal = loadArsenal("assets/config/weapons.json");
    EXPECT_EQ(arsenal.defaultWeapon, "blaster");
    EXPECT_EQ(arsenal.weapon("spread").angles.size(), 3u);
    EXPECT_EQ(arsenal.weapon("laser").missile, "laser_missile");
    EXPECT_EQ(&arsenal.weapon("unknown"), &arsenal.weapon("blaster"));

    Prefabs prefabs;
    registerGameComponents(prefabs);
    loadPrefabs(prefabs, "assets/config/entities.json");
    for (const auto &[name, weapon] : arsenal.weapons)
        EXPECT_TRUE(prefabs.contains(weapon.missile)) << name;
    for (const auto &bonus : arsenal.dropPrefabs)
        EXPECT_TRUE(prefabs.contains(bonus)) << bonus;
}

TEST(Weapons, MissingFileKeepsTheDefaultBlaster)
{
    const Arsenal arsenal = loadArsenal("does/not/exist.json");
    EXPECT_EQ(arsenal.weapon("blaster").angles.size(), 1u);
    EXPECT_TRUE(arsenal.dropPrefabs.empty());
}

TEST(Weapons, OneShotFiresOneMissilePerAngle)
{
    Game game;
    game.rules().lobbyDelay = 0.f; // no waiting room in rule tests
    game.arsenal().weapons["blaster"].angles = {-10.f, 0.f, 10.f};
    game.onPlayerJoined(0);
    game.onInput(0, protocol::InputFlag::Shoot);
    EXPECT_EQ(countOf(game, protocol::EntityType::PlayerMissile), 3);
}

TEST(Weapons, DestroyedEnemyDropsABonusThatChangesTheWeapon)
{
    Game game;
    game.rules().lobbyDelay = 0.f; // no waiting room in rule tests
    game.loadEntitiesConfig("tests/data/entities.json");
    game.loadLevel("tests/data/one_grunt.json");
    game.arsenal().dropChance = 1.f;
    game.arsenal().dropPrefabs = {"bonus_laser"};
    game.onPlayerJoined(0); // ship at y = 300, level with the enemy
    game.onInput(0, protocol::InputFlag::Shoot);
    game.onInput(0, 0);

    bool bonusSeen = false;
    for (int tick = 0; tick < 60 * 20 && game.playerWeapon(0) != "laser"; tick++) {
        game.update(1.f / 60.f);
        bonusSeen = bonusSeen || countOf(game, protocol::EntityType::Bonus) > 0;
    }
    EXPECT_TRUE(bonusSeen);
    EXPECT_EQ(game.playerWeapon(0), "laser");
    EXPECT_EQ(countOf(game, protocol::EntityType::Bonus), 0);
}
