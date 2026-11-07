#include "brickrush/Game.hpp"

#include "brickrush/Prefabs.hpp"
#include "engine/systems/Movement.hpp"

#include <algorithm>
#include <cmath>
#include <map>

namespace brickrush {

namespace {

constexpr float CellWidth = 80.f; // a brick and the gap after it
constexpr float CellHeight = 30.f;
constexpr float BricksTop = 70.f;
constexpr float PaddleY = Game::Height - 50.f;
constexpr float LaunchAngle = 0.5f; // radians from the vertical

// The paddle stays on screen; a ball waiting on it follows it.
void paddleSystem(engine::ecs::Registry &registry, float)
{
    float paddleCenter = Game::Width / 2.f;
    registry.each<Paddle, Position, Box>([&](engine::ecs::Entity, Paddle &, Position &position, Box &box) {
        position.x = std::clamp(position.x, 0.f, Game::Width - box.width);
        paddleCenter = position.x + box.width / 2.f;
    });
    registry.each<Ball, Position, Box>([&](engine::ecs::Entity, Ball &ball, Position &position, Box &box) {
        if (ball.attached)
            position = {paddleCenter - box.width / 2.f, PaddleY - box.height};
    });
}

} // namespace

Game::Game(const std::string &prefabsPath, std::vector<std::string> levelPaths) : _levelPaths(std::move(levelPaths))
{
    loadPrefabs(_prefabs, prefabsPath);
    // Subscribed once: the handlers run at dispatch(), after every system has finished iterating.
    _bus.subscribe<BrickHit>([this](const BrickHit &event) { onBrickHit(event); });
    _bus.subscribe<BallLost>([this](const BallLost &) { onBallLost(); });
    restart();
}

void Game::restart()
{
    _level = 0;
    _lives = StartLives;
    _score = 0;
    _won = false;
    loadLevel();
}

// Order matters: move first, then keep the paddle on screen, then bounce on the new positions.
void Game::registerSystems()
{
    _registry.addSystem(engine::systems::movement<Position, Velocity>);
    _registry.addSystem(paddleSystem);
    _registry.addSystem([this](engine::ecs::Registry &registry, float) { collisionSystem(registry); });
}

// A new, empty world with the bricks of the current level, the paddle and the ball.
void Game::loadLevel()
{
    _registry = engine::ecs::Registry{};
    registerSystems();
    spawnBricks(readJson(_levelPaths.at(_level)));
    _paddle = spawn("paddle", 0.f, PaddleY);
    _registry.get<Position>(_paddle)->x = (Width - _registry.get<Box>(_paddle)->width) / 2.f;
    _ball = spawn("ball", Width / 2.f, PaddleY);
    resetBall();
}

// Each character of a row is a brick prefab from the legend; any other character leaves a hole.
void Game::spawnBricks(const nlohmann::json &level)
{
    const auto legend = level.at("legend").get<std::map<std::string, std::string>>();
    const auto rows = level.at("rows").get<std::vector<std::string>>();
    std::size_t columns = 0;
    for (const std::string &row : rows)
        columns = std::max(columns, row.size());
    const float left = (Width - static_cast<float>(columns) * CellWidth) / 2.f;

    for (std::size_t row = 0; row < rows.size(); row++) {
        for (std::size_t column = 0; column < rows[row].size(); column++) {
            const auto brick = legend.find(std::string(1, rows[row][column]));
            if (brick != legend.end())
                spawn(brick->second, left + static_cast<float>(column) * CellWidth,
                      BricksTop + static_cast<float>(row) * CellHeight);
        }
    }
}

engine::ecs::Entity Game::spawn(const std::string &prefab, float x, float y)
{
    const engine::ecs::Entity entity = _prefabs.spawn(_registry, prefab);
    _registry.emplace<Position>(entity, x, y);
    _registry.emplace<Velocity>(entity);
    return entity;
}

// The ball waits on the paddle until the player launches it.
void Game::resetBall()
{
    _registry.get<Ball>(_ball)->attached = true;
    *_registry.get<Velocity>(_ball) = {};
}

void Game::movePaddle(float direction)
{
    if (auto *velocity = _registry.get<Velocity>(_paddle))
        velocity->x = std::clamp(direction, -1.f, 1.f) * _registry.get<Paddle>(_paddle)->speed;
}

void Game::launch()
{
    Ball *ball = _registry.get<Ball>(_ball);
    if (!ball || !ball->attached || over() || won())
        return;
    ball->attached = false;
    *_registry.get<Velocity>(_ball) = {ball->speed * std::sin(LaunchAngle), -ball->speed * std::cos(LaunchAngle)};
}

void Game::step(float timeStepSeconds)
{
    if (over() || won())
        return;
    _registry.update(timeStepSeconds);
    _bus.dispatch();
    if (!levelCleared())
        return;
    if (_level + 1 < _levelPaths.size()) {
        _level++;
        loadLevel();
    } else {
        _won = true;
    }
}

void Game::onBrickHit(const BrickHit &event)
{
    Brick *brick = _registry.get<Brick>(event.brick);
    if (!brick || brick->hits == 0) // an unbreakable brick only bounces the ball
        return;
    if (--brick->hits > 0)
        return;
    _score += brick->points;
    _registry.kill(event.brick);
}

void Game::onBallLost()
{
    _lives--;
    if (_lives > 0)
        resetBall();
}

// Cleared when only unbreakable bricks are left.
bool Game::levelCleared()
{
    bool breakableLeft = false;
    _registry.each<Brick>([&breakableLeft](engine::ecs::Entity, Brick &brick) {
        if (brick.hits > 0)
            breakableLeft = true;
    });
    return !breakableLeft;
}

} // namespace brickrush
