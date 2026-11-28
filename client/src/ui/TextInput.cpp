#include "TextInput.hpp"

namespace rtype::client::ui {

TextInput::TextInput(const sf::Font &font, const std::string &labelText, const std::string &defaultValue,
                     sf::Vector2f position, sf::Vector2f size, std::size_t maxChars)
    : _label(font, labelText, 20), _text(font, defaultValue, 22), _value(defaultValue), _maxChars(maxChars)
{
    _label.setFillColor(sf::Color(150, 158, 172));
    _label.setLetterSpacing(1.6f);
    const sf::FloatRect bounds = _label.getLocalBounds(); // right-aligned against the box, as OptionSelector
    _label.setPosition({position.x + 196.f - bounds.position.x - bounds.size.x, position.y + 13.f});

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
        _box.setFillColor(sf::Color(0, 0, 0, 230));
        _box.setOutlineColor(sf::Color(238, 244, 255));
    } else {
        _box.setFillColor(sf::Color(0, 0, 0, 190));
        _box.setOutlineColor(sf::Color(255, 255, 255, 60));
    }

    std::string displayText = _value;
    if (_focused && _showCursor) {
        displayText += "_";
    }
    _text.setString(displayText);
    _text.setFillColor(sf::Color::White);
    _text.setPosition({_box.getPosition().x + 15.f, _box.getPosition().y + 10.f});
}

void TextInput::draw(sf::RenderTarget &window) const
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

} // namespace rtype::client::ui
