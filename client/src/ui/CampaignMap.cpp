#include "CampaignMap.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>

namespace rtype::client::ui {

namespace {

constexpr float NodeRadius = 38.f;
constexpr float BossRadius = 52.f;

const std::array<sf::Color, 3> WorldColors = {sf::Color(70, 150, 255), sf::Color(255, 112, 46),
                                              sf::Color(176, 92, 255)}; // as in render/Looks.hpp
const std::array<const char *, 3> WorldNames = {"WORLD 1 - ORBITAL BELT", "WORLD 2 - MARS", "WORLD 3 - SINGULARITY"};

nlohmann::json readJson(const std::string &path)
{
    std::ifstream file(path);
    if (!file)
        return nlohmann::json::object();
    return nlohmann::json::parse(file, nullptr, false); // an invalid file reads as "discarded", handled below
}

// The campaign files listed in game_rules.json, with the name written in each level.
std::vector<std::string> campaignNames()
{
    const nlohmann::json rules = readJson("assets/config/game_rules.json");
    std::vector<std::string> names;
    if (!rules.is_object())
        return names;
    for (const auto &path :
         rules.value("campaign", nlohmann::json::object()).value("levels", nlohmann::json::array())) {
        const nlohmann::json level = readJson(path.get<std::string>());
        names.push_back(level.is_object() ? level.value("level_name", "Level") : "Level");
    }
    return names;
}

bool isBoss(std::size_t index)
{
    return index % CampaignMap::LevelsPerWorld == CampaignMap::LevelsPerWorld - 1;
}

// "2-1": world, then level within the world.
std::string levelLabel(std::size_t index)
{
    return std::to_string(index / CampaignMap::LevelsPerWorld + 1) + "-" +
           std::to_string(index % CampaignMap::LevelsPerWorld + 1);
}

} // namespace

CampaignMap::CampaignMap(const sf::Font &font) : _font(font)
{
    const std::vector<std::string> names = campaignNames();
    for (std::size_t i = 0; i < names.size(); i++) {
        const float t = static_cast<float>(i);
        _levels.push_back({names[i], {230.f + t * 182.f, 455.f + std::sin(t * 1.3f) * 100.f}});
    }
    _difficulty = std::make_unique<OptionSelector>(
        font, "DIFFICULTY", std::vector<std::string>{"EASY", "NORMAL", "HARD"}, sf::Vector2f(680.f, 735.f), 1);
    _playButton = std::make_unique<Button>(font, "PLAY", sf::Vector2f(680.f, 808.f), sf::Vector2f(260.f, 58.f));
    _backButton = std::make_unique<Button>(font, "BACK", sf::Vector2f(980.f, 808.f), sf::Vector2f(260.f, 58.f));
}

void CampaignMap::open(std::size_t unlocked)
{
    _unlocked = std::min(unlocked, _levels.empty() ? 0 : _levels.size() - 1);
    select(_unlocked); // start on the newest level reached
    _choice.reset();
    _back = false;
}

void CampaignMap::select(std::size_t index)
{
    if (index <= _unlocked && index < _levels.size())
        _selected = index;
}

void CampaignMap::play()
{
    if (!_levels.empty())
        _choice = Choice{_selected, static_cast<int>(_difficulty->getSelectedIndex())};
}

void CampaignMap::handleEvent(const sf::Event &event, const sf::RenderWindow &window)
{
    if (const auto *moved = event.getIf<sf::Event::MouseMoved>()) {
        const sf::Vector2f mouse = window.mapPixelToCoords(moved->position);
        _hovered.reset();
        for (std::size_t i = 0; i < _levels.size(); i++) {
            const sf::Vector2f d = mouse - _levels[i].position;
            if (d.x * d.x + d.y * d.y < BossRadius * BossRadius)
                _hovered = i;
        }
        _difficulty->handleMouseMove(mouse);
        _playButton->handleMouseMove(mouse);
        _backButton->handleMouseMove(mouse);
    } else if (const auto *click = event.getIf<sf::Event::MouseButtonPressed>()) {
        const sf::Vector2f mouse = window.mapPixelToCoords(click->position);
        if (_hovered)
            select(*_hovered);
        _difficulty->handleMouseClick(mouse);
        if (_playButton->handleMouseClick(mouse))
            play();
        else if (_backButton->handleMouseClick(mouse))
            _back = true;
    } else if (const auto *key = event.getIf<sf::Event::KeyPressed>()) {
        if (key->code == sf::Keyboard::Key::Right && _selected + 1 <= _unlocked)
            select(_selected + 1);
        else if (key->code == sf::Keyboard::Key::Left && _selected > 0)
            select(_selected - 1);
        else if (key->code == sf::Keyboard::Key::Enter || key->code == sf::Keyboard::Key::Space)
            play();
        else if (key->code == sf::Keyboard::Key::Escape)
            _back = true;
        else
            _difficulty->handleKeyPress(key->code);
    }
}

std::optional<CampaignMap::Choice> CampaignMap::takeChoice()
{
    return std::exchange(_choice, std::nullopt);
}

bool CampaignMap::takeBack()
{
    return std::exchange(_back, false);
}

sf::Text CampaignMap::text(const std::string &string, unsigned int size, sf::Color color, sf::Vector2f center) const
{
    sf::Text label(_font, string, size);
    label.setFillColor(color);
    label.setLetterSpacing(1.8f);
    label.setOutlineColor(sf::Color(0, 0, 0, 200));
    label.setOutlineThickness(2.f);
    const auto bounds = label.getLocalBounds();
    label.setOrigin({bounds.position.x + bounds.size.x / 2.f, bounds.position.y + bounds.size.y / 2.f});
    label.setPosition(center);
    return label;
}

void CampaignMap::draw(sf::RenderTarget &window) const
{
    window.draw(text("CAMPAIGN", 56, sf::Color::White, {960.f, 205.f}));
    drawWorldLabels(window);
    drawPath(window);
    for (std::size_t i = 0; i < _levels.size(); i++)
        drawNode(window, i);
    drawPanel(window);
}

void CampaignMap::drawWorldLabels(sf::RenderTarget &window) const
{
    for (std::size_t world = 0; world * LevelsPerWorld < _levels.size(); world++) {
        const float x =
            _levels[world * LevelsPerWorld + 1 < _levels.size() ? world * LevelsPerWorld + 1 : world * LevelsPerWorld]
                .position.x;
        window.draw(
            text(WorldNames[world % WorldNames.size()], 24, WorldColors[world % WorldColors.size()], {x, 272.f}));
    }
}

// Dots between the nodes; the part already travelled is bright.
void CampaignMap::drawPath(sf::RenderTarget &window) const
{
    for (std::size_t i = 0; i + 1 < _levels.size(); i++) {
        const sf::Vector2f from = _levels[i].position;
        const sf::Vector2f step = (_levels[i + 1].position - from) / 10.f;
        const sf::Color color = i < _unlocked ? sf::Color(238, 244, 255) : sf::Color(255, 255, 255, 50);
        for (int dot = 1; dot < 10; dot++) {
            sf::CircleShape point(3.f);
            point.setOrigin({3.f, 3.f});
            point.setPosition(from + step * static_cast<float>(dot));
            point.setFillColor(color);
            window.draw(point);
        }
    }
}

// A ring of light in the colour of its world; a locked level is dim with a padlock; the selected one pulses and
// turns white; a boss level is bigger.
void CampaignMap::drawNode(sf::RenderTarget &window, std::size_t index) const
{
    const Level &level = _levels[index];
    const bool locked = index > _unlocked;
    const bool selected = index == _selected;
    const float pulse = selected ? 1.f + 0.06f * std::sin(_time * 5.f) : 1.f;
    const float radius = (isBoss(index) ? BossRadius : NodeRadius) * pulse;
    const sf::Color color =
        locked ? sf::Color(255, 255, 255, 60) : WorldColors[(index / LevelsPerWorld) % WorldColors.size()];

    if (selected) {
        sf::CircleShape outer(radius + 12.f, 48);
        outer.setOrigin({radius + 12.f, radius + 12.f});
        outer.setPosition(level.position);
        outer.setFillColor(sf::Color::Transparent);
        outer.setOutlineThickness(1.f);
        outer.setOutlineColor(sf::Color(255, 255, 255, 120));
        window.draw(outer);
    }
    sf::CircleShape node(radius, 48);
    node.setOrigin({radius, radius});
    node.setPosition(level.position);
    node.setFillColor(sf::Color(0, 0, 0, 235));
    node.setOutlineThickness(selected ? 4.f : 3.f);
    node.setOutlineColor(selected ? sf::Color::White : color);
    window.draw(node);

    if (locked)
        drawLock(window, level.position);
    else
        window.draw(text(isBoss(index) ? "BOSS" : levelLabel(index), isBoss(index) ? 22 : 26, sf::Color::White,
                         level.position));
    if (index < _unlocked) // already passed
        window.draw(text("CLEAR", 13, color, level.position + sf::Vector2f{0.f, -radius - 16.f}));
}

void CampaignMap::drawLock(sf::RenderTarget &window, sf::Vector2f center) const
{
    sf::CircleShape shackle(10.f);
    shackle.setOrigin({10.f, 10.f});
    shackle.setPosition(center + sf::Vector2f{0.f, -8.f});
    shackle.setFillColor(sf::Color::Transparent);
    shackle.setOutlineThickness(2.f);
    shackle.setOutlineColor(sf::Color(255, 255, 255, 140));
    window.draw(shackle);
    sf::RectangleShape body({26.f, 20.f});
    body.setOrigin({13.f, 0.f});
    body.setPosition(center + sf::Vector2f{0.f, -2.f});
    body.setFillColor(sf::Color::Black); // hides the lower half of the shackle
    body.setOutlineThickness(2.f);
    body.setOutlineColor(sf::Color(255, 255, 255, 140));
    window.draw(body);
}

// The selected level, the difficulty and the buttons.
void CampaignMap::drawPanel(sf::RenderTarget &window) const
{
    sf::RectangleShape panel({1000.f, 285.f});
    panel.setPosition({460.f, 608.f}); // between the map and the ground of the world
    panel.setFillColor(sf::Color(0, 0, 0, 242));
    panel.setOutlineThickness(1.f);
    panel.setOutlineColor(WorldColors[world() % WorldColors.size()]);
    window.draw(panel);
    if (!_levels.empty())
        window.draw(text(_levels[_selected].name, 32, sf::Color::White, {960.f, 660.f}));
    _difficulty->draw(window);
    _playButton->draw(window);
    _backButton->draw(window);
}

} // namespace rtype::client::ui
