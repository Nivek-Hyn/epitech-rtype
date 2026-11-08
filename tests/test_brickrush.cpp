#include "brickrush/Components.hpp"
#include "brickrush/Game.hpp"

#include <gtest/gtest.h>

#include <vector>

namespace {

const std::string Assets = "games/brickrush/assets/";
constexpr float Step = 1.f / 60.f;

brickrush::Game makeGame()
{
    return brickrush::Game(Assets + "bricks.json", {Assets + "level1.json", Assets + "level2.json"});
}

std::vector<engine::ecs::Entity> bricks(brickrush::Game &game)
{
    std::vector<engine::ecs::Entity> found;
    game.registry().each<brickrush::Brick>(
        [&found](engine::ecs::Entity entity, brickrush::Brick &) { found.push_back(entity); });
    return found;
}

engine::ecs::Entity ball(brickrush::Game &game)
{
    engine::ecs::Entity found = engine::ecs::NullEntity;
    game.registry().each<brickrush::Ball>([&found](engine::ecs::Entity entity, brickrush::Ball &) { found = entity; });
    return found;
}

// Sends the ball straight up into the last brick of the grid, which has no brick below it.
// Returns the points of that brick.
int throwBallAtLastBrick(brickrush::Game &game, int brickHits)
{
    engine::ecs::Registry &registry = game.registry();
    const engine::ecs::Entity brick = bricks(game).back();
    registry.get<brickrush::Brick>(brick)->hits = brickHits;
    const brickrush::Position target = *registry.get<brickrush::Position>(brick);
    const engine::ecs::Entity thrown = ball(game);
    registry.get<brickrush::Ball>(thrown)->attached = false;
    *registry.get<brickrush::Position>(thrown) = {target.x + 20.f, target.y + 40.f};
    *registry.get<brickrush::Velocity>(thrown) = {0.f, -400.f};
    return registry.get<brickrush::Brick>(brick)->points;
}

} // namespace

TEST(BrickRush, LevelGridBecomesBrickPrefabs)
{
    brickrush::Game game = makeGame();
    EXPECT_EQ(bricks(game).size(), 48u);
    EXPECT_EQ(game.lives(), brickrush::Game::StartLives);
}

TEST(BrickRush, BallBreaksABrickScoresAndBouncesBack)
{
    brickrush::Game game = makeGame();
    const int points = throwBallAtLastBrick(game, 1);
    for (int i = 0; i < 10; i++)
        game.step(Step);

    EXPECT_EQ(bricks(game).size(), 47u);
    EXPECT_EQ(game.score(), points);
    EXPECT_GT(game.registry().get<brickrush::Velocity>(ball(game))->y, 0.f);
}

TEST(BrickRush, UnbreakableBrickOnlyBouncesTheBall)
{
    brickrush::Game game = makeGame();
    throwBallAtLastBrick(game, 0);
    for (int i = 0; i < 10; i++)
        game.step(Step);

    EXPECT_EQ(bricks(game).size(), 48u);
    EXPECT_EQ(game.score(), 0);
}

TEST(BrickRush, LosingEveryBallEndsTheGame)
{
    brickrush::Game game = makeGame();
    for (int life = 0; life < brickrush::Game::StartLives; life++) {
        engine::ecs::Registry &registry = game.registry();
        registry.get<brickrush::Ball>(ball(game))->attached = false;
        *registry.get<brickrush::Position>(ball(game)) = {500.f, brickrush::Game::Height - 5.f};
        *registry.get<brickrush::Velocity>(ball(game)) = {0.f, 600.f};
        game.step(Step);
    }
    EXPECT_TRUE(game.over());
    game.restart();
    EXPECT_FALSE(game.over());
}

TEST(BrickRush, ClearingALevelLoadsTheNextOne)
{
    brickrush::Game game = makeGame();
    const auto all = bricks(game);
    for (std::size_t i = 0; i + 1 < all.size(); i++)
        game.registry().kill(all[i]);
    const int points = throwBallAtLastBrick(game, 1);
    for (int i = 0; i < 10; i++)
        game.step(Step);

    EXPECT_EQ(game.level(), 1u);
    EXPECT_EQ(game.score(), points);
}

TEST(BrickRush, ARobotFollowingTheBallKeepsBreakingBricks)
{
    brickrush::Game game = makeGame();
    game.launch();
    for (int i = 0; i < 60 * 60 && !game.won(); i++) {
        engine::ecs::Registry &registry = game.registry();
        float paddleCenter = 0.f;
        registry.each<brickrush::Paddle, brickrush::Position, brickrush::Box>(
            [&](engine::ecs::Entity, brickrush::Paddle &, brickrush::Position &position, brickrush::Box &box) {
                paddleCenter = position.x + box.width / 2.f;
            });
        const float ballCenter = registry.get<brickrush::Position>(ball(game))->x + 7.f;
        game.movePaddle(ballCenter < paddleCenter - 10.f ? -1.f : (ballCenter > paddleCenter + 10.f ? 1.f : 0.f));
        game.launch();
        game.step(Step);
    }
    EXPECT_GT(game.score(), 300);
    EXPECT_FALSE(game.over());
}
