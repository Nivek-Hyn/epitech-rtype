// Head-up display of a game: score, wave, players and lives, damage flash, banners and end screen.
// Same look as the menus: black panels, thin white lines, white text, the colour of the world for what changes.

#include "game/Session.hpp"

#include "common/Constants.hpp"
#include "render/Looks.hpp"
#include "render/Neon.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace rtype::client {

namespace {

using neon::withAlpha;

const std::array<const char *, 3> WorldNames = {"WORLD 1  ORBITAL BELT", "WORLD 2  MARS", "WORLD 3  SINGULARITY"};
const sf::Color Dim(150, 158, 172);

std::uint8_t alphaOf(float ratio, float maximum)
{
    return static_cast<std::uint8_t>(std::clamp(ratio, 0.f, 1.f) * maximum);
}

// The wave banner fades in, stays, then fades out.
float bannerAlpha(float remaining)
{
    const float progress = remaining / Session::WaveBannerDuration;
    if (progress > 0.8f)
        return std::clamp((1.f - progress) / 0.2f, 0.f, 1.f);
    if (progress < 0.25f)
        return std::clamp(progress / 0.25f, 0.f, 1.f);
    return 1.f;
}

std::string lifeStatus(int lives, bool active, bool wasSpawned)
{
    if (lives <= 0)
        return active ? "LAST CHANCE" : "DOWN";
    if (!active && wasSpawned)
        return "RESPAWNING";
    return std::to_string(lives) + " LIVES";
}

// A panel: black glass, a thin line around it and brighter corners.
void frame(sf::RenderTarget &window, sf::FloatRect rect, float alpha, bool highContrast, sf::Color corner)
{
    neon::Canvas canvas(window, nullptr);
    const sf::Vector2f a = rect.position;
    const sf::Vector2f b = a + rect.size;
    canvas.fill({a, {b.x, a.y}, b, {a.x, b.y}}, sf::Color(0, 0, 0, alphaOf(alpha, highContrast ? 245.f : 228.f)));
    canvas.path({a, {b.x, a.y}, b, {a.x, b.y}}, true, 1.f, withAlpha(looks::Light, 0.22f * alpha));
    constexpr float Arm = 14.f;
    const sf::Color line = withAlpha(corner, alpha);
    canvas.path({{a.x, a.y + Arm}, a, {a.x + Arm, a.y}}, false, 2.f, line);
    canvas.path({{b.x - Arm, a.y}, {b.x, a.y}, {b.x, a.y + Arm}}, false, 2.f, line);
    canvas.path({{b.x, b.y - Arm}, b, {b.x - Arm, b.y}}, false, 2.f, line);
    canvas.path({{a.x + Arm, b.y}, {a.x, b.y}, {a.x, b.y - Arm}}, false, 2.f, line);
}

// A horizontal rule on each side of a centred title.
void rules(sf::RenderTarget &window, sf::Vector2f center, float gap, float length, sf::Color color)
{
    neon::Canvas canvas(window, nullptr);
    canvas.line({center.x - gap - length, center.y}, {center.x - gap, center.y}, 1.5f, color);
    canvas.line({center.x + gap, center.y}, {center.x + gap + length, center.y}, 1.5f, color);
}

} // namespace

sf::Text Session::centeredText(const std::string &string, unsigned int size, sf::Color color, sf::Vector2f center) const
{
    sf::Text text(_resources.getFont("default"), string, size);
    text.setFillColor(color);
    text.setLetterSpacing(1.6f);
    const auto bounds = text.getLocalBounds();
    text.setOrigin({bounds.position.x + bounds.size.x / 2.f, bounds.position.y + bounds.size.y / 2.f});
    text.setPosition(center);
    outline(text);
    return text;
}

// In high contrast, every HUD text gets a black outline so it stays readable over any sprite.
void Session::outline(sf::Text &text) const
{
    if (!_highContrast)
        return;
    text.setOutlineColor(sf::Color::Black);
    text.setOutlineThickness(3.f);
}

// In high contrast, a red ring around each enemy shot: enemy and player shots differ by shape, not only color.
void Session::drawShotMarkers(sf::RenderTarget &window) const
{
    for (const auto &entity : _shown) {
        if (entity.type != protocol::EntityType::EnemyMissile)
            continue;
        sf::CircleShape ring(26.f);
        ring.setOrigin({26.f, 26.f});
        ring.setPosition({entity.x + 18.f, entity.y + 12.f});
        ring.setFillColor(sf::Color::Transparent);
        ring.setOutlineColor(sf::Color(255, 60, 60));
        ring.setOutlineThickness(3.f);
        window.draw(ring);
    }
}

void Session::drawAutoFire(sf::RenderTarget &window) const
{
    if (!_autoFire)
        return;
    sf::Text text(_resources.getFont("default"), "AUTO FIRE  F1", 16);
    text.setLetterSpacing(1.6f);
    text.setFillColor(looks::Light);
    text.setPosition({40.f, 166.f});
    outline(text);
    window.draw(text);
}

void Session::drawWeaponBanner(sf::RenderTarget &window) const
{
    if (_weaponBannerTimer <= 0.f || _weaponBannerText.empty())
        return;
    const float alpha = std::min(1.f, _weaponBannerTimer / 0.4f);
    const sf::Text text =
        centeredText(_weaponBannerText, 22, withAlpha(looks::Light, alpha), {WorldWidth / 2.f, 150.f});
    rules(window, {WorldWidth / 2.f, 152.f}, text.getLocalBounds().size.x / 2.f + 24.f, 90.f,
          withAlpha(looks::Light, 0.5f * alpha));
    window.draw(text);
}

void Session::drawConnecting(sf::RenderTarget &window) const
{
    window.draw(centeredText("CONNECTING TO THE SERVER", 30, looks::Light, {WorldWidth / 2.f, 500.f}));
    window.draw(centeredText("Make sure ./r-type_server is running    ESC  back to the menu", 16, Dim,
                             {WorldWidth / 2.f, 550.f}));
}

void Session::drawHud(sf::RenderTarget &window) const
{
    drawScore(window);
    drawAutoFire(window);
    drawWeaponBanner(window);
    drawBossAlert(window);
    drawPlayers(window);
    drawDamageFlash(window);
    drawWaveBanner(window);
    drawWaitingRoom(window);
    drawPlayerLeft(window);
    drawEndScreen(window);
}

// Score, world, level and wave in a panel, top left.
void Session::drawScore(sf::RenderTarget &window) const
{
    const std::size_t world = static_cast<std::size_t>(_level / 3) % WorldNames.size();
    const sf::Color accent = looks::worldPalette(world).accent;
    frame(window, {{24.f, 20.f}, {400.f, 132.f}}, 1.f, _highContrast, accent);

    sf::Text title(_resources.getFont("default"), WorldNames[world], 15);
    title.setLetterSpacing(2.f);
    title.setFillColor(accent);
    title.setPosition({42.f, 32.f});
    outline(title);
    window.draw(title);

    sf::Text score(_resources.getFont("default"), std::to_string(_score), 40);
    score.setLetterSpacing(1.4f);
    score.setFillColor(looks::Light);
    score.setPosition({40.f, 50.f});
    outline(score);
    window.draw(score);

    sf::Text stage(_resources.getFont("default"),
                   "LEVEL " + std::to_string(_level + 1) + "   WAVE " + std::to_string(std::max(_wave, 1)), 15);
    stage.setLetterSpacing(1.6f);
    stage.setFillColor(Dim);
    stage.setPosition({42.f, 106.f});
    outline(stage);
    window.draw(stage);

    static const std::array<std::pair<const char *, sf::Color>, 3> weapons{{{"BLASTER", sf::Color(120, 220, 255)},
                                                                            {"SPREAD", sf::Color(80, 200, 255)},
                                                                            {"LASER", sf::Color(255, 90, 220)}}};
    sf::Color weaponColor = looks::Light;
    for (const auto &[name, color] : weapons) {
        if (_currentWeapon == name)
            weaponColor = color;
    }
    sf::Text weapon(_resources.getFont("default"), _currentWeapon, 15);
    weapon.setLetterSpacing(1.6f);
    weapon.setFillColor(weaponColor);
    const auto bounds = weapon.getLocalBounds();
    weapon.setPosition({406.f - bounds.size.x, 106.f});
    outline(weapon);
    window.draw(weapon);
}

void Session::drawPanel(sf::RenderTarget &window, sf::Vector2f position, sf::Vector2f size) const
{
    frame(window, {position, size}, 1.f, _highContrast, looks::Light);
}

// A life: a small ship in the player's colour.
void Session::drawHeart(sf::RenderTarget &window, sf::Vector2f center, sf::Color color) const
{
    neon::Canvas canvas(window, nullptr);
    sf::Transform transform;
    transform.translate(center);
    transform.scale({0.42f, 0.42f});
    canvas.setTransform(transform);
    canvas.shape({{36, 0},
                  {14, -5},
                  {-2, -7},
                  {-14, -17},
                  {-24, -17},
                  {-17, -6},
                  {-26, -5},
                  {-26, 5},
                  {-17, 6},
                  {-24, 17},
                  {-14, 17},
                  {-2, 7},
                  {14, 5}},
                 looks::Black, 4.f, color);
}

// While a boss (see render/Looks.hpp) is on screen, top centre: a blinking warning as it arrives, then its name, and
// its health in a bar that fills as it comes in. The health just lost stays white a moment behind the red.
void Session::drawBossAlert(sf::RenderTarget &window) const
{
    const auto boss = std::find_if(_shown.begin(), _shown.end(), [](const protocol::EntityState &entity) {
        return entity.type == protocol::EntityType::Enemy && looks::isBoss(entity.variant);
    });
    if (boss == _shown.end() && _bossBar < 0.005f)
        return;
    const float center = WorldWidth / 2.f;
    const sf::Color corner = boss != _shown.end() ? looks::worldPalette(looks::worldOfEnemy(boss->variant)).accent
                                                  : looks::Light;
    frame(window, {{center - 340.f, 14.f}, {680.f, 66.f}}, std::min(1.f, _bossBar * 4.f + 0.5f), _highContrast, corner);
    if (boss != _shown.end()) {
        const bool warning = _bossTime < 3.f;
        const float blink = _highContrast || !warning ? 1.f : 0.6f + 0.4f * std::sin(_hudTime * 9.f);
        const sf::Color color = warning ? withAlpha(looks::Danger, blink) : looks::Light;
        const sf::Text text =
            centeredText(warning ? "WARNING" : looks::bossName(boss->variant), 17, color, {center, 36.f});
        rules(window, {center, 37.f}, text.getLocalBounds().size.x / 2.f + 20.f, 100.f,
              withAlpha(warning ? looks::Danger : looks::Light, warning ? blink : 0.35f));
        window.draw(text);
    }

    constexpr float Half = 300.f;
    constexpr float Top = 56.f;
    constexpr float Height = 10.f;
    neon::Canvas canvas(window, nullptr);
    const auto bar = [&canvas](float from, float to, sf::Color color) {
        canvas.fill({{from, Top}, {to, Top}, {to, Top + Height}, {from, Top + Height}}, color);
    };
    bar(center - Half - 3.f, center + Half + 3.f, sf::Color(0, 0, 0, 220));
    const float left = center - Half;
    bar(left, left + 2.f * Half * _bossTrail, withAlpha(looks::Light, 0.75f));
    bar(left, left + 2.f * Half * _bossBar, looks::Danger);
    if (_bossBar > 0.01f) // the hot edge of the bar
        bar(left + 2.f * Half * _bossBar - 2.f, left + 2.f * Half * _bossBar, looks::DangerHot);
    for (int i = 1; i < 10; ++i) { // a notch every tenth
        const float x = left + 2.f * Half * static_cast<float>(i) / 10.f;
        canvas.line({x, Top}, {x, Top + Height}, 2.f, sf::Color(0, 0, 0, 200));
    }
    const sf::Vector2f a{center - Half - 3.f, Top - 3.f};
    const sf::Vector2f b{center + Half + 3.f, Top + Height + 3.f};
    canvas.path({a, {b.x, a.y}, b, {a.x, b.y}}, true, 1.f, withAlpha(looks::Light, 0.35f));
}

// One line per player, top right: lives, respawning or dead. Players who never joined are hidden.
void Session::drawPlayers(sf::RenderTarget &window) const
{
    std::array<bool, 4> active{};
    for (const auto &entity : _shown) {
        if (entity.type == protocol::EntityType::Player && entity.variant < 4)
            active[entity.variant] = true;
    }
    std::vector<std::uint8_t> shown;
    for (std::uint8_t player = 0; player < 4; ++player) {
        if (active[player] || _lives[player] > 0 || _localPlayer == player)
            shown.push_back(player);
    }
    if (shown.empty())
        return;
    const float width = 380.f;
    drawPanel(window, {WorldWidth - width - 24.f, 20.f}, {width, 24.f + 32.f * static_cast<float>(shown.size())});
    float y = 34.f;
    for (const std::uint8_t player : shown) {
        drawPlayerLine(window, player, active[player], y);
        y += 32.f;
    }
}

// "P2 YOU" in the player's color, then one small ship per life, or the status when there is none.
void Session::drawPlayerLine(sf::RenderTarget &window, std::uint8_t player, bool active, float y) const
{
    const float left = WorldWidth - 384.f;
    const int lives = _lives[player];
    const bool alive = lives > 0 || active;
    sf::Text name(_resources.getFont("default"),
                  "P" + std::to_string(player + 1) + (_localPlayer == player ? "  YOU" : ""), 17);
    name.setLetterSpacing(1.6f);
    name.setFillColor(alive ? looks::playerColor(player) : Dim);
    name.setPosition({left, y});
    outline(name);
    window.draw(name);
    if (lives <= 0 || (!active && _playerWasSpawned)) {
        sf::Text status(_resources.getFont("default"), lifeStatus(lives, active, _playerWasSpawned), 15);
        status.setLetterSpacing(1.6f);
        status.setFillColor(lives > 0 ? looks::Light : Dim);
        status.setPosition({left + 140.f, y + 2.f});
        outline(status);
        window.draw(status);
        return;
    }
    for (int life = 0; life < lives; life++)
        drawHeart(window, {left + 156.f + static_cast<float>(life) * 34.f, y + 12.f}, looks::playerColor(player));
}

// The edges of the screen turn red for a moment when a ship is hit.
void Session::drawDamageFlash(sf::RenderTarget &window) const
{
    if (_damageFlash <= 0.f)
        return;
    const std::uint8_t alpha = alphaOf(_damageFlash / DamageFlashDuration, _highContrast ? 50.f : 150.f);
    const sf::Color red(255, 30, 50, alpha);
    const sf::Color clear(255, 30, 50, 0);
    neon::Canvas canvas(window, nullptr);
    constexpr float Edge = 220.f;
    canvas.gradient({{0.f, 0.f}, {WorldWidth, Edge}}, red, clear);
    canvas.gradient({{0.f, WorldHeight - Edge}, {WorldWidth, Edge}}, clear, red);
}

void Session::drawWaveBanner(sf::RenderTarget &window) const
{
    if (_waveBanner <= 0.f)
        return;
    const float alpha = bannerAlpha(_waveBanner);
    const sf::Color light = withAlpha(looks::Light, alpha);
    const sf::Color accent = withAlpha(looks::worldPalette(static_cast<std::size_t>(_level / 3)).accent, alpha);
    const sf::Text title = centeredText("WAVE " + std::to_string(_wave), 50, light, {WorldWidth / 2.f, 236.f});
    rules(window, {WorldWidth / 2.f, 240.f}, title.getLocalBounds().size.x / 2.f + 30.f, 220.f,
          withAlpha(looks::Light, 0.5f * alpha));
    window.draw(title);
    window.draw(centeredText("INCOMING SQUADRON", 16, accent, {WorldWidth / 2.f, 284.f}));
}

// While the server waits for players: how many are here, ready button and controls to try.
void Session::drawWaitingRoom(sf::RenderTarget &window) const
{
    if (!_waiting)
        return;
    const auto players = std::count_if(_shown.begin(), _shown.end(), [](const protocol::EntityState &entity) {
        return entity.type == protocol::EntityType::Player;
    });
    frame(window, {{WorldWidth / 2.f - 450.f, 290.f}, {900.f, 260.f}}, 1.f, _highContrast, looks::Light);
    window.draw(centeredText("WAITING FOR PLAYERS", 38, looks::Light, {WorldWidth / 2.f, 340.f}));
    window.draw(centeredText(std::to_string(players) + " / 4 CONNECTED", 16, Dim, {WorldWidth / 2.f, 384.f}));

    neon::Canvas canvas(window, nullptr);
    const sf::Color box = _isReady ? sf::Color(90, 235, 120) : looks::Light;
    const sf::Vector2f a{WorldWidth / 2.f - 220.f, 412.f};
    const sf::Vector2f b{WorldWidth / 2.f + 220.f, 462.f};
    canvas.shape({a, {b.x, a.y}, b, {a.x, b.y}}, sf::Color(0, 0, 0, 220), 2.f, box);
    canvas.flush();
    window.draw(centeredText(_isReady ? "READY  WAITING FOR THE OTHERS" : "SPACE OR ENTER WHEN READY", 18, box,
                             {WorldWidth / 2.f, 436.f}));
    window.draw(
        centeredText("MOVE  ARROWS OR YOUR KEYS     NETWORK  F3     MENU  ESC", 14, Dim, {WorldWidth / 2.f, 510.f}));
}

void Session::drawPlayerLeft(sf::RenderTarget &window) const
{
    if (_leftTimer <= 0.f)
        return;
    window.draw(centeredText(_leftMessage, 24, looks::Light, {WorldWidth / 2.f, 190.f}));
}

// Victory or defeat, decided by the server, with the final score.
void Session::drawEndScreen(sf::RenderTarget &window) const
{
    if (!_gameOver)
        return;
    sf::RectangleShape overlay({WorldWidth, WorldHeight});
    overlay.setFillColor(sf::Color(0, 0, 0, 215));
    window.draw(overlay);
    const sf::Color title = _gameWon ? looks::Light : looks::Danger;
    const sf::Text heading =
        centeredText(_gameWon ? "MISSION ACCOMPLISHED" : "GAME OVER", 66, title, {WorldWidth / 2.f, 430.f});
    rules(window, {WorldWidth / 2.f, 434.f}, heading.getLocalBounds().size.x / 2.f + 34.f, 200.f,
          withAlpha(title, 0.5f));
    window.draw(heading);
    window.draw(centeredText("FINAL SCORE   " + std::to_string(_score), 26, looks::Light, {WorldWidth / 2.f, 520.f}));
    window.draw(centeredText("ESC  BACK TO THE MENU", 16, Dim, {WorldWidth / 2.f, 590.f}));
}

} // namespace rtype::client
