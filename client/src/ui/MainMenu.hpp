#pragma once

#include "Button.hpp"
#include "GameConfig.hpp"
#include "OptionSelector.hpp"
#include "Starfield.hpp"
#include "TextInput.hpp"

#include <SFML/Graphics.hpp>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace rtype::client {
class ResourceManager;
}

namespace rtype::client::ui {

enum class MenuScreen { Root, HostGame, JoinGame, Settings };

// What the menu asks the application to do.
enum class MenuAction { None, Campaign, Infinite, Host, Connect, Editor, Exit };

class MainMenu {
public:
    explicit MainMenu(const sf::Font &font, ResourceManager *resources = nullptr);
    void setResourceManager(ResourceManager *resources);

    void handleEvent(const sf::Event &event, const sf::RenderWindow &window);
    void update(float dt);
    void draw(sf::RenderWindow &window);

    MenuAction getAndResetAction();
    const GameConfig &getConfig() const;
    void setStatusMessage(const std::string &message, sf::Color color = sf::Color::Yellow);
    // Changes the accessibility settings from the game (F1, F2) and keeps the settings screen in sync.
    void setAccessibility(bool autoFire, bool highContrast);

private:
    void initRootScreen();
    void initHostScreen();
    void initJoinScreen();
    void initSettingsScreen();
    void updateKeyButtonLabels();

    void handleRootEvent(std::optional<sf::Vector2f> mouse, bool clicked, const sf::Event::KeyPressed *key);
    void handleHostEvent(const sf::Event &event, std::optional<sf::Vector2f> mouse, bool clicked,
                         const sf::Event::KeyPressed *key);
    void handleJoinEvent(const sf::Event &event, std::optional<sf::Vector2f> mouse, bool clicked,
                         const sf::Event::KeyPressed *key);
    void handleSettingsEvent(std::optional<sf::Vector2f> mouse, bool clicked, const sf::Event::KeyPressed *key);
    bool clickSettingSelectors(sf::Vector2f mouse);
    bool startRebinding(sf::Vector2f mouse);
    bool clickPreset(sf::Vector2f mouse);
    void rebind(sf::Keyboard::Key key);
    void confirmHost();
    void confirmJoin();
    void back();
    void drawScreenTitle(sf::RenderWindow &window, const std::string &title, float y);
    void drawSettings(sf::RenderWindow &window);

    void navigateMenu(int direction);
    void triggerCurrentSelection();
    void playUiSound(const std::string &id = "click");

    const sf::Font &_font;
    ResourceManager *_resources{nullptr};
    MenuScreen _currentScreen{MenuScreen::Root};
    MenuAction _pendingAction{MenuAction::None};
    GameConfig _config;

    sf::Text _titleText;
    sf::Text _subtitleText;
    sf::Text _screenTitleText;
    sf::Text _statusText;
    float _statusTimer{0.f};

    std::vector<std::unique_ptr<Button>> _rootButtons;
    int _rootSelectedIndex{0};

    std::unique_ptr<OptionSelector> _hostModeSelector;
    std::unique_ptr<OptionSelector> _hostDifficultySelector;
    std::unique_ptr<OptionSelector> _hostMissionSelector;
    std::unique_ptr<TextInput> _hostPortInput;
    std::vector<std::unique_ptr<Button>> _hostButtons;
    int _hostSelectedIndex{0};

    std::unique_ptr<TextInput> _joinIpInput;
    std::unique_ptr<TextInput> _joinPortInput;
    std::vector<std::unique_ptr<Button>> _joinButtons;
    int _joinSelectedIndex{0};

    std::unique_ptr<OptionSelector> _sfxVolumeSelector;
    std::unique_ptr<OptionSelector> _musicVolumeSelector;
    std::unique_ptr<OptionSelector> _autoFireSelector;
    std::unique_ptr<OptionSelector> _contrastSelector;
    sf::Text _controlsTitleText;
    std::vector<std::unique_ptr<Button>> _keyButtons;
    std::vector<std::unique_ptr<Button>> _presetButtons;
    std::vector<std::unique_ptr<Button>> _settingsButtons;
    int _settingsSelectedIndex{0};
    int _rebindingAction{-1};
};

} // namespace rtype::client::ui
