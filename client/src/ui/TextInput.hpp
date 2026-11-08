#pragma once

#include <SFML/Graphics.hpp>
#include <string>

namespace rtype::client::ui {

class TextInput {
public:
    TextInput(const sf::Font &font, const std::string &labelText, const std::string &defaultValue,
              sf::Vector2f position, sf::Vector2f size = {340.f, 50.f}, std::size_t maxChars = 24);

    void setFocused(bool focused);
    bool isFocused() const;

    bool handleMouseClick(sf::Vector2f mousePos);
    void handleEvent(const sf::Event &event);
    void update(float dt);
    void draw(sf::RenderWindow &window) const;

    const std::string &getValue() const;
    void setValue(const std::string &value);

private:
    void updateVisuals();

    sf::Text _label;
    sf::RectangleShape _box;
    sf::Text _text;
    std::string _value;
    std::size_t _maxChars;
    bool _focused{false};
    float _cursorTimer{0.f};
    bool _showCursor{true};
};

} // namespace rtype::client::ui
