#include "MainMenu.hpp"
#include "ResourceManager.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <optional>

namespace rtype::client::ui {

MainMenu::MainMenu(const sf::Font &font, ResourceManager *resources)
    : _font(font), _resources(resources), _config(GameConfig::loadFromFile("config.json")),
      _titleText(font, "R - T Y P E", 80), _subtitleText(font, "A SPACE SHOOTER", 22), _screenTitleText(font, "", 36),
      _statusText(font, "", 22), _controlsTitleText(font, "--- CONTROLS RECONFIGURATION ---", 22)
{
    _titleText.setFillColor(sf::Color(255, 204, 0));
    _titleText.setLetterSpacing(1.5f);
    const auto titleBounds = _titleText.getLocalBounds();
    _titleText.setPosition({960.f - (titleBounds.size.x / 2.f), 100.f});

    _subtitleText.setFillColor(sf::Color(0, 220, 255));
    _subtitleText.setLetterSpacing(2.0f);
    const auto subBounds = _subtitleText.getLocalBounds();
    _subtitleText.setPosition({960.f - (subBounds.size.x / 2.f), 195.f});

    _screenTitleText.setFillColor(sf::Color(255, 255, 255));
    _statusText.setFillColor(sf::Color::Yellow);

    initRootScreen();
    initHostScreen();
    initJoinScreen();
    initSettingsScreen();
}

void MainMenu::initRootScreen()
{
    const std::vector<std::string> labels = {"CAMPAIGN",       "INFINITE MODE", "HOST A GAME", "JOIN A GAME",
                                             "CREATE A LEVEL", "SETTINGS",      "QUIT"};
    const float startY = 290.f;
    const float btnWidth = 380.f;
    const float btnHeight = 55.f;
    const float posX = 960.f - (btnWidth / 2.f);

    for (std::size_t i = 0; i < labels.size(); ++i) {
        auto btn =
            std::make_unique<Button>(_font, labels[i], sf::Vector2f(posX, startY + (static_cast<float>(i) * 72.f)),
                                     sf::Vector2f(btnWidth, btnHeight));
        _rootButtons.push_back(std::move(btn));
    }
    _rootButtons[0]->setSelected(true);
}

void MainMenu::initHostScreen()
{
    const float posX = 680.f;
    float startY = 360.f;
    const float spacing = 80.f;

    _hostModeSelector = std::make_unique<OptionSelector>(
        _font, "GAME MODE", std::vector<std::string>{"CAMPAIGN", "INFINITE"}, sf::Vector2f(posX, startY));
    startY += spacing;

    _hostDifficultySelector = std::make_unique<OptionSelector>(
        _font, "DIFFICULTY", std::vector<std::string>{"EASY", "NORMAL", "HARD"}, sf::Vector2f(posX, startY), 1);
    startY += spacing;

    _hostMissionSelector = std::make_unique<OptionSelector>(
        _font, "START AT",
        std::vector<std::string>{"WORLD 1 : ORBITAL BELT", "WORLD 2 : MARS", "WORLD 3 : SINGULARITY"},
        sf::Vector2f(posX, startY));
    startY += spacing;

    _hostPortInput = std::make_unique<TextInput>(_font, "SERVER PORT", _config.hostPort, sf::Vector2f(posX, startY),
                                                 sf::Vector2f(340.f, 50.f), 6);
    startY += 110.f;

    const float btnWidth = 260.f;
    auto startBtn =
        std::make_unique<Button>(_font, "LAUNCH SERVER", sf::Vector2f(680.f, startY), sf::Vector2f(btnWidth, 55.f));
    auto backBtn = std::make_unique<Button>(_font, "BACK", sf::Vector2f(980.f, startY), sf::Vector2f(btnWidth, 55.f));

    _hostButtons.push_back(std::move(startBtn));
    _hostButtons.push_back(std::move(backBtn));
}

void MainMenu::initJoinScreen()
{
    const float posX = 680.f;
    float startY = 400.f;

    _joinIpInput = std::make_unique<TextInput>(_font, "SERVER IP", _config.joinIp, sf::Vector2f(posX, startY),
                                               sf::Vector2f(340.f, 50.f), 20);
    startY += 90.f;

    _joinPortInput = std::make_unique<TextInput>(_font, "SERVER PORT", _config.joinPort, sf::Vector2f(posX, startY),
                                                 sf::Vector2f(340.f, 50.f), 6);
    startY += 120.f;

    const float btnWidth = 260.f;
    auto connectBtn =
        std::make_unique<Button>(_font, "CONNECT", sf::Vector2f(680.f, startY), sf::Vector2f(btnWidth, 55.f));
    auto backBtn = std::make_unique<Button>(_font, "BACK", sf::Vector2f(980.f, startY), sf::Vector2f(btnWidth, 55.f));

    _joinButtons.push_back(std::move(connectBtn));
    _joinButtons.push_back(std::move(backBtn));
}

void MainMenu::initSettingsScreen()
{
    const float posX = 680.f;
    float startY = 250.f;

    const std::size_t sfxIdx =
        std::clamp(static_cast<std::size_t>(std::round(_config.sfxVolume / 25.f)), std::size_t{0}, std::size_t{4});
    const std::size_t musicIdx =
        std::clamp(static_cast<std::size_t>(std::round(_config.musicVolume / 25.f)), std::size_t{0}, std::size_t{4});

    _sfxVolumeSelector = std::make_unique<OptionSelector>(_font, "SFX VOLUME",
                                                          std::vector<std::string>{"MUTE", "25%", "50%", "75%", "100%"},
                                                          sf::Vector2f(posX, startY), sfxIdx);
    startY += 70.f;

    _musicVolumeSelector = std::make_unique<OptionSelector>(
        _font, "MUSIC VOLUME", std::vector<std::string>{"MUTE", "25%", "50%", "75%", "100%"},
        sf::Vector2f(posX, startY), musicIdx);
    startY += 70.f;

    // Accessibility: also toggled in game with F1 (auto fire) and F2 (high contrast).
    _autoFireSelector = std::make_unique<OptionSelector>(_font, "AUTO FIRE (F1)", std::vector<std::string>{"OFF", "ON"},
                                                         sf::Vector2f(posX, startY), _config.autoFire ? 1 : 0);
    startY += 70.f;
    _contrastSelector =
        std::make_unique<OptionSelector>(_font, "HIGH CONTRAST (F2)", std::vector<std::string>{"OFF", "ON"},
                                         sf::Vector2f(posX, startY), _config.highContrast ? 1 : 0);
    startY += 70.f;
    const std::vector<std::string> styles{"classic", "neon", "arcade"};
    const auto style = std::find(styles.begin(), styles.end(), _config.visualStyle);
    _styleSelector = std::make_unique<OptionSelector>(
        _font, "VISUAL STYLE", std::vector<std::string>{"CLASSIC", "NEON", "ARCADE"}, sf::Vector2f(posX, startY),
        style == styles.end() ? 1 : static_cast<std::size_t>(style - styles.begin()));
    startY += 80.f;

    _controlsTitleText.setString("--- CONTROLS RECONFIGURATION ---");
    _controlsTitleText.setCharacterSize(22);
    _controlsTitleText.setFillColor(sf::Color(0, 220, 255));
    const auto b = _controlsTitleText.getLocalBounds();
    _controlsTitleText.setPosition({960.f - (b.size.x / 2.f), startY});

    const float colLeftX = 650.f;
    const float colRightX = 970.f;
    const float btnWidth = 300.f;
    const float btnHeight = 44.f;
    const float keysStartY = startY + 40.f;
    const float rowSpacing = 54.f;

    _keyButtons.clear();
    _presetButtons.clear();
    _settingsButtons.clear();

    // Row 1: UP and DOWN
    _keyButtons.push_back(
        std::make_unique<Button>(_font, "", sf::Vector2f(colLeftX, keysStartY), sf::Vector2f(btnWidth, btnHeight)));
    _keyButtons.push_back(
        std::make_unique<Button>(_font, "", sf::Vector2f(colRightX, keysStartY), sf::Vector2f(btnWidth, btnHeight)));

    // Row 2: LEFT and RIGHT
    _keyButtons.push_back(std::make_unique<Button>(_font, "", sf::Vector2f(colLeftX, keysStartY + rowSpacing),
                                                   sf::Vector2f(btnWidth, btnHeight)));
    _keyButtons.push_back(std::make_unique<Button>(_font, "", sf::Vector2f(colRightX, keysStartY + rowSpacing),
                                                   sf::Vector2f(btnWidth, btnHeight)));

    // Row 3: SHOOT centered
    _keyButtons.push_back(std::make_unique<Button>(_font, "", sf::Vector2f(810.f, keysStartY + rowSpacing * 2.f),
                                                   sf::Vector2f(btnWidth, btnHeight)));

    // Presets row placed clearly lower down
    const float presetsY = keysStartY + rowSpacing * 3.f + 18.f;
    const float presetBtnWidth = 210.f;
    const float presetBtnHeight = 42.f;

    _presetButtons.push_back(std::make_unique<Button>(_font, "PRESET AZERTY", sf::Vector2f(620.f, presetsY),
                                                      sf::Vector2f(presetBtnWidth, presetBtnHeight)));
    _presetButtons.push_back(std::make_unique<Button>(_font, "PRESET QWERTY", sf::Vector2f(855.f, presetsY),
                                                      sf::Vector2f(presetBtnWidth, presetBtnHeight)));
    _presetButtons.push_back(std::make_unique<Button>(_font, "RESET DEFAULTS", sf::Vector2f(1090.f, presetsY),
                                                      sf::Vector2f(presetBtnWidth, presetBtnHeight)));

    // Back button
    auto backBtn =
        std::make_unique<Button>(_font, "BACK", sf::Vector2f(835.f, presetsY + 68.f), sf::Vector2f(250.f, 48.f));
    _settingsButtons.push_back(std::move(backBtn));

    updateKeyButtonLabels();
}

void MainMenu::updateKeyButtonLabels()
{
    if (_keyButtons.size() < 5)
        return;

    const std::vector<std::string> actionNames = {"MOVE UP", "MOVE DOWN", "MOVE LEFT", "MOVE RIGHT", "SHOOT"};
    const std::vector<sf::Keyboard::Key> keys = {_config.keyUp, _config.keyDown, _config.keyLeft, _config.keyRight,
                                                 _config.keyShoot};

    for (std::size_t i = 0; i < 5; ++i) {
        if (_rebindingAction == static_cast<int>(i)) {
            _keyButtons[i]->setLabel(actionNames[i] + ": <PRESS KEY>");
        } else {
            _keyButtons[i]->setLabel(actionNames[i] + ": [" + GameConfig::keyToString(keys[i]) + "]");
        }
    }
}

void MainMenu::setStatusMessage(const std::string &message, sf::Color color)
{
    _statusText.setString(message);
    _statusText.setFillColor(color);
    const auto bounds = _statusText.getLocalBounds();
    _statusText.setPosition({960.f - (bounds.size.x / 2.f), 970.f});
    _statusTimer = 5.0f;
}

void MainMenu::setResourceManager(ResourceManager *resources)
{
    _resources = resources;
}

void MainMenu::playUiSound(const std::string &id)
{
    if (_resources) {
        _resources->playSound(id, 75.f);
    }
}

void MainMenu::update(float dt)
{
    if (_currentScreen == MenuScreen::HostGame) {
        _hostPortInput->update(dt);
        const bool isScenario = (_hostModeSelector->getSelectedIndex() == 0);
        _hostMissionSelector->setEnabled(isScenario);
    } else if (_currentScreen == MenuScreen::JoinGame) {
        _joinIpInput->update(dt);
        _joinPortInput->update(dt);
    }

    if (_statusTimer > 0.f) {
        _statusTimer -= dt;
        if (_statusTimer <= 0.f) {
            _statusText.setString("");
        }
    }
    if (_sfxVolumeSelector)
        _config.sfxVolume = static_cast<float>(_sfxVolumeSelector->getSelectedIndex()) * 25.f;
    if (_musicVolumeSelector)
        _config.musicVolume = static_cast<float>(_musicVolumeSelector->getSelectedIndex()) * 25.f;
}

void MainMenu::unlockLevel(int level)
{
    _config.campaignUnlocked = std::max(_config.campaignUnlocked, level);
    _config.saveToFile("config.json");
}

void MainMenu::setAccessibility(bool autoFire, bool highContrast)
{
    _config.autoFire = autoFire;
    _config.highContrast = highContrast;
    _autoFireSelector->setSelectedIndex(autoFire ? 1 : 0);
    _contrastSelector->setSelectedIndex(highContrast ? 1 : 0);
}

namespace {

// Where the mouse is, in world coordinates, for a mouse event; nothing for other events.
std::optional<sf::Vector2f> mousePosition(const sf::Event &event, const sf::RenderWindow &window)
{
    if (const auto *moved = event.getIf<sf::Event::MouseMoved>())
        return window.mapPixelToCoords(moved->position);
    if (const auto *click = event.getIf<sf::Event::MouseButtonPressed>();
        click && click->button == sf::Mouse::Button::Left)
        return window.mapPixelToCoords(click->position);
    return std::nullopt;
}

const std::vector<std::string> ActionNames = {"Up", "Down", "Left", "Right", "Shoot"};

} // namespace

void MainMenu::handleEvent(const sf::Event &event, const sf::RenderWindow &window)
{
    const std::optional<sf::Vector2f> mouse = mousePosition(event, window);
    const bool clicked = event.is<sf::Event::MouseButtonPressed>() && mouse;
    const auto *key = event.getIf<sf::Event::KeyPressed>();
    if (_currentScreen == MenuScreen::Root)
        handleRootEvent(mouse, clicked, key);
    else if (_currentScreen == MenuScreen::HostGame)
        handleHostEvent(event, mouse, clicked, key);
    else if (_currentScreen == MenuScreen::JoinGame)
        handleJoinEvent(event, mouse, clicked, key);
    else if (key && _rebindingAction != -1)
        rebind(key->code);
    else
        handleSettingsEvent(mouse, clicked, key);
}

void MainMenu::handleRootEvent(std::optional<sf::Vector2f> mouse, bool clicked, const sf::Event::KeyPressed *key)
{
    for (std::size_t i = 0; mouse && i < _rootButtons.size(); ++i) {
        if (!_rootButtons[i]->contains(*mouse))
            continue;
        _rootSelectedIndex = static_cast<int>(i);
        for (std::size_t j = 0; j < _rootButtons.size(); ++j)
            _rootButtons[j]->setSelected(j == i);
        if (clicked)
            triggerCurrentSelection();
    }
    if (!key)
        return;
    if (key->code == sf::Keyboard::Key::Up)
        navigateMenu(-1);
    else if (key->code == sf::Keyboard::Key::Down)
        navigateMenu(1);
    else if (key->code == sf::Keyboard::Key::Enter || key->code == sf::Keyboard::Key::Space)
        triggerCurrentSelection();
}

void MainMenu::handleHostEvent(const sf::Event &event, std::optional<sf::Vector2f> mouse, bool clicked,
                               const sf::Event::KeyPressed *key)
{
    _hostPortInput->handleEvent(event);
    if (mouse && !clicked) {
        _hostModeSelector->handleMouseMove(*mouse);
        _hostDifficultySelector->handleMouseMove(*mouse);
        _hostMissionSelector->handleMouseMove(*mouse);
        for (auto &button : _hostButtons)
            button->handleMouseMove(*mouse);
    } else if (clicked) {
        if (_hostModeSelector->handleMouseClick(*mouse) || _hostDifficultySelector->handleMouseClick(*mouse) ||
            _hostMissionSelector->handleMouseClick(*mouse) || _hostPortInput->handleMouseClick(*mouse))
            playUiSound("click");
        if (_hostButtons[0]->handleMouseClick(*mouse))
            confirmHost();
        else if (_hostButtons[1]->handleMouseClick(*mouse))
            back();
    } else if (key) {
        if (key->code == sf::Keyboard::Key::Escape)
            back();
        else if (key->code == sf::Keyboard::Key::Enter)
            confirmHost();
        else {
            _hostModeSelector->handleKeyPress(key->code);
            _hostDifficultySelector->handleKeyPress(key->code);
            _hostMissionSelector->handleKeyPress(key->code);
        }
    }
}

void MainMenu::confirmHost()
{
    playUiSound("click");
    _config.gameMode = static_cast<int>(_hostModeSelector->getSelectedIndex());
    _config.difficulty = static_cast<int>(_hostDifficultySelector->getSelectedIndex());
    _config.mission = static_cast<int>(_hostMissionSelector->getSelectedIndex());
    _config.hostPort = _hostPortInput->getValue().empty() ? "4242" : _hostPortInput->getValue();
    _pendingAction = MenuAction::Host;
}

void MainMenu::handleJoinEvent(const sf::Event &event, std::optional<sf::Vector2f> mouse, bool clicked,
                               const sf::Event::KeyPressed *key)
{
    _joinIpInput->handleEvent(event);
    _joinPortInput->handleEvent(event);
    if (mouse && !clicked) {
        for (auto &button : _joinButtons)
            button->handleMouseMove(*mouse);
    } else if (clicked) {
        _joinIpInput->handleMouseClick(*mouse);
        _joinPortInput->handleMouseClick(*mouse);
        if (_joinButtons[0]->handleMouseClick(*mouse))
            confirmJoin();
        else if (_joinButtons[1]->handleMouseClick(*mouse))
            back();
    } else if (key) {
        if (key->code == sf::Keyboard::Key::Escape)
            back();
        else if (key->code == sf::Keyboard::Key::Enter)
            confirmJoin();
    }
}

void MainMenu::confirmJoin()
{
    playUiSound("click");
    _config.joinIp = _joinIpInput->getValue().empty() ? "127.0.0.1" : _joinIpInput->getValue();
    _config.joinPort = _joinPortInput->getValue().empty() ? "4242" : _joinPortInput->getValue();
    _pendingAction = MenuAction::Connect;
}

void MainMenu::back()
{
    playUiSound("click");
    _config.saveToFile("config.json");
    _currentScreen = MenuScreen::Root;
}

void MainMenu::handleSettingsEvent(std::optional<sf::Vector2f> mouse, bool clicked, const sf::Event::KeyPressed *key)
{
    if (key && key->code == sf::Keyboard::Key::Escape) {
        back();
        return;
    }
    if (!mouse)
        return;
    if (!clicked) {
        for (auto *selector : {&_sfxVolumeSelector, &_musicVolumeSelector, &_autoFireSelector, &_contrastSelector, &_styleSelector})
            (*selector)->handleMouseMove(*mouse);
        for (auto *buttons : {&_keyButtons, &_presetButtons, &_settingsButtons}) {
            for (auto &button : *buttons)
                button->handleMouseMove(*mouse);
        }
        return;
    }
    if (_rebindingAction != -1) { // waiting for a key: a click elsewhere than on an action cancels
        if (!startRebinding(*mouse)) {
            _rebindingAction = -1;
            updateKeyButtonLabels();
            setStatusMessage("Rebinding cancelled", sf::Color::White);
        }
        return;
    }
    if (clickSettingSelectors(*mouse) || startRebinding(*mouse) || clickPreset(*mouse))
        return;
    if (!_settingsButtons.empty() && _settingsButtons[0]->handleMouseClick(*mouse))
        back();
}

// Volumes, auto fire and high contrast: a click changes the setting and saves it at once.
bool MainMenu::clickSettingSelectors(sf::Vector2f mouse)
{
    bool changed = false;
    if (_sfxVolumeSelector->handleMouseClick(mouse)) {
        _config.sfxVolume = static_cast<float>(_sfxVolumeSelector->getSelectedIndex()) * 25.f;
        changed = true;
    }
    if (_musicVolumeSelector->handleMouseClick(mouse)) {
        _config.musicVolume = static_cast<float>(_musicVolumeSelector->getSelectedIndex()) * 25.f;
        changed = true;
    }
    if (_autoFireSelector->handleMouseClick(mouse)) {
        _config.autoFire = _autoFireSelector->getSelectedIndex() == 1;
        changed = true;
    }
    if (_contrastSelector->handleMouseClick(mouse)) {
        _config.highContrast = _contrastSelector->getSelectedIndex() == 1;
        changed = true;
    }
    if (_styleSelector->handleMouseClick(mouse)) {
        static const std::array<const char *, 3> styles{"classic", "neon", "arcade"};
        _config.visualStyle = styles[std::min<std::size_t>(_styleSelector->getSelectedIndex(), 2)];
        changed = true;
    }
    if (changed) {
        playUiSound("click");
        _config.saveToFile("config.json");
    }
    return changed;
}

// A click on an action button waits for the key to assign to it.
bool MainMenu::startRebinding(sf::Vector2f mouse)
{
    for (std::size_t i = 0; i < _keyButtons.size(); ++i) {
        if (_keyButtons[i]->handleMouseClick(mouse)) {
            playUiSound("click");
            _rebindingAction = static_cast<int>(i);
            updateKeyButtonLabels();
            setStatusMessage("Press a key for this action, or Escape to cancel", sf::Color::Yellow);
            return true;
        }
    }
    return false;
}

bool MainMenu::clickPreset(sf::Vector2f mouse)
{
    struct Preset {
        void (GameConfig::*apply)();
        const char *message;
    };
    static const std::array<Preset, 3> presets = {{
        {&GameConfig::applyAzerty, "AZERTY profile applied (Z, Q, S, D, Space)"},
        {&GameConfig::applyQwerty, "QWERTY profile applied (W, A, S, D, Space)"},
        {&GameConfig::resetToDefaults, "Controls reset to defaults"},
    }};
    for (std::size_t i = 0; i < presets.size() && i < _presetButtons.size(); ++i) {
        if (_presetButtons[i]->handleMouseClick(mouse)) {
            playUiSound("click");
            (_config.*presets[i].apply)();
            _config.saveToFile("config.json");
            updateKeyButtonLabels();
            setStatusMessage(presets[i].message, sf::Color::Green);
            return true;
        }
    }
    return false;
}

// The pressed key goes to the action waiting for one, unless another action already uses it.
void MainMenu::rebind(sf::Keyboard::Key key)
{
    const std::string action = ActionNames.at(static_cast<std::size_t>(_rebindingAction));
    if (key == sf::Keyboard::Key::Escape) {
        playUiSound("click");
        setStatusMessage("Rebinding cancelled", sf::Color::White);
    } else if (const std::string conflict = _config.getConflictAction(key, action); !conflict.empty()) {
        playUiSound("hit");
        setStatusMessage("[" + GameConfig::keyToString(key) + "] is already used for " + conflict,
                         sf::Color(255, 80, 80));
        return;
    } else if (key == sf::Keyboard::Key::Unknown) {
        playUiSound("hit");
        setStatusMessage("Unsupported key", sf::Color(255, 80, 80));
        return;
    } else {
        std::array<sf::Keyboard::Key *, 5> keys = {&_config.keyUp, &_config.keyDown, &_config.keyLeft,
                                                   &_config.keyRight, &_config.keyShoot};
        *keys.at(static_cast<std::size_t>(_rebindingAction)) = key;
        playUiSound("click");
        _config.saveToFile("config.json");
        setStatusMessage("Action " + action + " bound to [" + GameConfig::keyToString(key) + "]", sf::Color::Green);
    }
    _rebindingAction = -1;
    updateKeyButtonLabels();
}

void MainMenu::navigateMenu(int direction)
{
    const int count = static_cast<int>(_rootButtons.size());
    _rootSelectedIndex = ((_rootSelectedIndex + direction) % count + count) % count;
    for (std::size_t i = 0; i < _rootButtons.size(); ++i)
        _rootButtons[i]->setSelected(static_cast<int>(i) == _rootSelectedIndex);
}

void MainMenu::triggerCurrentSelection()
{
    playUiSound("click");
    static const std::array<MenuAction, 7> actions = {MenuAction::Campaign, MenuAction::Infinite, MenuAction::None,
                                                      MenuAction::None,     MenuAction::Editor,   MenuAction::None,
                                                      MenuAction::Exit};
    static const std::array<MenuScreen, 7> screens = {MenuScreen::Root,     MenuScreen::Root, MenuScreen::HostGame,
                                                      MenuScreen::JoinGame, MenuScreen::Root, MenuScreen::Settings,
                                                      MenuScreen::Root};
    const auto index = static_cast<std::size_t>(_rootSelectedIndex);
    _pendingAction = actions.at(index);
    _currentScreen = screens.at(index);
}

void MainMenu::drawScreenTitle(sf::RenderTarget &window, const std::string &title, float y)
{
    _screenTitleText.setString(title);
    const auto bounds = _screenTitleText.getLocalBounds();
    _screenTitleText.setPosition({960.f - (bounds.size.x / 2.f), y});
    window.draw(_screenTitleText);
}

void MainMenu::draw(sf::RenderTarget &window)
{
    if (_currentScreen != MenuScreen::Settings) { // the settings need the whole height
        window.draw(_titleText);
        window.draw(_subtitleText);
    }
    if (_currentScreen == MenuScreen::Root) {
        for (const auto &button : _rootButtons)
            button->draw(window);
    } else if (_currentScreen == MenuScreen::HostGame) {
        drawScreenTitle(window, "HOST A GAME", 290.f);
        _hostModeSelector->draw(window);
        _hostDifficultySelector->draw(window);
        _hostMissionSelector->draw(window);
        _hostPortInput->draw(window);
        for (const auto &button : _hostButtons)
            button->draw(window);
    } else if (_currentScreen == MenuScreen::JoinGame) {
        drawScreenTitle(window, "JOIN A GAME", 310.f);
        _joinIpInput->draw(window);
        _joinPortInput->draw(window);
        for (const auto &button : _joinButtons)
            button->draw(window);
    } else {
        drawSettings(window);
    }
    if (!_statusText.getString().isEmpty())
        window.draw(_statusText);
}

void MainMenu::drawSettings(sf::RenderTarget &window)
{
    drawScreenTitle(window, "SETTINGS", 178.f);
    for (const auto *selector :
         {&_sfxVolumeSelector, &_musicVolumeSelector, &_autoFireSelector, &_contrastSelector, &_styleSelector})
        (*selector)->draw(window);
    window.draw(_controlsTitleText);
    for (const auto *buttons : {&_keyButtons, &_presetButtons, &_settingsButtons}) {
        for (const auto &button : *buttons)
            button->draw(window);
    }
}

MenuAction MainMenu::getAndResetAction()
{
    const auto action = _pendingAction;
    _pendingAction = MenuAction::None;
    return action;
}

const GameConfig &MainMenu::getConfig() const
{
    return _config;
}

} // namespace rtype::client::ui
