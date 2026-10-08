#include "Game.hpp"

#include <gtest/gtest.h>

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
    const float startX = game.snapshot().entities[0].x;
    game.onInput(1, protocol::InputFlag::Right);
    game.update(0.1f);
    EXPECT_GT(game.snapshot().entities[0].x, startX);
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
