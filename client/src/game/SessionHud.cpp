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
    return text;
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
    drawBossAlert(window);
    drawPlayers(window);
    drawDamageFlash(window);
    drawWaveBanner(window);
    drawEndScreen(window);
}

// Score and wave, top left.
void Session::drawScore(sf::RenderWindow &window) const
{
    sf::Text score(_resources.getFont("default"), "SCORE: " + std::to_string(_score), 28);
    score.setFillColor(sf::Color(0, 230, 255));
    score.setPosition({45.f, 25.f});
    window.draw(score);

    sf::Text stage(_resources.getFont("default"), "STAGE 1 - WAVE " + std::to_string(_wave), 20);
    stage.setFillColor(sf::Color(255, 204, 0));
    stage.setPosition({45.f, 62.f});
    window.draw(stage);
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
    flash.setFillColor(sf::Color(255, 0, 0, alphaOf(_damageFlash / DamageFlashDuration, 135.f)));
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
