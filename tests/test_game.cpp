#include "Game.hpp"
#include "EnemyShootSystem.hpp"
#include "common/Components.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>

using namespace rtype;
using namespace rtype::server;

namespace {

// Enemies appear at a random height and the configured ones move and shoot, so these tests use a level whose
// enemy type is unknown to the game: it flies straight, never shoots and has the default 3 hit points.
std::string writeStraightEnemyLevel()
{
    const auto path = std::filesystem::temp_directory_path() / "rtype_test_straight_enemy_level.json";
    std::ofstream(path) << R"({"stage": 1, "level_name": "Test", "waves": [{"wave_id": 1, "delay_before_start": 0.0,
        "enemies": [{"type": "test_straight", "spawn_y": 300.0, "speed": 100.0}]}]})";
    return path.string();
}

std::size_t countOfType(const protocol::Snapshot &snapshot, protocol::EntityType type)
{
    return static_cast<std::size_t>(
        std::count_if(snapshot.entities.begin(), snapshot.entities.end(),
                      [type](const protocol::EntityState &entity) { return entity.type == type; }));
}

// Spawns the level's enemy, then flies the ship of `player` to its height so it is hit or can shoot it.
void alignShipWithEnemy(Game &game, PlayerId player)
{
    game.update(1.f / 60.f);
    for (int tick = 0; tick < 600; ++tick) {
        const auto snapshot = game.snapshot();
        float shipY = 0.f;
        float enemyY = 0.f;
        for (const auto &entity : snapshot.entities) {
            if (entity.type == protocol::EntityType::Player && entity.variant == player)
                shipY = entity.y;
            else if (entity.type == protocol::EntityType::Enemy)
                enemyY = entity.y;
        }
        if (std::abs(shipY - enemyY) < 10.f) {
            game.onInput(player, 0);
            return;
        }
        game.onInput(player, shipY > enemyY ? protocol::InputFlag::Up : protocol::InputFlag::Down);
        game.update(1.f / 60.f);
    }
}

} // namespace

TEST(Game, PlayerJoinAppearsInSnapshot)
{
    Game game;
    game.onPlayerJoined(0);
    const auto snap = game.snapshot();
    ASSERT_EQ(snap.entities.size(), 1u);
    EXPECT_EQ(snap.entities[0].type, protocol::EntityType::Player);
}

TEST(Game, InputMovesPlayer)
{
    Game game;
    game.onPlayerJoined(1);
    const auto before = game.snapshot();
    ASSERT_EQ(before.entities.size(), 1u);
    game.onInput(1, protocol::InputFlag::Right);
    game.update(0.1f);
    const auto after = game.snapshot();
    ASSERT_EQ(after.entities.size(), 1u);
    EXPECT_GT(after.entities[0].x, before.entities[0].x);
}

TEST(Game, PlayerLeftProducesDestroyEvent)
{
    Game game;
    game.onPlayerJoined(2);
    game.onPlayerLeft(2);
    EXPECT_TRUE(game.snapshot().entities.empty());
    const auto events = game.takeEvents();
    ASSERT_EQ(events.size(), 1u);
    EXPECT_TRUE(std::holds_alternative<protocol::EntityDestroyed>(events[0]));
}

TEST(Game, RejoiningPlayerGetsANewNetworkIdentityAndFreshMovement)
{
    Game game;
    game.onPlayerJoined(2);
    const auto initial = game.snapshot();
    ASSERT_EQ(initial.entities.size(), 1u);
    const auto previousNetworkId = initial.entities[0].netId;

    game.onInput(2, protocol::InputFlag::Right);
    game.update(0.25f);
    game.onPlayerLeft(2);
    const auto events = game.takeEvents();
    ASSERT_EQ(events.size(), 1u);
    const auto *destroyed = std::get_if<protocol::EntityDestroyed>(&events[0]);
    ASSERT_NE(destroyed, nullptr);
    EXPECT_EQ(destroyed->netId, previousNetworkId);

    // Reusing a player slot must not resurrect its old network object or velocity.
    game.onPlayerJoined(2);
    game.update(0.25f);
    const auto rejoined = game.snapshot();
    ASSERT_EQ(rejoined.entities.size(), 1u);
    const auto &ship = rejoined.entities[0];
    EXPECT_NE(ship.netId, previousNetworkId);
    EXPECT_EQ(ship.type, protocol::EntityType::Player);
    EXPECT_EQ(ship.variant, 2);
    EXPECT_FLOAT_EQ(ship.x, initial.entities[0].x);
    EXPECT_FLOAT_EQ(ship.y, initial.entities[0].y);
}

TEST(Game, HoldingShootKeepsSnapshotsWithinDatagramLimit)
{
    Game game;
    game.onPlayerJoined(0);

    for (int tick = 0; tick < 600; ++tick) {
        game.onInput(0, protocol::InputFlag::Shoot);
        game.update(1.f / 60.f);
    }

    EXPECT_LE(game.snapshot().entities.size(), protocol::MaxEntitiesPerSnapshot);
}

TEST(Game, EnemiesAppearOnlyWhilePlayersAreConnected)
{
    Game game;
    for (int tick = 0; tick < 240; tick++)
        game.update(1.f / 60.f);
    EXPECT_TRUE(game.snapshot().entities.empty());

    game.onPlayerJoined(0);
    for (int tick = 0; tick < 240; tick++)
        game.update(1.f / 60.f);
    const auto snap = game.snapshot();
    const auto enemyCount = std::count_if(snap.entities.begin(), snap.entities.end(), [](const auto &entity) {
        return entity.type == protocol::EntityType::Enemy;
    });
    EXPECT_EQ(enemyCount, 1);
}

TEST(Game, DeadPlayerCannotControlAReusedEntity)
{
    Game game;
    game.loadLevel(writeStraightEnemyLevel());
    game.onPlayerJoined(0);
    alignShipWithEnemy(game, 0);
    // The enemy flies on and reaches the stationary ship.
    for (int step = 0; step < 200 && countOfType(game.snapshot(), protocol::EntityType::Player) > 0; ++step)
        game.update(0.25f);

    const auto afterCollision = game.snapshot();
    EXPECT_EQ(countOfType(afterCollision, protocol::EntityType::Player), 0u);
    EXPECT_NO_THROW(game.onInput(0, protocol::InputFlag::Right));

    game.onPlayerJoined(1);
    game.onPlayerJoined(2); // Both freed local ids are now reused.
    EXPECT_NO_THROW(game.onInput(0, protocol::InputFlag::Right));
    game.update(0.1f);
    const auto afterInput = game.snapshot();
    int playersFound = 0;
    for (const auto &entity : afterInput.entities) {
        if (entity.type == protocol::EntityType::Player) {
            playersFound++;
            EXPECT_FLOAT_EQ(entity.x, 100.f);
        }
    }
    EXPECT_EQ(playersFound, 2);
}

TEST(Game, PlayerMissileDestroysEnemyAndScores)
{
    Game game;
    game.loadLevel(writeStraightEnemyLevel());
    game.onPlayerJoined(0);
    alignShipWithEnemy(game, 0);
    // The enemy has 3 hit points: hold the trigger until it is destroyed.
    for (int tick = 0; tick < 600 && countOfType(game.snapshot(), protocol::EntityType::Enemy) > 0; ++tick) {
        game.onInput(0, protocol::InputFlag::Shoot);
        game.update(1.f / 60.f);
    }

    EXPECT_EQ(countOfType(game.snapshot(), protocol::EntityType::Enemy), 0u);
    EXPECT_EQ(game.snapshot().score, 100u);
    const auto events = game.takeEvents();
    const auto killed = std::count_if(events.begin(), events.end(), [](const protocol::Message &message) {
        const auto *destroyed = std::get_if<protocol::EntityDestroyed>(&message);
        return destroyed && destroyed->reason == protocol::DestroyReason::Killed;
    });
    EXPECT_GE(killed, 2); // at least the missile and the enemy
}

TEST(Game, EnemyZigzagShootingAppliesMovementPattern)
{
    engine::ecs::Registry registry;
    auto enemy = registry.spawn();
    registry.emplace<rtype::components::Position>(enemy, 500.f, 200.f);
    registry.emplace<rtype::components::Kind>(enemy, protocol::EntityType::Enemy, 0);
    registry.emplace<rtype::components::EnemyAttack>(enemy, 0.f, 0.5f, 300.f, 0, "zigzag");

    engine::ecs::Entity spawnedBullet = engine::ecs::NullEntity;
    updateEnemyShooting(registry, [&](protocol::EntityType type, std::uint8_t variant, float x, float y) {
        spawnedBullet = registry.spawn();
        registry.emplace<rtype::components::Kind>(spawnedBullet, type, variant);
        registry.emplace<rtype::components::Position>(spawnedBullet, x, y);
        registry.emplace<rtype::components::Velocity>(spawnedBullet, 0.f, 0.f);
        return spawnedBullet;
    }, 0.6f);

    ASSERT_NE(spawnedBullet, engine::ecs::NullEntity);
    const auto *pattern = registry.get<rtype::components::MovementPattern>(spawnedBullet);
    ASSERT_NE(pattern, nullptr);
    EXPECT_EQ(pattern->type, "zigzag");

    const auto *vel = registry.get<rtype::components::Velocity>(spawnedBullet);
    ASSERT_NE(vel, nullptr);
    EXPECT_NE(vel->y, 0.f);
}

TEST(Game, GameRulesLoadedCorrectlyForGameDesigner)
{
    Game game;
    const auto &rules = game.rules();
    EXPECT_GE(rules.campaignLevels.size(), 3u);
    EXPECT_GT(rules.infinite.maxBulletSpeed, 0.f);
    EXPECT_GT(rules.infinite.healthGrowthPerLoop, 0.f);
    EXPECT_GT(rules.infinite.scoreMultiplierPerLoop, 0.f);
    EXPECT_EQ(rules.difficulties.at(Game::Difficulty::Normal).healthMultiplier, 1.0f);
    EXPECT_LT(rules.difficulties.at(Game::Difficulty::Easy).healthMultiplier, 1.0f);
    EXPECT_GT(rules.difficulties.at(Game::Difficulty::Hard).healthMultiplier, 1.0f);
}

TEST(Game, DifficultySettingsChangeGameModeAndDifficulty)
{
    Game game;
    EXPECT_EQ(game.gameMode(), Game::GameMode::Campaign);
    EXPECT_EQ(game.difficulty(), Game::Difficulty::Normal);

    game.setGameMode(Game::GameMode::Infinite);
    EXPECT_EQ(game.gameMode(), Game::GameMode::Infinite);

    game.setDifficulty(Game::Difficulty::Hard);
    EXPECT_EQ(game.difficulty(), Game::Difficulty::Hard);
}

TEST(Game, CampaignProgressionAdvancesLevelsAndFinishes)
{
    Game game;
    game.setGameMode(Game::GameMode::Campaign);
    game.rules().intermissionDelay = 0.0f;
    game.onPlayerJoined(0);

    // Move player safely out of the horizontal line of enemy fire
    for (int tick = 0; tick < 120; ++tick) {
        game.onInput(0, protocol::InputFlag::Up);
        game.update(1.f / 60.f);
    }

    EXPECT_EQ(game.currentLevelIndex(), 0u);
    EXPECT_EQ(game.gameState(), protocol::GameState::Playing);

    // Fast-forward until level 1 finishes (all waves spawn and enemies despawn out of bounds)
    for (int tick = 0; tick < 3600; ++tick) {
        game.update(0.1f);
        if (game.currentLevelIndex() == 1u)
            break;
    }
    EXPECT_EQ(game.currentLevelIndex(), 1u);

    // Fast-forward level 2
    for (int tick = 0; tick < 3600; ++tick) {
        game.update(0.1f);
        if (game.currentLevelIndex() == 2u)
            break;
    }
    EXPECT_EQ(game.currentLevelIndex(), 2u);

    // Fast-forward level 3 (Boss stage)
    for (int tick = 0; tick < 3600; ++tick) {
        game.update(0.1f);
        if (game.gameState() == protocol::GameState::Finished)
            break;
    }
    EXPECT_EQ(game.gameState(), protocol::GameState::Finished);
}

TEST(Game, InfiniteModeLoopsAndIncrementsLoopCount)
{
    Game game;
    game.setGameMode(Game::GameMode::Infinite);
    game.rules().intermissionDelay = 0.0f;
    game.onPlayerJoined(0);

    // Move player safely out of the horizontal line of enemy fire
    for (int tick = 0; tick < 120; ++tick) {
        game.onInput(0, protocol::InputFlag::Up);
        game.update(1.f / 60.f);
    }

    EXPECT_EQ(game.infiniteLoopCount(), 1u);
    EXPECT_EQ(game.currentLevelIndex(), 0u);

    // Fast-forward all 3 levels
    for (int level = 0; level < 3; ++level) {
        for (int tick = 0; tick < 3600; ++tick) {
            game.update(0.1f);
            if (level < 2 && game.currentLevelIndex() == static_cast<std::size_t>(level + 1))
                break;
            if (level == 2 && game.infiniteLoopCount() == 2u)
                break;
        }
    }

    // Now in loop 2, back to stage 0, and game is still Playing!
    EXPECT_EQ(game.infiniteLoopCount(), 2u);
    EXPECT_EQ(game.currentLevelIndex(), 0u);
    EXPECT_EQ(game.gameState(), protocol::GameState::Playing);
}

TEST(Game, EnemyMultipleAttacksShootSimultaneously)
{
    engine::ecs::Registry registry;
    auto enemy = registry.spawn();
    registry.emplace<rtype::components::Position>(enemy, 500.f, 200.f);
    registry.emplace<rtype::components::Kind>(enemy, protocol::EntityType::Enemy, 0);

    std::vector<rtype::components::AttackEntry> attacks = {
        {0.f, 0.5f, 400.f, 1, "straight", 0.f},
        {0.f, 0.5f, 250.f, 0, "zigzag", 30.f}
    };
    registry.emplace<rtype::components::EnemyAttack>(enemy, rtype::components::EnemyAttack{attacks});

    std::vector<engine::ecs::Entity> spawnedBullets;
    updateEnemyShooting(registry, [&](protocol::EntityType type, std::uint8_t variant, float x, float y) {
        auto bullet = registry.spawn();
        registry.emplace<rtype::components::Kind>(bullet, type, variant);
        registry.emplace<rtype::components::Position>(bullet, x, y);
        registry.emplace<rtype::components::Velocity>(bullet, 0.f, 0.f);
        spawnedBullets.push_back(bullet);
        return bullet;
    }, 0.6f);

    ASSERT_EQ(spawnedBullets.size(), 2u);

    // Bullet 1: straight, offset 0, variant 1
    const auto *pos1 = registry.get<rtype::components::Position>(spawnedBullets[0]);
    const auto *kind1 = registry.get<rtype::components::Kind>(spawnedBullets[0]);
    ASSERT_NE(pos1, nullptr);
    ASSERT_NE(kind1, nullptr);
    EXPECT_FLOAT_EQ(pos1->y, 200.f);
    EXPECT_EQ(kind1->variant, 1);

    // Bullet 2: zigzag, offset +30, variant 0
    const auto *pos2 = registry.get<rtype::components::Position>(spawnedBullets[1]);
    const auto *kind2 = registry.get<rtype::components::Kind>(spawnedBullets[1]);
    const auto *pattern2 = registry.get<rtype::components::MovementPattern>(spawnedBullets[1]);
    ASSERT_NE(pos2, nullptr);
    ASSERT_NE(kind2, nullptr);
    ASSERT_NE(pattern2, nullptr);
    EXPECT_FLOAT_EQ(pos2->y, 230.f);
    EXPECT_EQ(kind2->variant, 0);
    EXPECT_EQ(pattern2->type, "zigzag");
}



