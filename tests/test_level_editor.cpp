#include "editor/Level.hpp"
#include "editor/Preview.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <fstream>

TEST(LevelEditor, ReloadedLevelKeepsEveryEnemyAndField)
{
    const editor::Level level = editor::loadLevel("assets/levels/level1.json");
    const nlohmann::json saved = editor::toJson(level);

    std::ifstream file("assets/levels/level1.json");
    const nlohmann::json original = nlohmann::json::parse(file);
    std::size_t placements = 0;
    for (const nlohmann::json &wave : saved["waves"])
        placements += wave.value("enemies", nlohmann::json::array()).size() +
                      wave.value("obstacles", nlohmann::json::array()).size();
    EXPECT_EQ(placements, level.enemies.size());
    EXPECT_EQ(saved["waves"].size(), original["waves"].size());
    EXPECT_EQ(saved["level_name"], level.name);
    EXPECT_EQ(saved["waves"][0]["enemies"][0]["type"], "satellite_scout");
}

TEST(LevelEditor, EnemiesAtTheSameTimeShareOneWaveSortedByTime)
{
    editor::Level level;
    level.enemies.push_back({5.f, {{"type", "b"}, {"spawn_y", 100.f}, {"speed", 50.f}, {"hp", 3}}});
    level.enemies.push_back({1.f, {{"type", "a"}, {"spawn_y", 200.f}, {"speed", 50.f}}});
    level.enemies.push_back({5.f, {{"type", "c"}, {"spawn_y", 300.f}, {"speed", 50.f}}});

    const nlohmann::json saved = editor::toJson(level);

    ASSERT_EQ(saved["waves"].size(), 2u);
    EXPECT_EQ(saved["waves"][0]["wave_id"], 1);
    EXPECT_FLOAT_EQ(saved["waves"][0]["delay_before_start"].get<float>(), 1.f);
    EXPECT_EQ(saved["waves"][1]["enemies"].size(), 2u);
    EXPECT_EQ(saved["waves"][1]["enemies"][0]["hp"], 3);
}

TEST(LevelEditor, PreviewSpawnsEnemiesAtTheirTimeAndMovesThemLeft)
{
    editor::Level level;
    level.enemies.push_back({1.f, {{"type", "a"}, {"spawn_y", 200.f}, {"speed", 100.f}}});
    editor::Preview preview(level);

    int count = 0;
    auto countEnemies = [&] {
        count = 0;
        preview.registry().each<editor::Position>([&](engine::ecs::Entity, editor::Position &) { count++; });
    };
    for (int i = 0; i < 30; i++)
        preview.step(1.f / 60.f);
    countEnemies();
    EXPECT_EQ(count, 0);

    for (int i = 0; i < 60; i++)
        preview.step(1.f / 60.f);
    countEnemies();
    EXPECT_EQ(count, 1);
    preview.registry().each<editor::Position>(
        [](engine::ecs::Entity, editor::Position &position) { EXPECT_LT(position.x, editor::Preview::WorldWidth); });
}

TEST(LevelEditor, TilesStayInTheObstaclesListOfTheirWave)
{
    const editor::Level level = editor::loadLevel("tests/data/one_tile.json");
    const nlohmann::json saved = editor::toJson(level);

    ASSERT_EQ(saved["waves"].size(), 1u);
    EXPECT_FALSE(saved["waves"][0].contains("enemies"));
    EXPECT_EQ(saved["waves"][0]["obstacles"].size(), 1u);
}

TEST(LevelEditor, OffersTheEnemyAndTilePrefabsOnly)
{
    const auto types = editor::loadEnemyTypes("assets/config/entities.json");
    const auto find = [&types](const std::string &name) {
        return std::find_if(types.begin(), types.end(), [&name](const auto &type) { return type.name == name; });
    };
    ASSERT_NE(find("satellite_scout"), types.end());
    EXPECT_EQ(find("satellite_scout")->list, "enemies");
    EXPECT_GT(find("satellite_scout")->speed, 0.f);
    ASSERT_NE(find("mars_rock"), types.end());
    EXPECT_EQ(find("mars_rock")->list, "obstacles");
    EXPECT_EQ(find("player_ship"), types.end());
}
