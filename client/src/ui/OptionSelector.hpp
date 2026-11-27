#pragma once

#include <SFML/Graphics.hpp>
#include <string>
#include <vector>

namespace rtype::client::ui {

class OptionSelector {
public:
    OptionSelector(const sf::Font &font, const std::string &labelText, std::vector<std::string> options,
                   sf::Vector2f position, std::size_t defaultIndex = 0);

    void setSelected(bool selected);
    bool isSelected() const;

    void next();
    void previous();

    bool handleMouseClick(sf::Vector2f mousePos);
    void handleMouseMove(sf::Vector2f mousePos);
    void handleKeyPress(sf::Keyboard::Key key);

    void draw(sf::RenderTarget &window) const;

    std::size_t getSelectedIndex() const;
    const std::string &getSelectedValue() const;
    void setSelectedIndex(std::size_t index);

    void setEnabled(bool enabled);
    bool isEnabled() const;

private:
    void updateVisuals();

    sf::Text _label;
    sf::RectangleShape _box;
    sf::Text _arrowLeft;
    sf::Text _valueText;
    sf::Text _arrowRight;

    std::vector<std::string> _options;
    std::size_t _currentIndex{0};
    bool _selected{false};
    bool _enabled{true};
};

} // namespace rtype::client::ui
