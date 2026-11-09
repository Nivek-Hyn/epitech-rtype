#include "editor/LevelEditor.hpp"

#include "common/Constants.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <utility>

namespace rtype::client {

namespace {

constexpr float PixelsPerSecond = 100.f;
constexpr float TimelineLeft = 120.f; // timeline x of the camera time
constexpr float TimeSnap = 0.25f;     // seconds

// The files OPEN goes through: the custom level, then the campaign.
const std::array<const char *, 10> Files = {
    LevelEditor::DefaultPath,    "assets/levels/level1.json", "assets/levels/level2.json", "assets/levels/level3.json",
    "assets/levels/level4.json", "assets/levels/level5.json", "assets/levels/level6.json", "assets/levels/level7.json",
    "assets/levels/level8.json", "assets/levels/level9.json"};

} // namespace

LevelEditor::LevelEditor(ResourceManager &resources, const sf::Font &font)
    : _font(font), _renderer(resources), _types(editor::loadEnemyTypes("assets/config/entities.json"))
{
    const std::array<const char *, 5> names = {"TEST", "SAVE", "OPEN", "WORLD", "BACK"};
    for (std::size_t i = 0; i < names.size(); i++)
        _buttons.push_back(std::make_unique<ui::Button>(
            font, names[i], sf::Vector2f(880.f + static_cast<float>(i) * 205.f, 18.f), sf::Vector2f(190.f, 54.f)));
}

void LevelEditor::open(const std::string &path)
{
    load(path);
    _test.reset();
    _back = false;
}

// The timeline is drawn in its own view: time on x, the world height on y, between the top bar and the palette.
sf::View LevelEditor::timelineView()
{
    sf::View view(sf::FloatRect({0.f, 0.f}, {WorldWidth * TimelineScale, WorldHeight}));
    view.setViewport(sf::FloatRect({0.f, TimelineTop / WorldHeight}, {1.f, TimelineHeight / WorldHeight}));
    return view;
}

void LevelEditor::load(const std::string &path)
{
    _registry = engine::ecs::Registry{};
    _path = path;
    _cameraTime = 0.f;
    _levelName = "Custom level";
    if (!std::filesystem::exists(path)) {
        _status = "New level: " + path;
        return;
    }
    try {
        const editor::Level level = editor::loadLevel(path);
        _levelName = level.name;
        for (const editor::Placement &placement : level.enemies) {
            const auto type = std::find_if(_types.begin(), _types.end(), [&placement](const editor::EnemyType &t) {
                return t.name == placement.type();
            });
            if (type == _types.end())
                continue; // a prefab that no longer exists
            const engine::ecs::Entity entity = _registry.spawn();
            _registry.emplace<Spawn>(entity, static_cast<std::size_t>(type - _types.begin()), placement.time,
                                     placement.y(), placement.fields);
        }
        _status = "Opened " + path;
    } catch (const std::exception &error) {
        _status = error.what();
    }
    // Campaign levels 1-3 are world 1, 4-6 world 2, 7-9 world 3.
    const auto file = std::find(Files.begin(), Files.end(), path);
    _background.setWorld(
        file == Files.begin() || file == Files.end() ? 0 : static_cast<std::size_t>(file - Files.begin() - 1) / 3);
}

bool LevelEditor::save()
{
    editor::Level level;
    level.name = _levelName;
    _registry.each<Spawn>([&](engine::ecs::Entity, Spawn &spawn) {
        nlohmann::json fields = spawn.fields;
        fields["type"] = _types[spawn.type].name;
        fields["spawn_y"] = std::round(spawn.y);
        level.enemies.push_back({spawn.time, fields, _types[spawn.type].list});
    });
    try {
        editor::saveLevel(level, _path);
        _status = "Saved " + _path;
        return true;
    } catch (const std::exception &error) {
        _status = error.what();
        return false;
    }
}

float LevelEditor::screenX(float time) const
{
    return TimelineLeft + (time - _cameraTime) * PixelsPerSecond;
}

float LevelEditor::timeAt(float x) const
{
    const float time = std::max(0.f, _cameraTime + (x - TimelineLeft) / PixelsPerSecond);
    return std::round(time / TimeSnap) * TimeSnap;
}

sf::FloatRect LevelEditor::boundsOf(const Spawn &spawn) const
{
    const editor::EnemyType &type = _types[spawn.type];
    return {{screenX(spawn.time), spawn.y + type.offsetY}, {type.width, type.height}};
}

protocol::EntityState LevelEditor::stateOf(engine::ecs::Entity entity, const Spawn &spawn) const
{
    const editor::EnemyType &type = _types[spawn.type];
    const auto kind = type.list == "obstacles" ? protocol::EntityType::Obstacle : protocol::EntityType::Enemy;
    return {entity, kind, static_cast<std::uint8_t>(type.variant), screenX(spawn.time), spawn.y};
}

// The placed entity under the mouse, if any (the last drawn wins, as on screen).
std::optional<engine::ecs::Entity> LevelEditor::spawnAt(sf::Vector2f mouse)
{
    std::optional<engine::ecs::Entity> found;
    _registry.each<Spawn>([&](engine::ecs::Entity entity, Spawn &spawn) {
        if (boundsOf(spawn).contains(mouse))
            found = entity;
    });
    return found;
}

void LevelEditor::place(sf::Vector2f mouse)
{
    const editor::EnemyType &type = _types[_selectedType];
    const float y =
        std::clamp(mouse.y - type.height / 2.f - type.offsetY, -type.offsetY, WorldHeight - type.height - type.offsetY);
    const engine::ecs::Entity entity = _registry.spawn();
    _registry.emplace<Spawn>(entity, _selectedType, timeAt(mouse.x - type.width / 2.f), y,
                             nlohmann::json{{"type", type.name}, {"speed", type.speed}});
    _dragged = entity;
}

void LevelEditor::handleEvent(const sf::Event &event, const sf::RenderWindow &window)
{
    if (const auto *moved = event.getIf<sf::Event::MouseMoved>()) {
        _mouse = window.mapPixelToCoords(moved->position, timelineView());
        const sf::Vector2f screen = window.mapPixelToCoords(moved->position);
        for (auto &button : _buttons)
            button->handleMouseMove(screen);
        if (_dragged) {
            if (Spawn *spawn = _registry.get<Spawn>(*_dragged)) {
                spawn->time = timeAt(_mouse.x - _types[spawn->type].width / 2.f);
                spawn->y = std::clamp(_mouse.y - _types[spawn->type].height / 2.f, 0.f,
                                      WorldHeight - _types[spawn->type].height);
            }
        }
    } else if (const auto *click = event.getIf<sf::Event::MouseButtonPressed>()) {
        _mouse = window.mapPixelToCoords(click->position, timelineView());
        handleClick(window.mapPixelToCoords(click->position), click->button);
    } else if (event.is<sf::Event::MouseButtonReleased>()) {
        _dragged.reset();
    } else if (const auto *wheel = event.getIf<sf::Event::MouseWheelScrolled>()) {
        scroll(-wheel->delta * 2.f);
    } else if (const auto *key = event.getIf<sf::Event::KeyPressed>()) {
        handleKey(*key);
    }
}

// screen: the click in screen coordinates, for the bars; _mouse holds it in timeline coordinates.
void LevelEditor::handleClick(sf::Vector2f screen, sf::Mouse::Button button)
{
    if (button == sf::Mouse::Button::Left && clickBars(screen))
        return;
    if (screen.y < TimelineTop || screen.y > TimelineTop + TimelineHeight)
        return;
    const std::optional<engine::ecs::Entity> under = spawnAt(_mouse);
    if (button == sf::Mouse::Button::Right && under)
        _registry.kill(*under);
    else if (button == sf::Mouse::Button::Left && under)
        _dragged = under;
    else if (button == sf::Mouse::Button::Left)
        place(_mouse);
}

// The buttons of the top bar and the slots of the palette.
bool LevelEditor::clickBars(sf::Vector2f screen)
{
    for (std::size_t i = 0; i < _buttons.size(); i++) {
        if (_buttons[i]->handleMouseClick(screen)) {
            pressButton(i);
            return true;
        }
    }
    for (std::size_t i = _paletteFirst; i < _types.size() && i < _paletteFirst + PaletteSlots; i++) {
        if (paletteSlot(i - _paletteFirst).contains(screen)) {
            _selectedType = i;
            return true;
        }
    }
    return false;
}

// TEST, SAVE, OPEN, WORLD, BACK.
void LevelEditor::pressButton(std::size_t index)
{
    if (index == 0 && save())
        _test = _path;
    else if (index == 1)
        save();
    else if (index == 2)
        openNextFile();
    else if (index == 3)
        _background.setWorld(_background.world() + 1);
    else if (index == 4)
        _back = true;
}

void LevelEditor::handleKey(const sf::Event::KeyPressed &key)
{
    using sf::Keyboard::Key;
    if (key.code == Key::S && key.control)
        save();
    else if (key.code == Key::Escape)
        _back = true;
    else if (key.code == Key::Left)
        scroll(-2.f);
    else if (key.code == Key::Right)
        scroll(2.f);
    else if (key.code == Key::Tab)
        selectType(key.shift ? -1 : 1);
    else if (key.code == Key::Delete || key.code == Key::Backspace) {
        if (const auto under = spawnAt(_mouse))
            _registry.kill(*under);
    }
}

void LevelEditor::scroll(float seconds)
{
    _cameraTime = std::max(0.f, _cameraTime + seconds);
}

void LevelEditor::selectType(int offset)
{
    const int count = static_cast<int>(_types.size());
    _selectedType = static_cast<std::size_t>(((static_cast<int>(_selectedType) + offset) % count + count) % count);
}

void LevelEditor::openNextFile()
{
    const auto current = std::find(Files.begin(), Files.end(), _path);
    const auto next = current == Files.end() || current + 1 == Files.end() ? Files.begin() : current + 1;
    load(*next);
}

void LevelEditor::update(float dt)
{
    _background.update(dt);
    // The palette follows the selected type.
    if (_selectedType < _paletteFirst)
        _paletteFirst = _selectedType;
    else if (_selectedType >= _paletteFirst + PaletteSlots)
        _paletteFirst = _selectedType - PaletteSlots + 1;
}

std::optional<std::string> LevelEditor::takeTest()
{
    return std::exchange(_test, std::nullopt);
}

bool LevelEditor::takeBack()
{
    return std::exchange(_back, false);
}

} // namespace rtype::client
