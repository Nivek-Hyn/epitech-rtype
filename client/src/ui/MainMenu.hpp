#pragma once

#include "Button.hpp"
#include "GameConfig.hpp"
#include "OptionSelector.hpp"
#include "Starfield.hpp"
#include "TextInput.hpp"

#include <SFML/Graphics.hpp>
#include <memory>
#include <string>
#include <vector>

namespace rtype::client {
class ResourceManager;
}

namespace rtype::client::ui {

enum class MenuScreen { Root, HostGame, JoinGame, Settings };

enum class MenuAction { None, Connect, Exit };

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

private:
    void initRootScreen();
    void initHostScreen();
    void initJoinScreen();
    void initSettingsScreen();
    void updateKeyButtonLabels();

    void navigateMenu(int direction);
    void triggerCurrentSelection();
    void playUiSound(const std::string &id = "click");

    const sf::Font &_font;
    ResourceManager *_resources{nullptr};
    Starfield _starfield;
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
    sf::Text _controlsTitleText;
    std::vector<std::unique_ptr<Button>> _keyButtons;
    std::vector<std::unique_ptr<Button>> _presetButtons;
    std::vector<std::unique_ptr<Button>> _settingsButtons;
    int _settingsSelectedIndex{0};
    int _rebindingAction{-1};
};

} // namespace rtype::client::ui
