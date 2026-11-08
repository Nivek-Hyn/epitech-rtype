#pragma once

#include <SFML/Graphics.hpp>
#include <string>

namespace rtype::client::ui {

class Button {
public:
    Button(const sf::Font &font, const std::string &label, sf::Vector2f position, sf::Vector2f size = {320.f, 55.f});

    void setPosition(sf::Vector2f position);
    void setSelected(bool selected);
    bool isSelected() const;

    bool contains(sf::Vector2f point) const;
    void handleMouseMove(sf::Vector2f mousePos);
    bool handleMouseClick(sf::Vector2f mousePos);

    void draw(sf::RenderWindow &window) const;

    const std::string &getLabel() const;
    void setLabel(const std::string &label);

private:
    void updateVisuals();

    sf::RectangleShape _box;
    sf::Text _text;
    std::string _label;
    bool _selected{false};
};

} // namespace rtype::client::ui
