// Levels: loading them, going to the next one, winning the campaign, starting again.

#include "Game.hpp"
#include "GameComponents.hpp"
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

void Game::checkLevels() const
{
    for (const std::string &path : _campaignLevels)
        loadLevelFromJson(path);
}

void Game::startAtLevel(std::size_t index)
{
    _currentLevelIndex = index < _campaignLevels.size() ? index : 0;
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
    _playerReady.fill(false);
    _score = 0;
    _weapons.fill(_arsenal.defaultWeapon);
    _infiniteLoopCount = 1;
    _endTimer = 0.f;
    loadFirstLevel();
    // Players still connected play the next game: full lives, and a ship for those who lost theirs.
    for (std::size_t player = 0; player < _players.size(); player++) {
        if (!_connected[player])
            continue;
        _lives[player] = _rules.playerLives;
        if (_players[player] == engine::ecs::NullEntity)
            spawnPlayerShip(player);
    }
}

// After a victory or a game over, the end screen stays restart_delay seconds, then a new game starts.
void Game::restartAfterEnd(float dt)
{
    if (_state != protocol::GameState::GameOver && _state != protocol::GameState::Finished)
        return;
    _endTimer += dt;
    if (_endTimer >= _rules.restartDelay)
        restartCampaign();
}

bool Game::allConnectedPlayersReady() const
{
    bool any = false;
    for (std::size_t i = 0; i < MaxPlayers; ++i) {
        if (_connected[i]) {
            any = true;
            if (!_playerReady[i])
                return false;
        }
    }
    return any;
}

// Waiting room: players press ready to start, or lobby_delay expires if configured.
void Game::updateLobby(float dt)
{
    if (_state != protocol::GameState::Waiting)
        return;
    if (!anyPlayerConnected()) {
        _lobbyTimer = 0.f;
        _playerReady.fill(false);
        return;
    }
    _lobbyTimer += dt;
    startIfLobbyOver();
}

void Game::startIfLobbyOver()
{
    if (_state != protocol::GameState::Waiting || !anyPlayerConnected())
        return;
    if (_rules.lobbyDelay <= 0.f || allConnectedPlayersReady()) {
        _state = protocol::GameState::Playing;
        return;
    }
    if (_rules.lobbyDelay > 0.f && _rules.lobbyDelay < 1000.f && _lobbyTimer >= _rules.lobbyDelay) {
        _state = protocol::GameState::Playing;
    }
}

} // namespace rtype::server
