#include "editor/Level.hpp"
#include "editor/Preview.hpp"

#include "engine/core/FixedTimestep.hpp"

#include <SFML/Graphics.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

namespace {

constexpr unsigned int WindowWidth = 1600;
constexpr unsigned int WindowHeight = 900;
constexpr float Top = 90.f; // the timeline starts below the help text
constexpr float Bottom = 870.f;
constexpr float WorldHeight = 1080.f;
constexpr float PixelsPerSecond = 40.f;
constexpr float TimeSnap = 0.5f; // enemies placed in the same half-second share a wave
constexpr float PickRadius = 12.f;

float toScreenY(float worldY)
{
    return Top + worldY * (Bottom - Top) / WorldHeight;
}
float toWorldY(float screenY)
{
    return std::clamp((screenY - Top) * WorldHeight / (Bottom - Top), 0.f, WorldHeight - 72.f);
}

// One color per type index, spread around the hue circle; the name is also shown on hover.
sf::Color typeColor(std::size_t index)
{
    static const sf::Color palette[] = {{230, 80, 80},   {80, 180, 230}, {240, 200, 60}, {120, 220, 120},
                                        {200, 120, 230}, {250, 150, 60}, {90, 230, 200}, {240, 240, 240}};
    return palette[index % std::size(palette)];
}

class Editor {
public:
    Editor(std::string path, std::vector<editor::EnemyType> types, const sf::Font &font)
        : _path(std::move(path)), _types(std::move(types)), _font(font)
    {
        if (std::filesystem::exists(_path))
            _level = editor::loadLevel(_path);
        _status = "Opened " + _path;
    }

    void run()
    {
        sf::RenderWindow window(sf::VideoMode({WindowWidth, WindowHeight}), "R-Type level editor");
        window.setVerticalSyncEnabled(true);
        engine::core::FixedTimestep clock(60.0);

        while (window.isOpen()) {
            while (const std::optional event = window.pollEvent())
                handle(window, *event);
            const int ticks = clock.advance();
            if (_preview)
                for (int i = 0; i < ticks; i++)
                    _preview->step(clock.step());
            draw(window);
        }
    }

private:
    void handle(sf::RenderWindow &window, const sf::Event &event)
    {
        if (event.is<sf::Event::Closed>())
            window.close();
        else if (const auto *key = event.getIf<sf::Event::KeyPressed>())
            handleKey(*key);
        else if (const auto *wheel = event.getIf<sf::Event::MouseWheelScrolled>())
            selectType(wheel->delta > 0 ? -1 : 1);
        else if (const auto *mouse = event.getIf<sf::Event::MouseMoved>())
            _mouse = sf::Vector2f(mouse->position);
        else if (const auto *click = event.getIf<sf::Event::MouseButtonPressed>(); click && !_preview) {
            const sf::Vector2f position(click->position);
            if (position.y < Top)
                return;
            if (click->button == sf::Mouse::Button::Left)
                add(position);
            else if (click->button == sf::Mouse::Button::Right)
                remove(position);
        }
    }

    void handleKey(const sf::Event::KeyPressed &key)
    {
        using sf::Keyboard::Key;
        if (key.code == Key::S && key.control)
            save();
        else if (key.code == Key::P)
            togglePreview();
        else if (key.code == Key::Tab)
            selectType(key.shift ? -1 : 1);
        else if (key.code == Key::Left)
            _scroll = std::max(0.f, _scroll - 5.f);
        else if (key.code == Key::Right)
            _scroll += 5.f;
        else if (key.code == Key::Add || key.code == Key::Equal)
            _types[_selected].speed += 10.f;
        else if ((key.code == Key::Subtract || key.code == Key::Hyphen) && _types[_selected].speed > 10.f)
            _types[_selected].speed -= 10.f;
    }

    float toScreenX(float time) const { return (time - _scroll) * PixelsPerSecond + 20.f; }
    float toTime(float screenX) const
    {
        const float time = std::max(0.f, (screenX - 20.f) / PixelsPerSecond + _scroll);
        return std::round(time / TimeSnap) * TimeSnap;
    }

    void selectType(int offset)
    {
        const int count = static_cast<int>(_types.size());
        _selected = static_cast<std::size_t>(((static_cast<int>(_selected) + offset) % count + count) % count);
    }

    void add(sf::Vector2f position)
    {
        const editor::EnemyType &type = _types[_selected];
        nlohmann::json fields = {
            {"type", type.name}, {"spawn_y", std::round(toWorldY(position.y))}, {"speed", type.speed}};
        _level.enemies.push_back({toTime(position.x), fields, type.list});
        _status = "Added " + type.name;
    }

    std::optional<std::size_t> under(sf::Vector2f position) const
    {
        for (std::size_t i = 0; i < _level.enemies.size(); i++) {
            const editor::Placement &placement = _level.enemies[i];
            if (std::hypot(toScreenX(placement.time) - position.x, toScreenY(placement.y()) - position.y) < PickRadius)
                return i;
        }
        return std::nullopt;
    }

    void remove(sf::Vector2f position)
    {
        if (const std::optional<std::size_t> index = under(position)) {
            _status = "Removed " + _level.enemies[*index].type();
            _level.enemies.erase(_level.enemies.begin() + static_cast<std::ptrdiff_t>(*index));
        }
    }

    void save()
    {
        try {
            editor::saveLevel(_level, _path);
            _status = "Saved " + _path;
        } catch (const std::exception &error) {
            _status = error.what();
        }
    }

    void togglePreview()
    {
        if (_preview)
            _preview.reset();
        else
            _preview.emplace(_level);
    }

    std::size_t typeIndex(const std::string &name) const
    {
        for (std::size_t i = 0; i < _types.size(); i++)
            if (_types[i].name == name)
                return i;
        return _types.size(); // unknown type: last palette color
    }

    void text(sf::RenderWindow &window, const std::string &string, sf::Vector2f position, unsigned int size = 18)
    {
        sf::Text label(_font, string, size);
        label.setPosition(position);
        window.draw(label);
    }

    void drawEnemy(sf::RenderWindow &window, sf::Vector2f center, std::size_t type)
    {
        sf::CircleShape dot(8.f);
        dot.setOrigin({8.f, 8.f});
        dot.setPosition(center);
        dot.setFillColor(typeColor(type));
        dot.setOutlineColor(sf::Color::Black);
        dot.setOutlineThickness(2.f);
        window.draw(dot);
    }

    void drawTimeline(sf::RenderWindow &window)
    {
        for (float second = std::floor(_scroll); toScreenX(second) < WindowWidth; second += 1.f) {
            const bool major = static_cast<int>(second) % 5 == 0;
            sf::RectangleShape line({1.f, Bottom - Top});
            line.setPosition({toScreenX(second), Top});
            line.setFillColor(major ? sf::Color(80, 80, 110) : sf::Color(45, 45, 65));
            window.draw(line);
            if (major)
                text(window, std::to_string(static_cast<int>(second)) + "s", {toScreenX(second) + 3.f, Bottom}, 14);
        }
        for (const editor::Placement &placement : _level.enemies)
            drawEnemy(window, {toScreenX(placement.time), toScreenY(placement.y())}, typeIndex(placement.type()));

        if (const std::optional<std::size_t> index = under(_mouse)) {
            const editor::Placement &placement = _level.enemies[*index];
            text(window,
                 placement.type() + "  t=" + std::to_string(placement.time).substr(0, 4) +
                     "s  y=" + std::to_string(static_cast<int>(placement.y())),
                 _mouse + sf::Vector2f(14.f, -10.f), 16);
        }
    }

    // The preview shows the world as the players see it, scaled to the timeline area.
    void drawPreview(sf::RenderWindow &window)
    {
        const float scale = WindowWidth / editor::Preview::WorldWidth;
        _preview->registry().each<editor::Position, editor::Source>(
            [&](engine::ecs::Entity, editor::Position &position, editor::Source &source) {
                const float y = toScreenY(position.y);
                drawEnemy(window, {position.x * scale, y}, typeIndex(_level.enemies[source.index].type()));
            });
        text(window, "PREVIEW  t=" + std::to_string(static_cast<int>(_preview->time())) + "s  (P to stop)",
             {20.f, Top + 10.f}, 20);
    }

    void draw(sf::RenderWindow &window)
    {
        window.clear(sf::Color(25, 25, 40));
        if (_preview)
            drawPreview(window);
        else
            drawTimeline(window);

        const editor::EnemyType &type = _types[_selected];
        text(window,
             "Left click: add   Right click: remove   Wheel/Tab: type   +/-: speed   Arrows: scroll   P: preview   "
             "Ctrl+S: save",
             {20.f, 10.f}, 16);
        text(window, "Type: " + type.name + "   speed: " + std::to_string(static_cast<int>(type.speed)), {20.f, 35.f});
        drawEnemy(window, {10.f, 46.f}, _selected);
        text(window, _level.name + "  |  " + std::to_string(_level.enemies.size()) + " enemies  |  " + _status,
             {20.f, 60.f}, 16);
        window.display();
    }

    std::string _path;
    std::vector<editor::EnemyType> _types;
    const sf::Font &_font;
    editor::Level _level;
    std::optional<editor::Preview> _preview;
    std::size_t _selected = 0;
    float _scroll = 0.f;
    sf::Vector2f _mouse;
    std::string _status;
};

} // namespace

// Usage: ./r-type_level_editor [level.json]   (run from the repository or installed game directory)
int main(int argc, char **argv)
{
    const std::string path = argc > 1 ? argv[1] : "assets/levels/new_level.json";
    try {
        sf::Font font;
        if (!font.openFromFile("assets/font.ttf"))
            throw std::runtime_error("Cannot open assets/font.ttf");
        Editor editor(path, editor::loadEnemyTypes("assets/config/entities.json"), font);
        editor.run();
    } catch (const std::exception &error) {
        std::cerr << "level editor: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
