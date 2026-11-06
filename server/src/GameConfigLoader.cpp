// Reading the configuration: entities.json (prefabs, see Prefabs.cpp) and game_rules.json.

#include "Game.hpp"
#include <algorithm>
#include <array>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <string>
#include <utility>

namespace rtype::server {

namespace {

using Json = nlohmann::json;

bool hasSection(const Json &json, const char *name)
{
    return json.contains(name) && json[name].is_object();
}

Game::DifficultyConfig parseDifficulty(const Json &json)
{
    Game::DifficultyConfig difficulty;
    difficulty.healthMultiplier = json.value("health_multiplier", 1.0f);
    difficulty.speedMultiplier = json.value("speed_multiplier", 1.0f);
    difficulty.fireRateMultiplier = json.value("fire_rate_multiplier", 1.0f);
    difficulty.bulletSpeedMultiplier = json.value("bullet_speed_multiplier", 1.0f);
    difficulty.scoreMultiplier = json.value("score_multiplier", 1.0f);
    return difficulty;
}

void parseDifficulties(const Json &json, Game::GameRules &rules)
{
    const std::array<std::pair<const char *, Game::Difficulty>, 3> levels = {
        {{"easy", Game::Difficulty::Easy}, {"normal", Game::Difficulty::Normal}, {"hard", Game::Difficulty::Hard}}};
    for (const auto &[name, level] : levels) {
        if (json.contains(name))
            rules.difficulties[level] = parseDifficulty(json[name]);
    }
}

void parsePlayerRules(const Json &json, Game::GameRules &rules)
{
    rules.playerLives = std::max(1, json.value("lives", 3));
    rules.respawnDelay = json.value("respawn_delay", 2.0f);
    rules.invulnerability = json.value("invulnerability", 2.0f);
}

void parseInfiniteRules(const Json &json, Game::GameRules &rules)
{
    rules.infinite.healthGrowthPerLoop = json.value("health_growth_per_loop", 0.2f);
    rules.infinite.speedGrowthPerLoop = json.value("speed_growth_per_loop", 0.05f);
    rules.infinite.fireRateGrowthPerLoop = json.value("fire_rate_growth_per_loop", 0.1f);
    rules.infinite.minFireRate = json.value("min_fire_rate", 0.5f);
    rules.infinite.bulletSpeedGrowthPerLoop = json.value("bullet_speed_growth_per_loop", 0.1f);
    rules.infinite.maxBulletSpeed = json.value("max_bullet_speed", 700.0f);
    rules.infinite.scoreMultiplierPerLoop = json.value("score_multiplier_per_loop", 0.5f);
}

void parseCampaignRules(const Json &json, Game::GameRules &rules)
{
    if (json.contains("levels") && json["levels"].is_array()) {
        rules.campaignLevels.clear();
        for (const auto &level : json["levels"])
            rules.campaignLevels.push_back(level.get<std::string>());
    }
    rules.levelsPerWorld = std::max<std::size_t>(1, json.value<std::size_t>("levels_per_world", 3));
    rules.intermissionDelay = json.value("intermission_delay", 2.0f);
}

// Default configuration calibrated for Game Designers, used when game_rules.json does not say otherwise.
void setDefaultRules(Game::GameRules &rules)
{
    rules.difficulties[Game::Difficulty::Easy] = {0.8f, 0.85f, 0.8f, 0.8f, 0.8f};
    rules.difficulties[Game::Difficulty::Normal] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
    rules.difficulties[Game::Difficulty::Hard] = {1.25f, 1.15f, 1.25f, 1.2f, 1.5f};
    rules.infinite = {0.2f, 0.05f, 0.1f, 0.5f, 0.1f, 700.0f, 0.5f};
    rules.campaignLevels = {"assets/levels/level1.json", "assets/levels/level2.json", "assets/levels/level3.json"};
    rules.intermissionDelay = 2.0f;
}

} // namespace

void Game::applyDefaultRules()
{
    setDefaultRules(_rules);
}

void Game::loadEntitiesConfig(const std::string &filepath)
{
    loadPrefabs(_prefabs, filepath);
}

void Game::loadRules(const std::string &filepath)
{
    try {
        std::ifstream file(filepath);
        if (!file.is_open())
            return;
        Json json;
        file >> json;

        if (hasSection(json, "difficulties"))
            parseDifficulties(json["difficulties"], _rules);
        if (hasSection(json, "player"))
            parsePlayerRules(json["player"], _rules);
        if (hasSection(json, "infinite_mode"))
            parseInfiniteRules(json["infinite_mode"], _rules);
        if (hasSection(json, "campaign"))
            parseCampaignRules(json["campaign"], _rules);
    } catch (const std::exception &e) {
        std::cerr << "Warning: invalid " << filepath << ": " << e.what() << '\n';
    }
}

} // namespace rtype::server
