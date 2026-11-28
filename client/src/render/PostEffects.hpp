#pragma once

#include <SFML/Graphics.hpp>

#include <optional>
#include <string>

namespace rtype::client {

// The look of the whole picture, chosen in the settings. Classic draws straight to the window; the others draw
// the frame into a texture first, then light it with shaders:
// - Neon: bright pixels (shots, engines, explosions) glow, the edges of the screen darken.
// - Arcade: Neon plus the scanlines and the slightly curved glass of an arcade cabinet.
enum class VisualStyle { Classic, Neon, Arcade };

VisualStyle visualStyleFromName(const std::string &name);
std::string visualStyleName(VisualStyle style);

class PostEffects {
public:
    // Falls back to Classic when the graphics card has no shaders.
    void setStyle(VisualStyle style);
    VisualStyle style() const;

    // Where to draw this frame: the window itself in Classic, an offscreen texture otherwise.
    sf::RenderTarget &begin(sf::RenderWindow &window);
    // Shots, engines and explosions are drawn here too, so that only they glow (nullptr in Classic).
    sf::RenderTarget *glow();
    // Puts the frame on the window with the effects of the style, then the caller displays the window.
    void end(sf::RenderWindow &window, float time);

private:
    bool prepare(sf::Vector2u size);

    VisualStyle _style = VisualStyle::Classic;
    bool _failed = false;
    sf::Vector2u _size{0, 0};
    std::optional<sf::RenderTexture> _scene;
    std::optional<sf::RenderTexture> _glowLayer;
    std::optional<sf::RenderTexture> _bright;
    std::optional<sf::RenderTexture> _blurA;
    std::optional<sf::RenderTexture> _blurB;
    std::optional<sf::Shader> _extract;
    std::optional<sf::Shader> _blur;
    std::optional<sf::Shader> _composite;
};

} // namespace rtype::client
