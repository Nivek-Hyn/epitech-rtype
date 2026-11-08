#pragma once

#include <SFML/Graphics.hpp>
#include <vector>

namespace rtype::client::ui {

class Starfield {
public:
    explicit Starfield(std::size_t starCount = 150, float width = 1920.f, float height = 1080.f);

    void update(float dt);
    void draw(sf::RenderWindow &window) const;

private:
    struct Star {
        float x;
        float y;
        float speed;
        float size;
        sf::Color color;
    };

    float _width;
    float _height;
    std::vector<Star> _stars;
};

} // namespace rtype::client::ui
