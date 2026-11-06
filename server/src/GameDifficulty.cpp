// Difficulty: how strong things are when they appear, with the difficulty and the infinite mode loops.

#include "Game.hpp"
#include <algorithm>
#include <cmath>

namespace rtype::server {

Game::DifficultyConfig &Game::currentDifficulty()
{
    return _rules.difficulties[_difficulty];
}

// Number of infinite mode loops already completed.
float Game::completedLoops() const
{
    return (_gameMode == GameMode::Infinite && _infiniteLoopCount > 1) ? static_cast<float>(_infiniteLoopCount - 1) : 0.0f;
}

int Game::scaledHealth(int health)
{
    const float growth = 1.0f + completedLoops() * _rules.infinite.healthGrowthPerLoop;
    return std::max(1, static_cast<int>(std::round(static_cast<float>(health) * currentDifficulty().healthMultiplier * growth)));
}

float Game::scaledSpeed(float speed)
{
    const float growth = 1.0f + completedLoops() * _rules.infinite.speedGrowthPerLoop;
    return speed * currentDifficulty().speedMultiplier * growth;
}

float Game::scaledFireRate(float fireRate)
{
    const float growth = 1.0f + completedLoops() * _rules.infinite.fireRateGrowthPerLoop;
    return std::max(_rules.infinite.minFireRate, fireRate / (currentDifficulty().fireRateMultiplier * growth));
}

float Game::scaledBulletSpeed(float bulletSpeed)
{
    const float growth = 1.0f + completedLoops() * _rules.infinite.bulletSpeedGrowthPerLoop;
    return std::min(_rules.infinite.maxBulletSpeed, bulletSpeed * currentDifficulty().bulletSpeedMultiplier * growth);
}

} // namespace rtype::server
