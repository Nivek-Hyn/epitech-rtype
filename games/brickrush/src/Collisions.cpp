// Ball physics: bounces on the walls, the paddle and the bricks. Only events are sent here; their
// effects (breaking a brick, losing a life) are applied at dispatch, after every system has run.

#include "brickrush/Game.hpp"

#include "engine/physics/Aabb.hpp"

#include <algorithm>
#include <cmath>
#include <optional>

namespace brickrush {

namespace {

constexpr float MaxBounceAngle = 1.05f; // radians from the vertical, at the edge of the paddle

engine::physics::Aabb boxOf(const Position &position, const Box &box)
{
    return {position.x, position.y, box.width, box.height};
}

// Returns false when the ball fell below the screen.
bool bounceOnWalls(Position &position, Velocity &velocity, const Box &box)
{
    if (position.x < 0.f) {
        position.x = 0.f;
        velocity.x = std::abs(velocity.x);
    } else if (position.x + box.width > Game::Width) {
        position.x = Game::Width - box.width;
        velocity.x = -std::abs(velocity.x);
    }
    if (position.y < 0.f) {
        position.y = 0.f;
        velocity.y = std::abs(velocity.y);
    }
    return position.y < Game::Height;
}

// The further from the center the ball hits the paddle, the more it goes sideways.
void bounceOnPaddle(Position &position, Velocity &velocity, const engine::physics::Aabb &ball,
                    const engine::physics::Aabb &paddle)
{
    const float offset = (ball.left + ball.width / 2.f) - (paddle.left + paddle.width / 2.f);
    const float angle = std::clamp(offset / (paddle.width / 2.f), -1.f, 1.f) * MaxBounceAngle;
    const float speed = std::hypot(velocity.x, velocity.y);
    velocity = {speed * std::sin(angle), -speed * std::cos(angle)};
    position.y = paddle.top - ball.height;
}

// Sends the ball away from the brick, along the axis where it entered the brick the least.
// Setting the direction (instead of flipping it) keeps a ball overlapping two bricks from getting stuck.
void bounceOnBrick(Velocity &velocity, const engine::physics::Aabb &ball, const engine::physics::Aabb &brick)
{
    const float overlapX = std::min(ball.left + ball.width, brick.left + brick.width) - std::max(ball.left, brick.left);
    const float overlapY = std::min(ball.top + ball.height, brick.top + brick.height) - std::max(ball.top, brick.top);
    if (overlapX < overlapY) {
        const bool ballOnLeft = ball.left + ball.width / 2.f < brick.left + brick.width / 2.f;
        velocity.x = ballOnLeft ? -std::abs(velocity.x) : std::abs(velocity.x);
    } else {
        const bool ballAbove = ball.top + ball.height / 2.f < brick.top + brick.height / 2.f;
        velocity.y = ballAbove ? -std::abs(velocity.y) : std::abs(velocity.y);
    }
}

} // namespace

void Game::collisionSystem(engine::ecs::Registry &registry)
{
    auto *ball = registry.get<Ball>(_ball);
    auto *position = registry.get<Position>(_ball);
    auto *velocity = registry.get<Velocity>(_ball);
    const Box *box = registry.get<Box>(_ball);
    if (!ball || ball->attached || !position || !velocity || !box)
        return;
    if (!bounceOnWalls(*position, *velocity, *box)) {
        _bus.enqueue(BallLost{});
        return;
    }

    const engine::physics::Aabb ballBox = boxOf(*position, *box);
    const auto *paddlePosition = registry.get<Position>(_paddle);
    const auto *paddleBox = registry.get<Box>(_paddle);
    if (paddlePosition && paddleBox && velocity->y > 0.f &&
        engine::physics::overlaps(ballBox, boxOf(*paddlePosition, *paddleBox)))
        bounceOnPaddle(*position, *velocity, ballBox, boxOf(*paddlePosition, *paddleBox));

    // One brick per step at most, so the ball never bounces twice in the same direction.
    std::optional<engine::ecs::Entity> hit;
    registry.each<Brick, Position, Box>([&](engine::ecs::Entity brick, Brick &, Position &brickPosition, Box &size) {
        if (!hit && engine::physics::overlaps(ballBox, boxOf(brickPosition, size))) {
            bounceOnBrick(*velocity, ballBox, boxOf(brickPosition, size));
            hit = brick;
        }
    });
    if (hit)
        _bus.enqueue(BrickHit{*hit});
}

} // namespace brickrush
