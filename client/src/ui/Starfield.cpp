#include "Starfield.hpp"
#include <random>

namespace rtype::client::ui {

Starfield::Starfield(std::size_t starCount, float width, float height) : _width(width), _height(height)
{
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> distX(0.f, _width);
    std::uniform_real_distribution<float> distY(0.f, _height);
    std::uniform_int_distribution<int> distLayer(0, 2);

    _stars.reserve(starCount);
    for (std::size_t i = 0; i < starCount; ++i) {
        int layer = distLayer(rng);
        float speed = 0.f;
        float size = 0.f;
        sf::Color color;

        if (layer == 0) {
            speed = 30.f;
            size = 1.5f;
            color = sf::Color(100, 110, 150, 180);
        } else if (layer == 1) {
            speed = 70.f;
            size = 2.5f;
            color = sf::Color(180, 190, 230, 220);
        } else {
            speed = 130.f;
            size = 3.5f;
            color = (i % 2 == 0) ? sf::Color(255, 235, 150) : sf::Color(140, 220, 255);
        }

        _stars.push_back({distX(rng), distY(rng), speed, size, color});
    }
}

void Starfield::update(float dt)
{
    for (auto &star : _stars) {
        star.x -= star.speed * dt;
        if (star.x < 0.f) {
            star.x = _width;
        }
    }
}

void Starfield::draw(sf::RenderWindow &window) const
{
    for (const auto &star : _stars) {
        sf::RectangleShape shape({star.size, star.size});
        shape.setPosition({star.x, star.y});
        shape.setFillColor(star.color);
        window.draw(shape);
    }
}

} // namespace rtype::client::ui
