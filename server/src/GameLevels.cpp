// Levels: loading them, going to the next one, winning the campaign, starting again.

#include "Game.hpp"
#include "common/Components.hpp"
#include <algorithm>

namespace rtype::server {

void Game::loadLevel(const std::string &filepath)
{
    _campaignLevels = {filepath};
    _currentLevelIndex = 0;
    loadCurrentLevel();
}

void Game::loadCampaign(const std::vector<std::string> &levelPaths)
{
    _campaignLevels = levelPaths;
    _currentLevelIndex = 0;
    if (!_campaignLevels.empty())
        loadCurrentLevel();
}

void Game::loadCurrentLevel()
{
    _currentLevel = loadLevelFromJson(_campaignLevels[_currentLevelIndex]);
    resetWaves();
}

void Game::loadFirstLevel()
{
    _currentLevelIndex = 0;
    if (_campaignLevels.empty())
        resetWaves();
    else
        loadCurrentLevel();
}

void Game::resetWaves()
{
    _levelTimer = 0.f;
    _transitionTimer = 0.f;
    for (auto &wave : _currentLevel.waves)
        wave.isSpawned = false;
}

bool Game::allWavesSpawned() const
{
    return !_currentLevel.waves.empty() && std::all_of(_currentLevel.waves.begin(), _currentLevel.waves.end(),
                                                       [](const WaveConfig &wave) { return wave.isSpawned; });
}

bool Game::anyEnemyAlive()
{
    bool alive = false;
    _registry.each<rtype::components::Kind>(
        [&alive](engine::ecs::Entity /*entity*/, const rtype::components::Kind &kind) {
            if (kind.type == protocol::EntityType::Enemy)
                alive = true;
        });
    return alive;
}

// A level is cleared when all its waves came and no enemy is left. Tiles do not count.
void Game::updateLevelProgression(float dt)
{
    if (_state != protocol::GameState::Playing)
        return;
    if (!allWavesSpawned() || anyEnemyAlive()) {
        _transitionTimer = 0.f;
        return;
    }
    _transitionTimer += dt;
    if (_transitionTimer < _rules.intermissionDelay)
        return;
    _transitionTimer = 0.f;
    goToNextLevel();
    restoreLivesOnNewWorld();
}

// After the last level, the campaign is won and infinite mode starts a new, harder loop.
void Game::goToNextLevel()
{
    if (_currentLevelIndex + 1 < _campaignLevels.size()) {
        _currentLevelIndex++;
        loadCurrentLevel();
    } else if (_gameMode == GameMode::Campaign) {
        _state = protocol::GameState::Finished;
    } else {
        _infiniteLoopCount++;
        loadFirstLevel();
    }
}

void Game::restartCampaign()
{
    clearEnemies();
    _state = protocol::GameState::Waiting;
    _lobbyTimer = 0.f;
    _score = 0;
    _weapons.fill(_arsenal.defaultWeapon);
    _infiniteLoopCount = 1;
    loadFirstLevel();
}

// Waiting room: the game starts lobby_delay seconds after the first player joined, so the others can join too.
void Game::updateLobby(float dt)
{
    if (_state != protocol::GameState::Waiting)
        return;
    if (!anyPlayerConnected()) {
        _lobbyTimer = 0.f;
        return;
    }
    _lobbyTimer += dt;
    startIfLobbyOver();
}

void Game::startIfLobbyOver()
{
    if (_state == protocol::GameState::Waiting && anyPlayerConnected() && _lobbyTimer >= _rules.lobbyDelay)
        _state = protocol::GameState::Playing;
}

} // namespace rtype::server
