#pragma once

#include "ResourceManager.hpp"
#include "common/Constants.hpp"
#include "editor/Level.hpp"
#include "engine/ecs/Registry.hpp"
#include "render/EntityRenderer.hpp"
#include "render/WorldBackground.hpp"
#include "ui/Button.hpp"

#include <SFML/Graphics.hpp>

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace rtype::client {

// The level editor inside the game. The level is seen as it is played: the world background, the real sprites, and
// time running to the right. Each enemy or tile placed is an entity of an engine Registry; saving writes the format
// read by the server (assets/levels/*.json), and Test plays the level for real on a local server.
class LevelEditor {
public:
    static constexpr const char *DefaultPath = "assets/levels/custom.json";
    static constexpr float TimelineTop = 90.f;                           // screen y of the timeline, below the top bar
    static constexpr float TimelineHeight = 840.f;                       // the palette is below
    static constexpr float TimelineScale = WorldHeight / TimelineHeight; // timeline units per screen pixel
    static constexpr std::size_t PaletteSlots = 10;

    LevelEditor(ResourceManager &resources, const sf::Font &font);

    void open(const std::string &path = DefaultPath);
    void handleEvent(const sf::Event &event, const sf::RenderWindow &window);
    void update(float dt);
    void draw(sf::RenderTarget &target, float time);

    // Saved file to play, when Test was pressed; true when Back was pressed.
    std::optional<std::string> takeTest();
    bool takeBack();

private:
    // One placed enemy or tile.
    struct Spawn {
        std::size_t type = 0;  // index in _types
        float time = 0.f;      // seconds from the start of the level
        float y = 0.f;         // spawn_y
        nlohmann::json fields; // the whole JSON object, so saving keeps the keys the editor does not show
    };

    void load(const std::string &path);
    bool save();
    void place(sf::Vector2f mouse);
    std::optional<engine::ecs::Entity> spawnAt(sf::Vector2f mouse);
    static sf::View timelineView();
    void handleClick(sf::Vector2f screen, sf::Mouse::Button button);
    bool clickBars(sf::Vector2f screen);
    void pressButton(std::size_t index);
    void handleKey(const sf::Event::KeyPressed &key);
    void scroll(float seconds);
    void selectType(int offset);
    void openNextFile();

    float screenX(float time) const;
    float timeAt(float x) const;
    sf::FloatRect boundsOf(const Spawn &spawn) const;
    protocol::EntityState stateOf(engine::ecs::Entity entity, const Spawn &spawn) const;

    // LevelEditorDraw.cpp
    void drawTimeline(sf::RenderTarget &target) const;
    void drawSpawns(sf::RenderTarget &target, float time);
    void drawPalette(sf::RenderTarget &target, float time) const;
    void drawThumbnail(sf::RenderTarget &target, std::size_t type, sf::FloatRect slot, float time) const;
    void drawTopBar(sf::RenderTarget &target) const;
    void drawHint(sf::RenderTarget &target) const;
    sf::FloatRect paletteSlot(std::size_t index) const;
    sf::Text label(const std::string &string, unsigned int size, sf::Color color) const;

    const sf::Font &_font;
    EntityRenderer _renderer;
    WorldBackground _background;
    std::vector<editor::EnemyType> _types;
    engine::ecs::Registry _registry; // one entity per Spawn
    std::string _path;
    std::string _levelName;
    std::size_t _selectedType = 0;
    std::size_t _paletteFirst = 0; // first type shown in the palette
    float _cameraTime = 0.f;       // time at the left edge of the timeline
    sf::Vector2f _mouse;
    std::optional<engine::ecs::Entity> _dragged;
    std::string _status;
    std::vector<std::unique_ptr<ui::Button>> _buttons; // test, save, open, world, back
    std::optional<std::string> _test;
    bool _back = false;
};

} // namespace rtype::client
