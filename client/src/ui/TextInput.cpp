#include "TextInput.hpp"

namespace rtype::client::ui {

TextInput::TextInput(const sf::Font &font, const std::string &labelText, const std::string &defaultValue,
                     sf::Vector2f position, sf::Vector2f size, std::size_t maxChars)
    : _label(font, labelText, 24), _text(font, defaultValue, 22),
      _value(defaultValue), _maxChars(maxChars)
{
    _label.setPosition({position.x, position.y + 10.f});
    _label.setFillColor(sf::Color(190, 195, 215));

    _box.setSize(size);
    _box.setPosition({position.x + 220.f, position.y});
    _box.setOutlineThickness(2.f);

    updateVisuals();
}

void TextInput::setFocused(bool focused)
{
    _focused = focused;
    _cursorTimer = 0.f;
    _showCursor = true;
    updateVisuals();
}

bool TextInput::isFocused() const
{
    return _focused;
}

bool TextInput::handleMouseClick(sf::Vector2f mousePos)
{
    const bool inside = _box.getGlobalBounds().contains(mousePos);
    setFocused(inside);
    return inside;
}

void TextInput::handleEvent(const sf::Event &event)
{
    if (!_focused)
        return;

    if (const auto *textEvent = event.getIf<sf::Event::TextEntered>()) {
        const auto unicode = textEvent->unicode;
        if (unicode == 8) {
            if (!_value.empty()) {
                _value.pop_back();
                updateVisuals();
            }
        } else if (unicode >= 32 && unicode < 127 && _value.size() < _maxChars) {
            _value.push_back(static_cast<char>(unicode));
            updateVisuals();
        }
    }
}

void TextInput::update(float dt)
{
    if (_focused) {
        _cursorTimer += dt;
        if (_cursorTimer >= 0.5f) {
            _cursorTimer = 0.f;
            _showCursor = !_showCursor;
            updateVisuals();
        }
    }
}

void TextInput::updateVisuals()
{
    if (_focused) {
        _box.setFillColor(sf::Color(25, 30, 50));
        _box.setOutlineColor(sf::Color(0, 230, 255));
    } else {
        _box.setFillColor(sf::Color(15, 18, 30));
        _box.setOutlineColor(sf::Color(70, 80, 120));
    }

    std::string displayText = _value;
    if (_focused && _showCursor) {
        displayText += "_";
    }
    _text.setString(displayText);
    _text.setFillColor(sf::Color::White);
    _text.setPosition({_box.getPosition().x + 15.f, _box.getPosition().y + 10.f});
}

void TextInput::draw(sf::RenderWindow &window) const
{
    window.draw(_label);
    window.draw(_box);
    window.draw(_text);
}

const std::string &TextInput::getValue() const
{
    return _value;
}

void TextInput::setValue(const std::string &value)
{
    _value = value;
    updateVisuals();
}

}
