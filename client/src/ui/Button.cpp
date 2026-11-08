#include "Button.hpp"

namespace rtype::client::ui {

Button::Button(const sf::Font &font, const std::string &label, sf::Vector2f position, sf::Vector2f size)
    : _text(font, label, 24), _label(label)
{
    _box.setSize(size);
    _box.setPosition(position);
    _box.setOutlineThickness(2.f);

    updateVisuals();
}

void Button::setPosition(sf::Vector2f position)
{
    _box.setPosition(position);
    updateVisuals();
}

void Button::setSelected(bool selected)
{
    if (_selected != selected) {
        _selected = selected;
        updateVisuals();
    }
}

bool Button::isSelected() const
{
    return _selected;
}

bool Button::contains(sf::Vector2f point) const
{
    return _box.getGlobalBounds().contains(point);
}

void Button::handleMouseMove(sf::Vector2f mousePos)
{
    setSelected(contains(mousePos));
}

bool Button::handleMouseClick(sf::Vector2f mousePos)
{
    return contains(mousePos);
}

void Button::updateVisuals()
{
    if (_selected) {
        _box.setFillColor(sf::Color(40, 50, 85, 230));
        _box.setOutlineColor(sf::Color(0, 230, 255));
        _text.setFillColor(sf::Color(255, 255, 255));
    } else {
        _box.setFillColor(sf::Color(20, 22, 35, 180));
        _box.setOutlineColor(sf::Color(80, 90, 130));
        _text.setFillColor(sf::Color(190, 195, 215));
    }

    const auto textBounds = _text.getLocalBounds();
    const auto boxBounds = _box.getGlobalBounds();

    const float textX = boxBounds.position.x + (boxBounds.size.x - textBounds.size.x) / 2.f - textBounds.position.x;
    const float textY = boxBounds.position.y + (boxBounds.size.y - textBounds.size.y) / 2.f - textBounds.position.y;
    _text.setPosition({textX, textY});
}

void Button::draw(sf::RenderWindow &window) const
{
    window.draw(_box);
    window.draw(_text);
}

const std::string &Button::getLabel() const
{
    return _label;
}

void Button::setLabel(const std::string &label)
{
    _label = label;
    _text.setString(label);
    updateVisuals();
}

} // namespace rtype::client::ui
