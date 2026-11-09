#include "ui/GameConfig.hpp"
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>

using rtype::client::ui::GameConfig;
using Key = sf::Keyboard::Key;

TEST(GameConfigTest, DefaultKeybindingsAreAzerty)
{
    GameConfig config;
    EXPECT_EQ(config.keyUp, Key::Z);
    EXPECT_EQ(config.keyDown, Key::S);
    EXPECT_EQ(config.keyLeft, Key::Q);
    EXPECT_EQ(config.keyRight, Key::D);
    EXPECT_EQ(config.keyShoot, Key::Space);
}

TEST(GameConfigTest, ApplyQwertyPreset)
{
    GameConfig config;
    config.applyQwerty();
    EXPECT_EQ(config.keyUp, Key::W);
    EXPECT_EQ(config.keyDown, Key::S);
    EXPECT_EQ(config.keyLeft, Key::A);
    EXPECT_EQ(config.keyRight, Key::D);
    EXPECT_EQ(config.keyShoot, Key::Space);
}

TEST(GameConfigTest, ApplyAzertyAndResetToDefaults)
{
    GameConfig config;
    config.applyQwerty();
    config.applyAzerty();
    EXPECT_EQ(config.keyUp, Key::Z);
    EXPECT_EQ(config.keyDown, Key::S);
    EXPECT_EQ(config.keyLeft, Key::Q);
    EXPECT_EQ(config.keyRight, Key::D);
    EXPECT_EQ(config.keyShoot, Key::Space);

    config.keyShoot = Key::Enter;
    config.resetToDefaults();
    EXPECT_EQ(config.keyShoot, Key::Space);
}

TEST(GameConfigTest, KeyToStringAndStringToKeyMapping)
{
    EXPECT_EQ(GameConfig::keyToString(Key::A), "A");
    EXPECT_EQ(GameConfig::keyToString(Key::Z), "Z");
    EXPECT_EQ(GameConfig::keyToString(Key::Up), "Up");
    EXPECT_EQ(GameConfig::keyToString(Key::Down), "Down");
    EXPECT_EQ(GameConfig::keyToString(Key::Left), "Left");
    EXPECT_EQ(GameConfig::keyToString(Key::Right), "Right");
    EXPECT_EQ(GameConfig::keyToString(Key::Space), "Space");

    EXPECT_EQ(GameConfig::stringToKey("a"), Key::A);
    EXPECT_EQ(GameConfig::stringToKey("Z"), Key::Z);
    EXPECT_EQ(GameConfig::stringToKey("UP"), Key::Up);
    EXPECT_EQ(GameConfig::stringToKey("down"), Key::Down);
    EXPECT_EQ(GameConfig::stringToKey("left"), Key::Left);
    EXPECT_EQ(GameConfig::stringToKey("right"), Key::Right);
    EXPECT_EQ(GameConfig::stringToKey("space"), Key::Space);
    EXPECT_EQ(GameConfig::stringToKey("ENTER"), Key::Enter);

    EXPECT_EQ(GameConfig::stringToKey("INVALID_KEY_NAME", Key::Unknown), Key::Unknown);
}

TEST(GameConfigTest, ConflictDetection)
{
    GameConfig config;
    config.applyAzerty(); // Z, S, Q, D, Space

    // Key Z is bound to Up
    EXPECT_EQ(config.getConflictAction(Key::Z), "Up");
    EXPECT_EQ(config.getConflictAction(Key::Z, "Up"), "");

    // Key Space is bound to Shoot
    EXPECT_EQ(config.getConflictAction(Key::Space), "Shoot");
    EXPECT_EQ(config.getConflictAction(Key::Space, "Shoot"), "");

    // Key W is free in AZERTY
    EXPECT_EQ(config.getConflictAction(Key::W), "");
}

TEST(GameConfigTest, SaveAndLoadValidConfig)
{
    const std::string testPath = "test_valid_config.json";

    GameConfig original;
    original.applyQwerty();
    original.sfxVolume = 75.f;
    original.musicVolume = 50.f;

    ASSERT_TRUE(original.saveToFile(testPath));

    GameConfig loaded = GameConfig::loadFromFile(testPath);
    EXPECT_EQ(loaded.keyUp, Key::W);
    EXPECT_EQ(loaded.keyDown, Key::S);
    EXPECT_EQ(loaded.keyLeft, Key::A);
    EXPECT_EQ(loaded.keyRight, Key::D);
    EXPECT_EQ(loaded.keyShoot, Key::Space);
    EXPECT_FLOAT_EQ(loaded.sfxVolume, 75.f);
    EXPECT_FLOAT_EQ(loaded.musicVolume, 50.f);

    std::filesystem::remove(testPath);
}

TEST(GameConfigTest, AccessibilitySettingsAreSavedAndOffByDefault)
{
    const std::string testPath = "test_accessibility_config.json";
    EXPECT_FALSE(GameConfig{}.autoFire);
    EXPECT_FALSE(GameConfig{}.highContrast);

    GameConfig original;
    original.autoFire = true;
    original.highContrast = true;
    ASSERT_TRUE(original.saveToFile(testPath));

    const GameConfig loaded = GameConfig::loadFromFile(testPath);
    EXPECT_TRUE(loaded.autoFire);
    EXPECT_TRUE(loaded.highContrast);
    std::filesystem::remove(testPath);
}

TEST(GameConfigTest, LoadCorruptedJsonFallsBackToDefaultsWithoutCrashing)
{
    const std::string testPath = "test_corrupted_syntax.json";

    // Write malformed JSON
    {
        std::ofstream f(testPath);
        f << "{ malformed json: not valid syntax !!!";
    }

    GameConfig loaded = GameConfig::loadFromFile(testPath);
    // Must not crash and must fall back to valid default controls
    EXPECT_EQ(loaded.keyUp, Key::Z);
    EXPECT_EQ(loaded.keyDown, Key::S);
    EXPECT_EQ(loaded.keyLeft, Key::Q);
    EXPECT_EQ(loaded.keyRight, Key::D);
    EXPECT_EQ(loaded.keyShoot, Key::Space);

    std::filesystem::remove(testPath);
}

TEST(GameConfigTest, LoadDuplicateKeyConflictInJsonFallsBackToDefaults)
{
    const std::string testPath = "test_conflicting_config.json";

    // Write JSON with conflicting duplicate keys (e.g. key_up and key_down both set to Z)
    {
        std::ofstream f(testPath);
        f << "{\n"
          << "  \"key_up\": \"Z\",\n"
          << "  \"key_down\": \"Z\",\n"
          << "  \"key_left\": \"Q\",\n"
          << "  \"key_right\": \"D\",\n"
          << "  \"key_shoot\": \"Space\"\n"
          << "}\n";
    }

    GameConfig loaded = GameConfig::loadFromFile(testPath);
    // Conflict detected: must reset to defaults so keyUp != keyDown
    EXPECT_EQ(loaded.keyUp, Key::Z);
    EXPECT_EQ(loaded.keyDown, Key::S);
    EXPECT_NE(loaded.keyUp, loaded.keyDown);

    std::filesystem::remove(testPath);
}

TEST(GameConfigTest, LoadUnknownKeyInJsonFallsBackToDefaults)
{
    const std::string testPath = "test_unknown_key.json";

    // Write JSON with invalid/unknown key string
    {
        std::ofstream f(testPath);
        f << "{\n"
          << "  \"key_up\": \"NON_EXISTENT_KEY_NAME_XYZ\",\n"
          << "  \"key_down\": \"S\",\n"
          << "  \"key_left\": \"Q\",\n"
          << "  \"key_right\": \"D\",\n"
          << "  \"key_shoot\": \"Space\"\n"
          << "}\n";
    }

    GameConfig loaded = GameConfig::loadFromFile(testPath);
    // Invalid key: must reset to defaults so keyUp is valid
    EXPECT_EQ(loaded.keyUp, Key::Z);

    std::filesystem::remove(testPath);
}
