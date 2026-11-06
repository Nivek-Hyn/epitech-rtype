#include "Game.hpp"
#include "EnemyShootSystem.hpp"
#include "MovementSystem.hpp"
#include "common/Components.hpp"

#include <gtest/gtest.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>

using namespace rtype;
using namespace rtype::server;

namespace {

// Rule tests use small dedicated levels, so that balancing the campaign does not break them.
constexpr const char *TestEntities = "tests/data/entities.json";
constexpr const char *OneGrunt = "tests/data/one_grunt.json";
constexpr const char *OneBoss = "tests/data/one_boss.json";
constexpr const char *OneSprite = "tests/data/one_sprite.json";

void run(Game &game, float seconds)
{
    for (int tick = 0; tick < static_cast<int>(seconds * 60.f); ++tick)
        game.update(1.f / 60.f);
}

long countOf(Game &game, protocol::EntityType type)
{
    const auto snap = game.snapshot();
    return std::count_if(snap.entities.begin(), snap.entities.end(),
                         [type](const auto &entity) { return entity.type == type; });
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
    game.loadEntitiesConfig(TestEntities);
    game.loadLevel(OneGrunt);
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
    game.loadEntitiesConfig(TestEntities);
    game.loadLevel(OneGrunt);
    game.rules().intermissionDelay = 1000.f; // the game must still be running when the others join
    game.onPlayerJoined(0);
    game.update(3.f);
    game.update(12.f); // The first configured enemy reaches the stationary player.

    const auto afterCollision = game.snapshot();
    EXPECT_EQ(std::count_if(afterCollision.entities.begin(), afterCollision.entities.end(),
                            [](const auto &entity) { return entity.type == protocol::EntityType::Player; }),
              0);
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
    game.loadEntitiesConfig(TestEntities);
    game.loadLevel(OneGrunt);
    game.setGameMode(Game::GameMode::Infinite);
    game.onPlayerJoined(0); // ship at y = 300, level with the first enemy
    game.onInput(0, protocol::InputFlag::Shoot);
    game.onInput(0, 0);
    for (int tick = 0; tick < 300; tick++)
        game.update(1.f / 60.f);

    EXPECT_EQ(game.snapshot().score, 100u);
    const auto events = game.takeEvents();
    const auto killed = std::count_if(events.begin(), events.end(), [](const protocol::Message &message) {
        const auto *destroyed = std::get_if<protocol::EntityDestroyed>(&message);
        return destroyed && destroyed->reason == protocol::DestroyReason::Killed;
    });
    EXPECT_EQ(killed, 2); // the missile and the enemy
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
    game.loadCampaign({"assets/levels/level1.json", "assets/levels/level2.json", "assets/levels/level3.json"});
    game.setGameMode(Game::GameMode::Campaign);
    game.loadEntitiesConfig(TestEntities);
    game.loadCampaign({OneGrunt, OneGrunt, OneGrunt});
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

    // Fast-forward level 3 (last stage)
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
    game.loadCampaign({"assets/levels/level1.json", "assets/levels/level2.json", "assets/levels/level3.json"});
    game.setGameMode(Game::GameMode::Infinite);
    game.loadEntitiesConfig(TestEntities);
    game.loadCampaign({OneGrunt, OneGrunt, OneGrunt});
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
    // (the patterns of worlds 2 and 3 are tested at the end of this file)
    ASSERT_NE(pos2, nullptr);
    ASSERT_NE(kind2, nullptr);
    ASSERT_NE(pattern2, nullptr);
    EXPECT_FLOAT_EQ(pos2->y, 230.f);
    EXPECT_EQ(kind2->variant, 0);
    EXPECT_EQ(pattern2->type, "zigzag");
}

TEST(Game, BossStopsAtItsPositionAndTheLevelWaitsForItsDeath)
{
    Game game;
    game.loadEntitiesConfig(TestEntities);
    game.loadLevel(OneBoss);
    game.rules().intermissionDelay = 0.0f;
    game.onPlayerJoined(0);
    run(game, 60.f);

    const auto snap = game.snapshot();
    const auto boss = std::find_if(snap.entities.begin(), snap.entities.end(), [](const auto &entity) {
        return entity.type == protocol::EntityType::Enemy;
    });
    ASSERT_NE(boss, snap.entities.end());
    EXPECT_NEAR(boss->x, 300.f, 10.f);
    EXPECT_EQ(game.gameState(), protocol::GameState::Playing);
}

TEST(Game, RammingKillsTheShipButNotTheBoss)
{
    Game game;
    game.loadEntitiesConfig(TestEntities);
    game.loadLevel(OneBoss);
    game.onPlayerJoined(0);
    game.onPlayerJoined(1); // stays alive, so that the level does not restart
    run(game, 5.f);

    for (int tick = 0; tick < 60; ++tick) {
        game.onInput(0, protocol::InputFlag::Right);
        game.update(1.f / 60.f);
    }
    EXPECT_EQ(countOf(game, protocol::EntityType::Player), 1);
    EXPECT_EQ(countOf(game, protocol::EntityType::Enemy), 1);
}

TEST(Game, CampaignGivesNoScore)
{
    Game game;
    game.loadEntitiesConfig(TestEntities);
    game.loadLevel(OneGrunt);
    game.setGameMode(Game::GameMode::Campaign);
    game.onPlayerJoined(0);
    game.onInput(0, protocol::InputFlag::Shoot);
    game.onInput(0, 0);
    run(game, 5.f);

    EXPECT_EQ(countOf(game, protocol::EntityType::Enemy), 0); // it was shot down...
    EXPECT_EQ(countOf(game, protocol::EntityType::Player), 1);
    EXPECT_EQ(game.snapshot().score, 0u); // ...without any points
}

TEST(Game, ScoreOfAnEnemyComesFromItsDefinition)
{
    Game game;
    game.loadEntitiesConfig(TestEntities);
    game.loadLevel(OneBoss);
    game.setGameMode(Game::GameMode::Infinite);
    game.rules().intermissionDelay = 100.f; // the boss does not come back during the test
    game.onPlayerJoined(0);
    for (int tick = 0; tick < 300; ++tick) {
        game.onInput(0, protocol::InputFlag::Shoot);
        game.update(1.f / 60.f);
    }
    EXPECT_EQ(countOf(game, protocol::EntityType::Enemy), 0);
    EXPECT_EQ(game.snapshot().score, 1000u);
}

TEST(Game, LevelRestartsCleanlyWhenNoPlayerIsLeft)
{
    Game game;
    game.loadEntitiesConfig(TestEntities);
    game.loadLevel(OneBoss);
    game.onPlayerJoined(0);
    run(game, 1.f);
    ASSERT_EQ(countOf(game, protocol::EntityType::Enemy), 1);

    game.onPlayerLeft(0);
    run(game, 0.1f);
    EXPECT_EQ(countOf(game, protocol::EntityType::Enemy), 0);

    game.onPlayerJoined(0);
    run(game, 1.f);
    EXPECT_EQ(countOf(game, protocol::EntityType::Enemy), 1);
}

TEST(Game, PlayerStartsWithTheConfiguredLives)
{
    Game game;
    game.rules().playerLives = 5;
    game.onPlayerJoined(0);
    EXPECT_EQ(game.playerLives(0), 5);
}

TEST(Game, DeadPlayerLosesALifeAndRespawnsAfterTheDelay)
{
    Game game;
    game.loadEntitiesConfig(TestEntities);
    game.loadLevel(OneBoss);
    game.onPlayerJoined(0);
    game.onPlayerJoined(1); // stays away from the boss
    run(game, 5.f);
    for (int tick = 0; tick < 60; ++tick) {
        game.onInput(0, protocol::InputFlag::Right);
        game.update(1.f / 60.f);
    }
    EXPECT_EQ(game.playerLives(0), 2);
    EXPECT_EQ(game.playerLives(1), 3);
    EXPECT_EQ(countOf(game, protocol::EntityType::Player), 1);

    run(game, 2.5f);
    EXPECT_EQ(countOf(game, protocol::EntityType::Player), 2);
}

namespace {

// Keeps one ship in contact with the stopped boss, so it dies as soon as it can. Returns the
// shortest time between two deaths.
float ramTheBossUntilOutOfLives(Game &game, std::uint8_t player)
{
    const float dt = 1.f / 60.f;
    float shortestGap = 1e9f;
    float lastDeath = -1.f;
    int lives = game.playerLives(player);
    for (float elapsed = 0.f; elapsed < 60.f && lives > 0; elapsed += dt) {
        const auto snap = game.snapshot();
        for (const auto &entity : snap.entities) {
            if (entity.type == protocol::EntityType::Player && entity.variant == player)
                game.onInput(player, entity.x < 280.f ? protocol::InputFlag::Right : std::uint8_t{0});
        }
        game.update(dt);
        if (game.playerLives(player) < lives) {
            if (lastDeath >= 0.f)
                shortestGap = std::min(shortestGap, elapsed - lastDeath);
            lastDeath = elapsed;
            lives = game.playerLives(player);
        }
    }
    game.update(dt); // the end of the game is decided on the next tick
    return shortestGap;
}

} // namespace

TEST(Game, InvulnerabilityProtectsAndGameIsOverWhenLivesRunOut)
{
    Game game;
    game.loadEntitiesConfig(TestEntities);
    game.loadLevel(OneBoss);
    game.onPlayerJoined(0);
    EXPECT_EQ(game.gameState(), protocol::GameState::Playing);

    EXPECT_GE(ramTheBossUntilOutOfLives(game, 0), 3.9f); // respawn delay + invulnerability

    EXPECT_EQ(game.playerLives(0), 0);
    EXPECT_EQ(game.gameState(), protocol::GameState::GameOver);
    EXPECT_EQ(game.snapshot().state, protocol::GameState::GameOver);
    EXPECT_EQ(countOf(game, protocol::EntityType::Enemy), 0);

    // It stays over: nothing comes back while the players look at the screen.
    run(game, 10.f);
    EXPECT_EQ(game.gameState(), protocol::GameState::GameOver);
    EXPECT_EQ(countOf(game, protocol::EntityType::Enemy), 0);
    EXPECT_EQ(countOf(game, protocol::EntityType::Player), 0);
}

TEST(Game, DeathWithLivesLeftIsNotGameOver)
{
    Game game;
    game.loadEntitiesConfig(TestEntities);
    game.loadLevel(OneBoss);
    game.onPlayerJoined(0);
    run(game, 5.f);
    for (int tick = 0; tick < 60 && game.playerLives(0) == 3; ++tick) {
        game.onInput(0, protocol::InputFlag::Right);
        game.update(1.f / 60.f);
    }
    ASSERT_EQ(game.playerLives(0), 2);
    EXPECT_EQ(countOf(game, protocol::EntityType::Player), 0); // waiting to come back: nobody on screen
    run(game, 0.5f);
    EXPECT_EQ(game.gameState(), protocol::GameState::Playing);
}

TEST(Game, GameGoesOnWhileAnotherPlayerHasLives)
{
    Game game;
    game.loadEntitiesConfig(TestEntities);
    game.loadLevel(OneBoss);
    game.onPlayerJoined(0);
    game.onPlayerJoined(1);

    ramTheBossUntilOutOfLives(game, 0);

    EXPECT_EQ(game.playerLives(0), 0);
    EXPECT_EQ(game.gameState(), protocol::GameState::Playing);
    EXPECT_EQ(countOf(game, protocol::EntityType::Player), 1);
}

TEST(Game, NextPlayersStartAgainFromTheFirstLevelAfterAGameOver)
{
    Game game;
    game.loadEntitiesConfig(TestEntities);
    game.loadCampaign({OneGrunt, OneBoss});
    game.rules().intermissionDelay = 0.f;
    game.onPlayerJoined(0);
    for (int tick = 0; tick < 60 * 60 && game.currentLevelIndex() == 0; ++tick)
        game.update(1.f / 60.f);
    ASSERT_EQ(game.currentLevelIndex(), 1u);

    ramTheBossUntilOutOfLives(game, 0);
    ASSERT_EQ(game.gameState(), protocol::GameState::GameOver);

    game.onPlayerLeft(0);
    EXPECT_EQ(game.gameState(), protocol::GameState::Playing);
    EXPECT_EQ(game.currentLevelIndex(), 0u);

    game.onPlayerJoined(0);
    EXPECT_EQ(game.playerLives(0), 3);
    EXPECT_EQ(countOf(game, protocol::EntityType::Player), 1);
}

namespace {

// Waits for the campaign to reach a level. A stationary ship is rammed by the grunt of each level.
void runUntilLevel(Game &game, std::size_t level)
{
    for (int tick = 0; tick < 60 * 60 && game.currentLevelIndex() != level; ++tick)
        game.update(1.f / 60.f);
    ASSERT_EQ(game.currentLevelIndex(), level);
}

} // namespace

TEST(Game, LivesComeBackToTheFullCountOnlyWhenANewWorldStarts)
{
    Game game;
    game.loadEntitiesConfig(TestEntities);
    game.loadCampaign({OneGrunt, OneGrunt, OneGrunt});
    game.rules().levelsPerWorld = 2;
    game.rules().intermissionDelay = 0.f;
    game.onPlayerJoined(0);

    runUntilLevel(game, 1); // same world: the lost life stays lost
    EXPECT_EQ(game.playerLives(0), 2);

    runUntilLevel(game, 2); // new world: 1 life left gives 3, not 4
    EXPECT_EQ(game.playerLives(0), 3);
}

TEST(Game, PlayerOutOfLivesComesBackInTheNextWorld)
{
    Game game;
    game.loadEntitiesConfig(TestEntities);
    game.loadCampaign({OneGrunt, OneGrunt});
    game.rules().levelsPerWorld = 1;
    game.rules().playerLives = 1;
    game.rules().intermissionDelay = 0.f;
    game.onPlayerJoined(0);
    game.onPlayerJoined(1); // the grunt only rams one of the two ships
    run(game, 10.f);
    ASSERT_EQ(game.playerLives(0) + game.playerLives(1), 2);

    runUntilLevel(game, 1);
    EXPECT_EQ(game.playerLives(0), 1);
    EXPECT_EQ(game.playerLives(1), 1);
    EXPECT_EQ(game.gameState(), protocol::GameState::Playing);

    run(game, 2.5f); // respawn delay
    EXPECT_EQ(countOf(game, protocol::EntityType::Player), 2);
}

TEST(Game, GameRulesGiveThreeLevelsPerWorld)
{
    Game game;
    EXPECT_EQ(game.rules().levelsPerWorld, 3u);
    EXPECT_EQ(game.rules().playerLives, 3);
}

TEST(Game, PlayerJoiningAFinishedGameGetsNoShip)
{
    Game game;
    game.loadEntitiesConfig(TestEntities);
    game.loadLevel(OneBoss);
    game.onPlayerJoined(0);
    ramTheBossUntilOutOfLives(game, 0);
    ASSERT_EQ(game.gameState(), protocol::GameState::GameOver);

    game.onPlayerJoined(1);
    run(game, 3.f);
    EXPECT_EQ(countOf(game, protocol::EntityType::Player), 0);
    EXPECT_EQ(game.gameState(), protocol::GameState::GameOver);
}

TEST(Movement, BackAndForthKeepsAdvancingAtItsConfiguredSpeed)
{
    engine::ecs::Registry registry;
    const auto enemy = registry.spawn();
    registry.emplace<rtype::components::Position>(enemy, 1000.f, 300.f);
    registry.emplace<rtype::components::Velocity>(enemy, 0.f, 0.f);
    registry.emplace<rtype::components::Kind>(enemy, protocol::EntityType::Enemy, 0);
    registry.emplace<rtype::components::MovementPattern>(
        enemy, rtype::components::MovementPattern{"back_and_forth", 0.f, 0.f, 62.f, 0.f, 0.f, 0.f});

    for (int tick = 0; tick < 3600; ++tick) {
        updateEnemyMovements(registry, 1.f / 60.f);
        const auto *velocity = registry.get<rtype::components::Velocity>(enemy);
        ASSERT_NE(velocity, nullptr);
        ASSERT_LT(velocity->x, 0.f);
        ASSERT_GE(velocity->x, -112.1f);
    }
}

TEST(Movement, PatternDoesNotDependOnPreviousGames)
{
    const auto verticalSpeedAfterOneSecond = [] {
        engine::ecs::Registry registry;
        const auto enemy = registry.spawn();
        registry.emplace<rtype::components::Position>(enemy, 1000.f, 300.f);
        registry.emplace<rtype::components::Velocity>(enemy, 0.f, 0.f);
        registry.emplace<rtype::components::Kind>(enemy, protocol::EntityType::Enemy, 0);
        registry.emplace<rtype::components::MovementPattern>(
            enemy, rtype::components::MovementPattern{"orbital", 0.f, 0.f, 100.f, 0.f, 0.f, 0.f});
        for (int tick = 0; tick < 60; ++tick)
            updateEnemyMovements(registry, 1.f / 60.f);
        return registry.get<rtype::components::Velocity>(enemy)->y;
    };

    EXPECT_FLOAT_EQ(verticalSpeedAfterOneSecond(), verticalSpeedAfterOneSecond());
}

namespace {

struct Flight {
    float minX = 1e9f, minY = 1e9f, maxY = -1e9f;
    float x = 0.f, y = 0.f, vy = 0.f;
};

// Moves one enemy with the real pattern code for some time, and records where it went.
Flight fly(const std::string &pattern, float startX, float startY, float speed, float seconds)
{
    engine::ecs::Registry registry;
    const auto enemy = registry.spawn();
    registry.emplace<rtype::components::Position>(enemy, startX, startY);
    registry.emplace<rtype::components::Velocity>(enemy, -speed, 0.f);
    registry.emplace<rtype::components::Kind>(enemy, protocol::EntityType::Enemy, 0);
    registry.emplace<rtype::components::MovementPattern>(
        enemy, rtype::components::MovementPattern{pattern, 0.f, 0.f, speed, 0.f, 0.f, 0.f, false});

    Flight flight;
    const float dt = 1.f / 60.f;
    for (int tick = 0; tick < static_cast<int>(seconds * 60.f); ++tick) {
        updateEnemyMovements(registry, dt);
        auto *position = registry.get<rtype::components::Position>(enemy);
        const auto *velocity = registry.get<rtype::components::Velocity>(enemy);
        position->x += velocity->x * dt;
        position->y += velocity->y * dt;
        flight.minX = std::min(flight.minX, position->x);
        flight.minY = std::min(flight.minY, position->y);
        flight.maxY = std::max(flight.maxY, position->y);
        flight.x = position->x;
        flight.y = position->y;
        flight.vy = velocity->y;
    }
    return flight;
}

} // namespace

TEST(Movement, MartianBounceKeepsJumpingBetweenSpawnHeightAndGround)
{
    // Over several minutes: the height of the jumps must not drift.
    const Flight flight = fly("martian_bounce", 1920.f, 500.f, 120.f, 300.f);
    EXPECT_NEAR(flight.minY, 500.f, 25.f);
    EXPECT_NEAR(flight.maxY, 1008.f, 15.f);
}

TEST(Movement, GravityDiveFliesFlatThenFallsFasterAndFaster)
{
    EXPECT_FLOAT_EQ(fly("gravity_dive", 1920.f, 300.f, 150.f, 4.f).y, 300.f);
    const float vySoon = fly("gravity_dive", 1920.f, 300.f, 150.f, 5.5f).vy;
    const float vyLater = fly("gravity_dive", 1920.f, 300.f, 150.f, 6.5f).vy;
    EXPECT_GT(vySoon, 0.f);
    EXPECT_GT(vyLater, vySoon);
}

TEST(Movement, GravityPullKeepsCrossingTheCentralLine)
{
    const Flight flight = fly("gravity_pull", 1920.f, 200.f, 150.f, 13.f);
    EXPECT_LT(flight.minY, 210.f);
    EXPECT_GT(flight.maxY, 800.f); // symmetrical around y = 540
    EXPECT_LT(flight.maxY, 1000.f);
}

TEST(Movement, BlackHoleOrbitSpiralsThenThrowsTheEnemyToTheLeft)
{
    const Flight flight = fly("black_hole_orbit", 1920.f, 300.f, 100.f, 60.f);
    EXPECT_LT(flight.minX, -66.f);
    EXPECT_GT(flight.minY, -72.f);
    EXPECT_LT(flight.maxY, 1080.f);
}

TEST(Game, GravityWellBulletCurvesTowardTheCentralLine)
{
    engine::ecs::Registry registry;
    const auto enemy = registry.spawn();
    registry.emplace<rtype::components::Position>(enemy, 1500.f, 100.f);
    registry.emplace<rtype::components::Kind>(enemy, protocol::EntityType::Enemy, 0);
    registry.emplace<rtype::components::EnemyAttack>(enemy, 0.f, 0.5f, 300.f, 0, "gravity_well");

    engine::ecs::Entity bullet = engine::ecs::NullEntity;
    updateEnemyShooting(registry, [&](protocol::EntityType type, std::uint8_t variant, float x, float y) {
        bullet = registry.spawn();
        registry.emplace<rtype::components::Kind>(bullet, type, variant);
        registry.emplace<rtype::components::Position>(bullet, x, y);
        registry.emplace<rtype::components::Velocity>(bullet, 0.f, 0.f);
        return bullet;
    }, 0.6f);

    ASSERT_NE(bullet, engine::ecs::NullEntity);
    EXPECT_GT(registry.get<rtype::components::Velocity>(bullet)->y, 0.f); // pulled down, toward y = 540
}

TEST(Movement, StoppedBossNeverLeavesTheScreen)
{
    engine::ecs::Registry registry;
    const auto boss = registry.spawn();
    registry.emplace<rtype::components::Position>(boss, 1500.f, 1008.f);
    registry.emplace<rtype::components::Velocity>(boss, 0.f, 0.f);
    registry.emplace<rtype::components::Kind>(boss, protocol::EntityType::Enemy, 0);
    registry.emplace<rtype::components::MovementPattern>(
        boss, rtype::components::MovementPattern{"dive", 0.f, 0.f, 30.f, 1500.f, 0.f, 0.f});

    updateEnemyMovements(registry, 1.f / 60.f);
    const auto *velocity = registry.get<rtype::components::Velocity>(boss);
    ASSERT_NE(velocity, nullptr);
    EXPECT_FLOAT_EQ(velocity->x, 0.f);
    EXPECT_FLOAT_EQ(velocity->y, 0.f);
}

TEST(Game, EnemyKeepsTheSpriteOfItsDefinition)
{
    Game game;
    game.loadEntitiesConfig(TestEntities);
    game.loadLevel(OneSprite);
    game.onPlayerJoined(0);
    run(game, 0.1f);

    const auto snap = game.snapshot();
    const auto enemy = std::find_if(snap.entities.begin(), snap.entities.end(), [](const auto &entity) {
        return entity.type == protocol::EntityType::Enemy;
    });
    ASSERT_NE(enemy, snap.entities.end());
    EXPECT_EQ(enemy->variant, 3);
}

TEST(Game, EveryEnemyOfTheGameDeclaresItsSprite)
{
    std::ifstream file("assets/config/entities.json");
    ASSERT_TRUE(file.is_open());
    const auto config = nlohmann::json::parse(file);
    for (const auto &[name, enemy] : config.at("enemies").items()) {
        ASSERT_TRUE(enemy.contains("variant")) << name;
        if (enemy.value("is_boss", false)) {
            EXPECT_GE(enemy.at("variant").get<int>(), 5) << name;
        }
    }
}

TEST(Movement, BossSpriteStaysOnScreenWhileBouncing)
{
    engine::ecs::Registry registry;
    const auto boss = registry.spawn();
    registry.emplace<rtype::components::Position>(boss, 1200.f, 700.f);
    registry.emplace<rtype::components::Velocity>(boss, 0.f, 0.f);
    registry.emplace<rtype::components::Kind>(boss, protocol::EntityType::Enemy, 6);
    registry.emplace<rtype::components::MovementPattern>(
        boss, rtype::components::MovementPattern{"martian_bounce", 60.f, 0.f, 35.f, 1200.f});

    const auto box = rtype::components::enemyHitbox(6);
    const float dt = 1.f / 60.f;
    for (int tick = 0; tick < 60 * 60; ++tick) {
        updateEnemyMovements(registry, dt);
        auto *position = registry.get<rtype::components::Position>(boss);
        const auto *velocity = registry.get<rtype::components::Velocity>(boss);
        position->y += velocity->y * dt;
        // One tick of tolerance: the ground is detected once it has been reached.
        EXPECT_LE(position->y + box.offsetY + box.height, WorldHeight + 5.f);
        EXPECT_GE(position->y + box.offsetY, -5.f);
    }
}
