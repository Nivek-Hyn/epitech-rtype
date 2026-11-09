// Drawing of the level editor: background, timeline with the placed sprites, top bar and palette.

#include "editor/LevelEditor.hpp"

#include <algorithm>
#include <cmath>
#include <string>

namespace rtype::client {

namespace {

constexpr float PixelsPerSecond = 100.f;
constexpr float TimelineLeft = 120.f;
constexpr float PaletteTop = 940.f;
const sf::Color Accent(0, 220, 255);

std::string seconds(float time)
{
    const int tenths = static_cast<int>(std::round(time * 10.f));
    return std::to_string(tenths / 10) + "." + std::to_string(tenths % 10) + " s";
}

} // namespace

sf::Text LevelEditor::label(const std::string &string, unsigned int size, sf::Color color) const
{
    sf::Text text(_font, string, size);
    text.setFillColor(color);
    text.setOutlineColor(sf::Color(0, 0, 0, 200));
    text.setOutlineThickness(2.f);
    return text;
}

void LevelEditor::draw(sf::RenderTarget &target, float time)
{
    _background.draw(target);
    const sf::View screen = target.getView();
    target.setView(timelineView());
    drawTimeline(target);
    drawSpawns(target, time);
    target.setView(screen);
    drawTopBar(target);
    drawPalette(target, time);
    drawHint(target);
}

// One line per second, a label every five; the left edge of the screen in game is where enemies appear.
void LevelEditor::drawTimeline(sf::RenderTarget &target) const
{
    const float visible = (WorldWidth * TimelineScale - TimelineLeft) / PixelsPerSecond;
    for (float second = std::ceil(_cameraTime); second <= _cameraTime + visible; second += 1.f) {
        const bool major = static_cast<int>(second) % 5 == 0;
        sf::RectangleShape line({major ? 3.f : 1.f, WorldHeight});
        line.setPosition({screenX(second), 0.f});
        line.setFillColor(major ? sf::Color(255, 255, 255, 70) : sf::Color(255, 255, 255, 25));
        target.draw(line);
        if (major) {
            sf::Text text = label(std::to_string(static_cast<int>(second)) + " s", 26, sf::Color(220, 230, 255));
            text.setPosition({screenX(second) + 8.f, 8.f});
            target.draw(text);
        }
    }
}

void LevelEditor::drawSpawns(sf::RenderTarget &target, float time)
{
    auto &registry = _registry;
    std::optional<std::pair<engine::ecs::Entity, sf::FloatRect>> hovered;
    registry.each<Spawn>([&](engine::ecs::Entity entity, Spawn &spawn) {
        _renderer.draw(target, stateOf(entity, spawn), time, 0.f, std::nullopt);
        if (boundsOf(spawn).contains(_mouse))
            hovered = {entity, boundsOf(spawn)};
    });
    if (!hovered)
        return;
    sf::RectangleShape outline(hovered->second.size);
    outline.setPosition(hovered->second.position);
    outline.setFillColor(sf::Color(0, 220, 255, 40));
    outline.setOutlineThickness(3.f);
    outline.setOutlineColor(Accent);
    target.draw(outline);
    const Spawn &spawn = *registry.get<Spawn>(hovered->first);
    sf::Text text = label(_types[spawn.type].name + "   at " + seconds(spawn.time) + "   y " +
                              std::to_string(static_cast<int>(spawn.y)),
                          26, sf::Color::White);
    text.setPosition(hovered->second.position + sf::Vector2f{0.f, -40.f});
    target.draw(text);
}

void LevelEditor::drawTopBar(sf::RenderTarget &target) const
{
    sf::RectangleShape bar({WorldWidth, TimelineTop});
    bar.setFillColor(sf::Color(6, 10, 24, 225));
    target.draw(bar);
    sf::Text title = label("LEVEL EDITOR - " + _levelName, 30, sf::Color(255, 204, 0));
    title.setPosition({30.f, 10.f});
    target.draw(title);
    sf::Text status = label(_status, 20, sf::Color(180, 200, 230));
    status.setPosition({30.f, 52.f});
    target.draw(status);
    for (const auto &button : _buttons)
        button->draw(target);
}

sf::FloatRect LevelEditor::paletteSlot(std::size_t index) const
{
    return {{24.f + static_cast<float>(index) * 134.f, PaletteTop + 14.f}, {120.f, 120.f}};
}

// Each prefab drawn small in its slot, through a view that frames its sprite.
void LevelEditor::drawThumbnail(sf::RenderTarget &target, std::size_t type, sf::FloatRect slot, float time) const
{
    const editor::EnemyType &look = _types[type];
    const float size = std::max(look.width, look.height) * 1.7f;
    sf::View view({look.width / 2.f, look.offsetY + look.height / 2.f}, {size, size});
    view.setViewport(sf::FloatRect({slot.position.x / WorldWidth, slot.position.y / WorldHeight},
                                   {slot.size.x / WorldWidth, slot.size.y / WorldHeight}));
    const sf::View screen = target.getView();
    target.setView(view);
    const auto kind = look.list == "obstacles" ? protocol::EntityType::Obstacle : protocol::EntityType::Enemy;
    _renderer.draw(target, {0, kind, static_cast<std::uint8_t>(look.variant), 0.f, 0.f}, time, 0.f, std::nullopt);
    target.setView(screen);
}

void LevelEditor::drawPalette(sf::RenderTarget &target, float time) const
{
    sf::RectangleShape panel({WorldWidth, WorldHeight - PaletteTop});
    panel.setPosition({0.f, PaletteTop});
    panel.setFillColor(sf::Color(6, 10, 24, 225));
    target.draw(panel);
    for (std::size_t i = _paletteFirst; i < _types.size() && i < _paletteFirst + PaletteSlots; i++) {
        const sf::FloatRect slot = paletteSlot(i - _paletteFirst);
        sf::RectangleShape frame(slot.size);
        frame.setPosition(slot.position);
        frame.setFillColor(sf::Color(20, 26, 48));
        frame.setOutlineThickness(i == _selectedType ? 4.f : 1.f);
        frame.setOutlineColor(i == _selectedType ? Accent : sf::Color(80, 90, 120));
        target.draw(frame);
        drawThumbnail(target, i, slot, time);
    }
    sf::Text name = label(_types[_selectedType].name + "  (Tab)", 22, Accent);
    name.setPosition({24.f + PaletteSlots * 134.f, PaletteTop + 20.f});
    target.draw(name);
}

void LevelEditor::drawHint(sf::RenderTarget &target) const
{
    sf::Text hint =
        label("Left click: place or drag   Right click: delete\nWheel / arrows: move in time   Ctrl+S: save", 18,
              sf::Color(200, 210, 230));
    hint.setPosition({24.f + PaletteSlots * 134.f, PaletteTop + 58.f});
    target.draw(hint);
}

} // namespace rtype::client
