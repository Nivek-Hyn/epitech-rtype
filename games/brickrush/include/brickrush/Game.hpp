#pragma once

#include "brickrush/Components.hpp"
#include "engine/core/EventBus.hpp"
#include "engine/ecs/Prefabs.hpp"
#include "engine/ecs/Registry.hpp"

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

namespace brickrush {

// Brick Rush rules, without any window: the paddle sends the ball back, the ball breaks the bricks.
// Bricks, paddle and ball are prefabs of assets/bricks.json; levels are grids of bricks.
class Game {
public:
    static constexpr float Width = 1000.f;
    static constexpr float Height = 750.f;
    static constexpr int StartLives = 3;

    Game(const std::string &prefabsPath, std::vector<std::string> levelPaths);

    // Back to the first level with full lives and no score.
    void restart();
    // -1 to go left, 1 to go right, 0 to stop.
    void movePaddle(float direction);
    void launch();
    // Simulates one fixed step: runs every system, then delivers the events they sent.
    void step(float timeStepSeconds);

    int lives() const { return _lives; }
    int score() const { return _score; }
    std::size_t level() const { return _level; }
    bool over() const { return _lives <= 0; }
    bool won() const { return _won; }
    engine::ecs::Registry &registry() { return _registry; }

private:
    void registerSystems();
    void loadLevel();
    void spawnBricks(const nlohmann::json &level);
    engine::ecs::Entity spawn(const std::string &prefab, float x, float y);
    void resetBall();
    void collisionSystem(engine::ecs::Registry &registry);
    void onBrickHit(const BrickHit &event);
    void onBallLost();
    bool levelCleared();

    engine::ecs::Prefabs<nlohmann::json> _prefabs;
    std::vector<std::string> _levelPaths;
    engine::ecs::Registry _registry;
    engine::core::EventBus _bus;
    engine::ecs::Entity _paddle = engine::ecs::NullEntity;
    engine::ecs::Entity _ball = engine::ecs::NullEntity;
    std::size_t _level = 0;
    int _lives = StartLives;
    int _score = 0;
    bool _won = false;
};

} // namespace brickrush
