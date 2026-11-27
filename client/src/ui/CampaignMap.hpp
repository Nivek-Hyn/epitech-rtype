#pragma once

#include "Button.hpp"
#include "OptionSelector.hpp"

#include <SFML/Graphics.hpp>

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace rtype::client::ui {

// The campaign as a map of levels, like mobile games: one node per level along a path, three worlds of three
// levels, the last one of each world being its boss. A level is playable once the previous one was reached.
class CampaignMap {
public:
    struct Choice {
        std::size_t level = 0;
        int difficulty = 1; // 0 easy, 1 normal, 2 hard
    };

    // Reads the campaign levels and their names from assets/config/game_rules.json and assets/levels.
    explicit CampaignMap(const sf::Font &font);

    // unlocked: index of the last playable level (saved in config.json).
    void open(std::size_t unlocked);
    void handleEvent(const sf::Event &event, const sf::RenderWindow &window);
    void update(float dt) { _time += dt; }
    void draw(sf::RenderTarget &window) const;

    // The world of the selected level, for the background.
    std::size_t world() const { return _selected / LevelsPerWorld; }
    std::size_t levelCount() const { return _levels.size(); }
    std::optional<Choice> takeChoice();
    bool takeBack();

    static constexpr std::size_t LevelsPerWorld = 3;

private:
    struct Level {
        std::string name;
        sf::Vector2f position;
    };

    void select(std::size_t index);
    void play();
    void drawPath(sf::RenderTarget &window) const;
    void drawNode(sf::RenderTarget &window, std::size_t index) const;
    void drawLock(sf::RenderTarget &window, sf::Vector2f center) const;
    void drawWorldLabels(sf::RenderTarget &window) const;
    void drawPanel(sf::RenderTarget &window) const;
    sf::Text text(const std::string &string, unsigned int size, sf::Color color, sf::Vector2f center) const;

    const sf::Font &_font;
    std::vector<Level> _levels;
    std::size_t _unlocked = 0;
    std::size_t _selected = 0;
    std::optional<std::size_t> _hovered;
    float _time = 0.f;
    std::unique_ptr<OptionSelector> _difficulty;
    std::unique_ptr<Button> _playButton;
    std::unique_ptr<Button> _backButton;
    std::optional<Choice> _choice;
    bool _back = false;
};

} // namespace rtype::client::ui
