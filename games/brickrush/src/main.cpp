#include "brickrush/Components.hpp"
#include "brickrush/Game.hpp"

#include "engine/core/FixedTimestep.hpp"

#include <SFML/Graphics.hpp>

#include <iostream>
#include <string>

namespace {

const std::string Assets = "games/brickrush/assets/";

void drawEntity(sf::RenderWindow &window, engine::ecs::Registry &registry, engine::ecs::Entity entity,
                const brickrush::Position &position, const brickrush::Box &box)
{
    const brickrush::Color *color = registry.get<brickrush::Color>(entity);
    const sf::Color fill = color ? sf::Color(color->r, color->g, color->b) : sf::Color::White;
    if (registry.has<brickrush::Ball>(entity)) {
        sf::CircleShape ball(box.width / 2.f);
        ball.setPosition({position.x, position.y});
        ball.setFillColor(fill);
        window.draw(ball);
        return;
    }
    sf::RectangleShape shape({box.width, box.height});
    shape.setPosition({position.x, position.y});
    shape.setFillColor(fill);
    // A brick that needs several hits has a white outline, so it is not told apart by color only.
    if (const brickrush::Brick *brick = registry.get<brickrush::Brick>(entity); brick && brick->hits > 1) {
        shape.setOutlineColor(sf::Color::White);
        shape.setOutlineThickness(-2.f);
    }
    window.draw(shape);
}

void draw(sf::RenderWindow &window, brickrush::Game &game)
{
    window.clear(sf::Color(18, 20, 32));
    engine::ecs::Registry &registry = game.registry();
    registry.each<brickrush::Position, brickrush::Box>(
        [&](engine::ecs::Entity entity, brickrush::Position &position, brickrush::Box &box) {
            drawEntity(window, registry, entity, position, box);
        });
    if (game.over() || game.won()) {
        sf::RectangleShape veil({brickrush::Game::Width, brickrush::Game::Height});
        veil.setFillColor(sf::Color(0, 0, 0, 150));
        window.draw(veil);
    }
    window.display();
}

// The score is shown in the title bar, so the demo needs no font file.
std::string title(const brickrush::Game &game)
{
    std::string text = "Brick Rush - level " + std::to_string(game.level() + 1) + " - score " +
                       std::to_string(game.score()) + " - lives " + std::to_string(game.lives());
    if (game.won())
        text += " - YOU WIN: press R to play again";
    else if (game.over())
        text += " - GAME OVER: press R to restart";
    return text;
}

float paddleDirection(const sf::RenderWindow &window)
{
    using sf::Keyboard::isKeyPressed;
    using sf::Keyboard::Key;
    if (!window.hasFocus())
        return 0.f;
    const bool left = isKeyPressed(Key::Left) || isKeyPressed(Key::Q) || isKeyPressed(Key::A);
    const bool right = isKeyPressed(Key::Right) || isKeyPressed(Key::D);
    return static_cast<float>(right) - static_cast<float>(left);
}

void handleEvents(sf::RenderWindow &window, brickrush::Game &game)
{
    while (const std::optional event = window.pollEvent()) {
        if (event->is<sf::Event::Closed>())
            window.close();
        else if (const auto *key = event->getIf<sf::Event::KeyPressed>()) {
            if (key->code == sf::Keyboard::Key::Escape)
                window.close();
            else if (key->code == sf::Keyboard::Key::Space)
                game.launch();
            else if (key->code == sf::Keyboard::Key::R && (game.over() || game.won()))
                game.restart();
        }
    }
}

void run(brickrush::Game &game)
{
    sf::RenderWindow window(sf::VideoMode({1000u, 750u}), "Brick Rush");
    window.setVerticalSyncEnabled(true);
    engine::core::FixedTimestep clock(60.0);
    std::string shownTitle;

    while (window.isOpen()) {
        handleEvents(window, game);
        game.movePaddle(paddleDirection(window));
        // Same fixed-step loop as the R-Type server: the simulation speed does not depend on the frame rate.
        for (int ticks = clock.advance(); ticks > 0; ticks--)
            game.step(clock.step());
        if (const std::string current = title(game); current != shownTitle) {
            window.setTitle(current);
            shownTitle = current;
        }
        draw(window, game);
    }
}

} // namespace

// Run from the repository root: the bricks and levels are read from games/brickrush/assets/.
int main()
{
    try {
        brickrush::Game game(Assets + "bricks.json", {Assets + "level1.json", Assets + "level2.json"});
        run(game);
    } catch (const std::exception &error) {
        std::cerr << "brickrush: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
