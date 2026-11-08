// Head-up display of a game: score, wave, players and lives, damage flash, banners and end screen.

#include "game/Session.hpp"

#include "common/Constants.hpp"

#include <algorithm>
#include <string>

namespace rtype::client {

namespace {

const std::array<sf::Color, 4> PlayerColors = {sf::Color(0, 230, 255), sf::Color(255, 170, 40), sf::Color(80, 255, 120),
                                               sf::Color(240, 110, 255)};

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
        return active ? "LAST CHANCE" : "DEAD";
    if (!active && wasSpawned)
        return "RESPAWNING... (" + std::to_string(lives) + " LIVES)";
    return std::to_string(lives) + " LIVES";
}

} // namespace

sf::Text Session::centeredText(const std::string &string, unsigned int size, sf::Color color, sf::Vector2f center) const
{
    sf::Text text(_resources.getFont("default"), string, size);
    text.setFillColor(color);
    const auto bounds = text.getLocalBounds();
    text.setOrigin({bounds.size.x / 2.f, bounds.size.y / 2.f});
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
void Session::drawShotMarkers(sf::RenderWindow &window) const
{
    for (const auto &entity : _shown) {
        if (entity.type != protocol::EntityType::EnemyMissile)
            continue;
        sf::CircleShape ring(26.f);
        ring.setOrigin({26.f, 26.f});
        ring.setPosition({entity.x, entity.y});
        ring.setFillColor(sf::Color::Transparent);
        ring.setOutlineColor(sf::Color(255, 60, 60));
        ring.setOutlineThickness(3.f);
        window.draw(ring);
    }
}

void Session::drawAutoFire(sf::RenderWindow &window) const
{
    if (!_autoFire)
        return;
    sf::Text text(_resources.getFont("default"), "AUTO FIRE ON (F1)", 20);
    text.setFillColor(sf::Color(255, 204, 0));
    text.setPosition({45.f, 122.f});
    outline(text);
    window.draw(text);
}

void Session::drawWeaponBanner(sf::RenderWindow &window) const
{
    if (_weaponBannerTimer <= 0.f || _weaponBannerText.empty())
        return;
    const float alpha = std::min(1.f, _weaponBannerTimer / 0.4f);
    sf::RectangleShape banner({460.f, 44.f});
    banner.setOrigin({230.f, 22.f});
    banner.setPosition({WorldWidth / 2.f, 110.f});
    banner.setFillColor(sf::Color(10, 20, 35, alphaOf(alpha, 220.f)));
    banner.setOutlineThickness(2.f);
    banner.setOutlineColor(sf::Color(0, 230, 255, alphaOf(alpha, 255.f)));
    window.draw(banner);
    window.draw(centeredText(_weaponBannerText, 20, sf::Color(255, 230, 80, alphaOf(alpha, 255.f)),
                             {WorldWidth / 2.f, 110.f}));
}

void Session::drawConnecting(sf::RenderWindow &window) const
{
    sf::Text text(_resources.getFont("default"),
                  "CONNECTING TO SERVER...\nEnsure ./r-type_server is running\n\n[ESC] Return to Menu", 30);
    text.setFillColor(sf::Color(255, 204, 0));
    text.setPosition({600.f, 480.f});
    window.draw(text);
}

void Session::drawHud(sf::RenderWindow &window) const
{
    drawScore(window);
    drawAutoFire(window);
    drawWeaponBanner(window);
    drawBossAlert(window);
    drawPlayers(window);
    drawDamageFlash(window);
    drawWaveBanner(window);
    drawWaitingRoom(window);
    drawEndScreen(window);
}

// Score and wave, top left.
void Session::drawScore(sf::RenderWindow &window) const
{
    sf::Text score(_resources.getFont("default"), "SCORE: " + std::to_string(_score), 28);
    score.setFillColor(sf::Color(0, 230, 255));
    score.setPosition({45.f, 25.f});
    outline(score);
    window.draw(score);

    sf::Text stage(_resources.getFont("default"), "STAGE 1 - WAVE " + std::to_string(_wave), 20);
    stage.setFillColor(sf::Color(255, 204, 0));
    stage.setPosition({45.f, 62.f});
    outline(stage);
    window.draw(stage);

    sf::Text weapon(_resources.getFont("default"), "WEAPON: " + _currentWeapon, 20);
    weapon.setFillColor(_currentWeapon == "LASER" ? sf::Color(255, 90, 220)
                       : (_currentWeapon == "SPREAD" ? sf::Color(80, 200, 255) : sf::Color(200, 220, 240)));
    weapon.setPosition({45.f, 92.f});
    outline(weapon);
    window.draw(weapon);
}

// A banner while the heavy cruiser boss (enemy variant 5) is on screen.
void Session::drawBossAlert(sf::RenderWindow &window) const
{
    const bool boss = std::any_of(_shown.begin(), _shown.end(), [](const protocol::EntityState &entity) {
        return entity.type == protocol::EntityType::Enemy && entity.variant == 5;
    });
    if (!boss)
        return;
    sf::RectangleShape background({360.f, 24.f});
    background.setOrigin({180.f, 12.f});
    background.setPosition({WorldWidth / 2.f, 45.f});
    background.setFillColor(sf::Color(20, 10, 15, 220));
    background.setOutlineThickness(2.f);
    background.setOutlineColor(sf::Color(255, 50, 70));
    window.draw(background);
    window.draw(centeredText("WARNING - BOSS ENCOUNTER", 16, sf::Color(255, 70, 70), {WorldWidth / 2.f, 43.f}));
}

// One line per player, top right: lives, respawning or dead. Players who never joined are hidden.
void Session::drawPlayers(sf::RenderWindow &window) const
{
    std::array<bool, 4> active{};
    for (const auto &entity : _shown) {
        if (entity.type == protocol::EntityType::Player && entity.variant < 4)
            active[entity.variant] = true;
    }
    float y = 25.f;
    for (std::uint8_t player = 0; player < 4; ++player) {
        const bool local = _localPlayer == player;
        const int lives = _lives[player];
        if (!active[player] && lives == 0 && !local)
            continue;
        const std::string label = "P" + std::to_string(player + 1) + (local ? " (YOU)" : "");
        sf::Text text(_resources.getFont("default"),
                      label + " : " + lifeStatus(lives, active[player], _playerWasSpawned), 20);
        text.setFillColor(lives > 0 ? PlayerColors[player]
                                    : (active[player] ? sf::Color(255, 200, 50) : sf::Color(140, 140, 140)));
        text.setOrigin({text.getLocalBounds().size.x, 0.f});
        text.setPosition({WorldWidth - 45.f, y});
        outline(text);
        window.draw(text);
        y += 26.f;
    }
}

// The screen turns red for a moment when a ship is hit.
void Session::drawDamageFlash(sf::RenderWindow &window) const
{
    if (_damageFlash <= 0.f)
        return;
    sf::RectangleShape flash({WorldWidth, WorldHeight});
    // Softer in high contrast: no bright full-screen flash.
    flash.setFillColor(sf::Color(255, 0, 0, alphaOf(_damageFlash / DamageFlashDuration, _highContrast ? 50.f : 135.f)));
    window.draw(flash);
}

void Session::drawWaveBanner(sf::RenderWindow &window) const
{
    if (_waveBanner <= 0.f)
        return;
    const float alpha = bannerAlpha(_waveBanner);
    sf::RectangleShape banner({640.f, 100.f});
    banner.setOrigin({320.f, 50.f});
    banner.setPosition({WorldWidth / 2.f, 250.f});
    banner.setFillColor(sf::Color(10, 15, 30, alphaOf(alpha, 220.f)));
    banner.setOutlineThickness(2.f);
    banner.setOutlineColor(sf::Color(255, 204, 0, alphaOf(alpha, 255.f)));
    window.draw(banner);
    window.draw(centeredText("--- WAVE " + std::to_string(_wave) + " ---", 44,
                             sf::Color(255, 204, 0, alphaOf(alpha, 255.f)), {WorldWidth / 2.f, 235.f}));
    window.draw(centeredText("INCOMING ENEMY SQUADRON", 18, sf::Color(0, 230, 255, alphaOf(alpha, 240.f)),
                             {WorldWidth / 2.f, 275.f}));
}

// While the server waits for players: how many are here, ready button and controls to try.
void Session::drawWaitingRoom(sf::RenderWindow &window) const
{
    if (!_waiting)
        return;
    const auto players = std::count_if(_shown.begin(), _shown.end(), [](const protocol::EntityState &entity) {
        return entity.type == protocol::EntityType::Player;
    });
    sf::RectangleShape panel({900.f, 270.f});
    panel.setOrigin({450.f, 135.f});
    panel.setPosition({WorldWidth / 2.f, 420.f});
    panel.setFillColor(sf::Color(10, 15, 30, 220));
    panel.setOutlineThickness(2.f);
    panel.setOutlineColor(sf::Color(0, 230, 255));
    window.draw(panel);
    window.draw(centeredText("WAITING FOR PLAYERS", 40, sf::Color(0, 230, 255), {WorldWidth / 2.f, 335.f}));
    window.draw(centeredText(std::to_string(players) + " / 4 connected", 20, sf::Color(180, 200, 230),
                             {WorldWidth / 2.f, 375.f}));

    // Ready indicator / button
    sf::RectangleShape readyBox({420.f, 48.f});
    readyBox.setOrigin({210.f, 24.f});
    readyBox.setPosition({WorldWidth / 2.f, 420.f});
    if (_isReady) {
        readyBox.setFillColor(sf::Color(0, 140, 60, 200));
        readyBox.setOutlineColor(sf::Color(0, 255, 120));
        readyBox.setOutlineThickness(2.f);
        window.draw(readyBox);
        window.draw(centeredText("READY ! (WAITING FOR OTHERS)", 20, sf::Color(255, 255, 255),
                                 {WorldWidth / 2.f, 420.f}));
    } else {
        readyBox.setFillColor(sf::Color(140, 90, 0, 200));
        readyBox.setOutlineColor(sf::Color(255, 204, 0));
        readyBox.setOutlineThickness(2.f);
        window.draw(readyBox);
        window.draw(centeredText("PRESS SPACE / ENTER TO READY", 20, sf::Color(255, 255, 255),
                                 {WorldWidth / 2.f, 420.f}));
    }

    window.draw(centeredText("Move: arrows or your keys    Network stats: F3    Menu: ESC", 18,
                             sf::Color(200, 210, 230), {WorldWidth / 2.f, 475.f}));
}

// Victory or defeat, decided by the server, with the final score.
void Session::drawEndScreen(sf::RenderWindow &window) const
{
    if (!_gameOver)
        return;
    sf::RectangleShape overlay({WorldWidth, WorldHeight});
    overlay.setFillColor(sf::Color(5, 5, 15, 205));
    window.draw(overlay);
    window.draw(centeredText(_gameWon ? "MISSION ACCOMPLISHED" : "GAME OVER", 64,
                             _gameWon ? sf::Color(0, 230, 255) : sf::Color(255, 50, 50), {WorldWidth / 2.f, 430.f}));
    window.draw(
        centeredText("FINAL SCORE: " + std::to_string(_score), 32, sf::Color(255, 204, 0), {WorldWidth / 2.f, 520.f}));
    window.draw(centeredText("Press [ESC] to Return to Menu", 22, sf::Color(170, 185, 215), {WorldWidth / 2.f, 600.f}));
}

} // namespace rtype::client
