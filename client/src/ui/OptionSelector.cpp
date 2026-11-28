#include "OptionSelector.hpp"

namespace rtype::client::ui {

OptionSelector::OptionSelector(const sf::Font &font, const std::string &labelText, std::vector<std::string> options,
                               sf::Vector2f position, std::size_t defaultIndex)
    : _label(font, labelText, 20), _arrowLeft(font, "<", 26), _valueText(font, "", 22), _arrowRight(font, ">", 26),
      _options(std::move(options)), _currentIndex(defaultIndex)
{
    _label.setFillColor(sf::Color(150, 158, 172));
    _label.setLetterSpacing(1.6f);
    // Right-aligned against the box, so a long label never runs into it.
    const sf::FloatRect bounds = _label.getLocalBounds();
    _label.setPosition({position.x + 196.f - bounds.position.x - bounds.size.x, position.y + 13.f});

    _box.setSize({340.f, 50.f});
    _box.setPosition({position.x + 220.f, position.y});
    _box.setOutlineThickness(2.f);

    _arrowLeft.setFillColor(sf::Color(238, 244, 255));
    _arrowRight.setFillColor(sf::Color(238, 244, 255));

    updateVisuals();
}

void OptionSelector::setSelected(bool selected)
{
    _selected = selected;
    updateVisuals();
}

bool OptionSelector::isSelected() const
{
    return _selected;
}

void OptionSelector::setEnabled(bool enabled)
{
    _enabled = enabled;
    updateVisuals();
}

bool OptionSelector::isEnabled() const
{
    return _enabled;
}

void OptionSelector::next()
{
    if (!_enabled || _options.empty())
        return;
    _currentIndex = (_currentIndex + 1) % _options.size();
    updateVisuals();
}

void OptionSelector::previous()
{
    if (!_enabled || _options.empty())
        return;
    _currentIndex = (_currentIndex == 0) ? _options.size() - 1 : _currentIndex - 1;
    updateVisuals();
}

bool OptionSelector::handleMouseClick(sf::Vector2f mousePos)
{
    if (!_enabled)
        return false;

    if (_arrowLeft.getGlobalBounds().contains(mousePos)) {
        previous();
        return true;
    }
    if (_arrowRight.getGlobalBounds().contains(mousePos)) {
        next();
        return true;
    }
    if (_box.getGlobalBounds().contains(mousePos)) {
        const float midX = _box.getPosition().x + (_box.getSize().x / 2.f);
        if (mousePos.x < midX) {
            previous();
        } else {
            next();
        }
        return true;
    }
    if (_label.getGlobalBounds().contains(mousePos)) {
        next();
        return true;
    }
    return false;
}

void OptionSelector::handleMouseMove(sf::Vector2f mousePos)
{
    if (_enabled) {
        setSelected(_box.getGlobalBounds().contains(mousePos) || _label.getGlobalBounds().contains(mousePos));
    }
}

void OptionSelector::handleKeyPress(sf::Keyboard::Key key)
{
    if (!_enabled || !_selected)
        return;

    if (key == sf::Keyboard::Key::Left || key == sf::Keyboard::Key::Q) {
        previous();
    } else if (key == sf::Keyboard::Key::Right || key == sf::Keyboard::Key::D) {
        next();
    }
}

void OptionSelector::updateVisuals()
{
    if (!_enabled) {
        _box.setFillColor(sf::Color(0, 0, 0, 120));
        _box.setOutlineColor(sf::Color(255, 255, 255, 25));
        _label.setFillColor(sf::Color(255, 255, 255, 70));
        _arrowLeft.setFillColor(sf::Color(255, 255, 255, 50));
        _arrowRight.setFillColor(sf::Color(255, 255, 255, 50));
        _valueText.setFillColor(sf::Color(255, 255, 255, 70));
    } else if (_selected) {
        _box.setFillColor(sf::Color(0, 0, 0, 230));
        _box.setOutlineColor(sf::Color(238, 244, 255));
        _label.setFillColor(sf::Color::White);
        _arrowLeft.setFillColor(sf::Color::White);
        _arrowRight.setFillColor(sf::Color::White);
        _valueText.setFillColor(sf::Color::White);
    } else {
        _box.setFillColor(sf::Color(0, 0, 0, 190));
        _box.setOutlineColor(sf::Color(255, 255, 255, 60));
        _label.setFillColor(sf::Color(150, 158, 172));
        _arrowLeft.setFillColor(sf::Color(150, 158, 172));
        _arrowRight.setFillColor(sf::Color(150, 158, 172));
        _valueText.setFillColor(sf::Color(238, 244, 255));
    }

    if (_currentIndex < _options.size()) {
        _valueText.setString(_options[_currentIndex]);
    } else {
        _valueText.setString("---");
    }

    const auto boxBounds = _box.getGlobalBounds();
    const auto valBounds = _valueText.getLocalBounds();

    _arrowLeft.setPosition({boxBounds.position.x + 15.f, boxBounds.position.y + 7.f});
    _arrowRight.setPosition({boxBounds.position.x + boxBounds.size.x - 30.f, boxBounds.position.y + 7.f});

    const float valX = boxBounds.position.x + (boxBounds.size.x - valBounds.size.x) / 2.f - valBounds.position.x;
    const float valY = boxBounds.position.y + (boxBounds.size.y - valBounds.size.y) / 2.f - valBounds.position.y;
    _valueText.setPosition({valX, valY});
}

void OptionSelector::draw(sf::RenderTarget &window) const
{
    window.draw(_label);
    window.draw(_box);
    window.draw(_arrowLeft);
    window.draw(_valueText);
    window.draw(_arrowRight);
}

std::size_t OptionSelector::getSelectedIndex() const
{
    return _currentIndex;
}

const std::string &OptionSelector::getSelectedValue() const
{
    static const std::string empty;
    if (_currentIndex < _options.size())
        return _options[_currentIndex];
    return empty;
}

void OptionSelector::setSelectedIndex(std::size_t index)
{
    if (index < _options.size()) {
        _currentIndex = index;
        updateVisuals();
    }
}

} // namespace rtype::client::ui
