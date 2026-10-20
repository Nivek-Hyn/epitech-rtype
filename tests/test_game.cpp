#include "Game.hpp"

#include <gtest/gtest.h>

#include <algorithm>

using namespace rtype;
using namespace rtype::server;

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
    for (int tick = 0; tick < 180; tick++)
        game.update(1.f / 60.f);
    EXPECT_TRUE(game.snapshot().entities.empty());

    game.onPlayerJoined(0);
    for (int tick = 0; tick < 180; tick++)
        game.update(1.f / 60.f);
    const auto snap = game.snapshot();
    const auto enemyCount = std::count_if(snap.entities.begin(), snap.entities.end(), [](const auto &entity) {
        return entity.type == protocol::EntityType::Enemy;
    });
    EXPECT_EQ(enemyCount, 1);
}

TEST(Game, PlayerMissileDestroysEnemyAndScores)
{
    Game game;
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
