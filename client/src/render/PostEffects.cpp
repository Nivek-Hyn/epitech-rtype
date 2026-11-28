#include "render/PostEffects.hpp"

#include <iostream>

namespace rtype::client {

namespace {

// What glows: the glow layer (shots, engines, explosions) and, a little, the brightest pixels of the frame
// (white and yellow titles). Bright backgrounds such as the Mars sky are below the threshold.
constexpr const char *ExtractShader = R"(
uniform sampler2D scene;
uniform sampler2D layer;
uniform float threshold;
void main()
{
    vec2 uv = gl_TexCoord[0].xy;
    vec3 color = texture2D(scene, uv).rgb;
    float light = dot(color, vec3(0.2126, 0.7152, 0.0722));
    vec3 glow = texture2D(layer, uv).rgb + color * smoothstep(threshold, 1.0, light) * 0.6;
    gl_FragColor = vec4(glow, 1.0);
}
)";

// One direction of a 9-tap Gaussian blur; run once horizontally and once vertically.
constexpr const char *BlurShader = R"(
uniform sampler2D source;
uniform vec2 step;
void main()
{
    vec2 uv = gl_TexCoord[0].xy;
    vec3 sum = texture2D(source, uv).rgb * 0.2270270270;
    sum += texture2D(source, uv + step * 1.3846153846).rgb * 0.3162162162;
    sum += texture2D(source, uv - step * 1.3846153846).rgb * 0.3162162162;
    sum += texture2D(source, uv + step * 3.2307692308).rgb * 0.0702702703;
    sum += texture2D(source, uv - step * 3.2307692308).rgb * 0.0702702703;
    gl_FragColor = vec4(sum, 1.0);
}
)";

// Adds the glow to the frame, darkens the edges and, for the arcade look, bends the glass and draws scanlines.
constexpr const char *CompositeShader = R"(
uniform sampler2D scene;
uniform sampler2D glow;
uniform float glowStrength;
uniform float arcade;
uniform float lines;
uniform float time;

vec2 bend(vec2 uv)
{
    vec2 centered = uv * 2.0 - 1.0;
    centered *= 1.0 + 0.035 * dot(centered.yx, centered.yx) * arcade;
    return centered * 0.5 + 0.5;
}

void main()
{
    vec2 uv = bend(gl_TexCoord[0].xy);
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) {
        gl_FragColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }
    vec3 color;
    float split = 0.0011 * arcade;
    color.r = texture2D(scene, uv + vec2(split, 0.0)).r;
    color.g = texture2D(scene, uv).g;
    color.b = texture2D(scene, uv - vec2(split, 0.0)).b;
    color += texture2D(glow, uv).rgb * glowStrength;

    float scan = 0.5 + 0.5 * cos(uv.y * lines * 6.2831853);
    color *= mix(1.0, 0.78 + 0.22 * scan, arcade);
    color *= 1.0 + 0.012 * arcade * sin(time * 50.0);

    vec2 fromCenter = uv - 0.5;
    float vignette = 1.0 - dot(fromCenter, fromCenter) * mix(0.55, 0.9, arcade);
    gl_FragColor = vec4(color * vignette, 1.0);
}
)";

std::optional<sf::Shader> makeShader(const char *source)
{
    std::optional<sf::Shader> shader(std::in_place);
    if (!shader->loadFromMemory(std::string(source), sf::Shader::Type::Fragment))
        return std::nullopt;
    return shader;
}

std::optional<sf::RenderTexture> makeTexture(sf::Vector2u size)
{
    std::optional<sf::RenderTexture> texture(std::in_place);
    if (!texture->resize(size))
        return std::nullopt;
    texture->setSmooth(true);
    return texture;
}

// Draws a whole texture over a whole target through a shader.
void pass(sf::RenderTarget &target, const sf::Texture &source, const sf::Shader &shader)
{
    const sf::Vector2f size(target.getSize());
    target.setView(sf::View(sf::FloatRect({0.f, 0.f}, size)));
    sf::Sprite sprite(source);
    sprite.setScale({size.x / static_cast<float>(source.getSize().x), size.y / static_cast<float>(source.getSize().y)});
    sf::RenderStates states;
    states.shader = &shader;
    states.blendMode = sf::BlendNone;
    target.draw(sprite, states);
}

} // namespace

VisualStyle visualStyleFromName(const std::string &name)
{
    if (name == "classic")
        return VisualStyle::Classic;
    if (name == "arcade")
        return VisualStyle::Arcade;
    return VisualStyle::Neon;
}

std::string visualStyleName(VisualStyle style)
{
    switch (style) {
    case VisualStyle::Classic:
        return "classic";
    case VisualStyle::Arcade:
        return "arcade";
    default:
        return "neon";
    }
}

void PostEffects::setStyle(VisualStyle style)
{
    _style = style;
}

VisualStyle PostEffects::style() const
{
    return _failed ? VisualStyle::Classic : _style;
}

bool PostEffects::prepare(sf::Vector2u size)
{
    if (_failed)
        return false;
    if (size == _size && _scene)
        return true;
    if (!sf::Shader::isAvailable()) {
        std::cerr << "[Graphics] Shaders are not available: classic look\n";
        _failed = true;
        return false;
    }
    if (!_extract) {
        _extract = makeShader(ExtractShader);
        _blur = makeShader(BlurShader);
        _composite = makeShader(CompositeShader);
    }
    const sf::Vector2u half{std::max(1u, size.x / 2), std::max(1u, size.y / 2)};
    const sf::Vector2u quarter{std::max(1u, size.x / 4), std::max(1u, size.y / 4)};
    _scene = makeTexture(size);
    _glowLayer = makeTexture(half);
    _bright = makeTexture(half);
    _blurA = makeTexture(quarter);
    _blurB = makeTexture(quarter);
    if (!_extract || !_blur || !_composite || !_scene || !_glowLayer || !_bright || !_blurA || !_blurB) {
        std::cerr << "[Graphics] Post effects could not start: classic look\n";
        _failed = true;
        return false;
    }
    _size = size;
    return true;
}

sf::RenderTarget &PostEffects::begin(sf::RenderWindow &window)
{
    if (_style == VisualStyle::Classic || !prepare(window.getSize()))
        return window;
    _scene->setView(window.getView());
    _glowLayer->setView(window.getView());
    _glowLayer->clear(sf::Color::Black);
    return *_scene;
}

sf::RenderTarget *PostEffects::glow()
{
    if (_style == VisualStyle::Classic || _failed || !_glowLayer)
        return nullptr;
    return &*_glowLayer;
}

void PostEffects::end(sf::RenderWindow &window, float time)
{
    if (_style == VisualStyle::Classic || _failed || !_scene)
        return;
    _scene->display();
    _glowLayer->display();
    const bool arcade = _style == VisualStyle::Arcade;

    _extract->setUniform("scene", sf::Shader::CurrentTexture);
    _extract->setUniform("layer", _glowLayer->getTexture());
    _extract->setUniform("threshold", 0.78f);
    _bright->clear();
    pass(*_bright, _scene->getTexture(), *_extract);
    _bright->display();

    // Two blur rounds at a quarter of the size: a wide, soft glow for little work.
    const sf::Vector2f texel(1.f / static_cast<float>(_blurA->getSize().x),
                             1.f / static_cast<float>(_blurA->getSize().y));
    _blur->setUniform("source", sf::Shader::CurrentTexture);
    const sf::Texture *source = &_bright->getTexture();
    for (int round = 0; round < 2; ++round) {
        _blur->setUniform("step", sf::Glsl::Vec2(texel.x, 0.f));
        _blurA->clear();
        pass(*_blurA, *source, *_blur);
        _blurA->display();
        _blur->setUniform("step", sf::Glsl::Vec2(0.f, texel.y));
        _blurB->clear();
        pass(*_blurB, _blurA->getTexture(), *_blur);
        _blurB->display();
        source = &_blurB->getTexture();
    }

    _composite->setUniform("scene", sf::Shader::CurrentTexture);
    _composite->setUniform("glow", _blurB->getTexture());
    _composite->setUniform("glowStrength", arcade ? 1.15f : 1.35f);
    _composite->setUniform("arcade", arcade ? 1.f : 0.f);
    _composite->setUniform("lines", static_cast<float>(window.getSize().y) / 3.f);
    _composite->setUniform("time", time);
    const sf::View view = window.getView();
    pass(window, _scene->getTexture(), *_composite);
    window.setView(view);
}

} // namespace rtype::client
