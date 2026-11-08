#include "MainMenu.hpp"
#include "ResourceManager.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>

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
    const std::vector<std::string> labels = {"SOLO", "HOST GAME", "JOIN GAME", "SETTINGS", "QUIT"};
    const float startY = 320.f;
    const float btnWidth = 380.f;
    const float btnHeight = 55.f;
    const float posX = 960.f - (btnWidth / 2.f);

    for (std::size_t i = 0; i < labels.size(); ++i) {
        auto btn =
            std::make_unique<Button>(_font, labels[i], sf::Vector2f(posX, startY + (static_cast<float>(i) * 75.f)),
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
        _font, "GAME MODE", std::vector<std::string>{"SCENARIO", "ENDLESS"}, sf::Vector2f(posX, startY));
    startY += spacing;

    _hostDifficultySelector = std::make_unique<OptionSelector>(
        _font, "DIFFICULTY", std::vector<std::string>{"EASY", "NORMAL", "HARD"}, sf::Vector2f(posX, startY), 1);
    startY += spacing;

    _hostMissionSelector = std::make_unique<OptionSelector>(
        _font, "MISSION", std::vector<std::string>{"1 : DEEP SPACE", "2 : ASTEROID BELT", "3 : ALIEN CORE"},
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
    float startY = 295.f;

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
    startY += 85.f;

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
    _starfield.update(dt);

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

void MainMenu::setAccessibility(bool autoFire, bool highContrast)
{
    _config.autoFire = autoFire;
    _config.highContrast = highContrast;
    _autoFireSelector->setSelectedIndex(autoFire ? 1 : 0);
    _contrastSelector->setSelectedIndex(highContrast ? 1 : 0);
}

void MainMenu::handleEvent(const sf::Event &event, const sf::RenderWindow &window)
{
    sf::Vector2f mousePos{0.f, 0.f};
    if (const auto *mouseMoved = event.getIf<sf::Event::MouseMoved>()) {
        mousePos = window.mapPixelToCoords(mouseMoved->position);
    }

    if (_currentScreen == MenuScreen::Root) {
        if (event.is<sf::Event::MouseMoved>()) {
            for (std::size_t i = 0; i < _rootButtons.size(); ++i) {
                if (_rootButtons[i]->contains(mousePos)) {
                    _rootSelectedIndex = static_cast<int>(i);
                    for (std::size_t j = 0; j < _rootButtons.size(); ++j)
                        _rootButtons[j]->setSelected(j == i);
                }
            }
        } else if (const auto *click = event.getIf<sf::Event::MouseButtonPressed>()) {
            if (click->button == sf::Mouse::Button::Left) {
                const auto clickedPos = window.mapPixelToCoords(click->position);
                for (std::size_t i = 0; i < _rootButtons.size(); ++i) {
                    if (_rootButtons[i]->handleMouseClick(clickedPos)) {
                        _rootSelectedIndex = static_cast<int>(i);
                        triggerCurrentSelection();
                        return;
                    }
                }
            }
        } else if (const auto *key = event.getIf<sf::Event::KeyPressed>()) {
            if (key->code == sf::Keyboard::Key::Up || key->code == sf::Keyboard::Key::Z) {
                navigateMenu(-1);
            } else if (key->code == sf::Keyboard::Key::Down || key->code == sf::Keyboard::Key::S) {
                navigateMenu(1);
            } else if (key->code == sf::Keyboard::Key::Enter || key->code == sf::Keyboard::Key::Space) {
                triggerCurrentSelection();
            }
        }
    } else if (_currentScreen == MenuScreen::HostGame) {
        _hostPortInput->handleEvent(event);

        if (event.is<sf::Event::MouseMoved>()) {
            _hostModeSelector->handleMouseMove(mousePos);
            _hostDifficultySelector->handleMouseMove(mousePos);
            _hostMissionSelector->handleMouseMove(mousePos);
            for (auto &btn : _hostButtons) {
                btn->handleMouseMove(mousePos);
            }
        } else if (const auto *click = event.getIf<sf::Event::MouseButtonPressed>()) {
            if (click->button == sf::Mouse::Button::Left) {
                const auto clickedPos = window.mapPixelToCoords(click->position);
                if (_hostModeSelector->handleMouseClick(clickedPos))
                    playUiSound("click");
                if (_hostDifficultySelector->handleMouseClick(clickedPos))
                    playUiSound("click");
                if (_hostMissionSelector->handleMouseClick(clickedPos))
                    playUiSound("click");
                if (_hostPortInput->handleMouseClick(clickedPos))
                    playUiSound("click");

                if (_hostButtons[0]->handleMouseClick(clickedPos)) {
                    playUiSound("click");
                    _config.gameMode = static_cast<int>(_hostModeSelector->getSelectedIndex());
                    _config.difficulty = static_cast<int>(_hostDifficultySelector->getSelectedIndex());
                    _config.mission = static_cast<int>(_hostMissionSelector->getSelectedIndex());
                    _config.hostPort = _hostPortInput->getValue().empty() ? "4242" : _hostPortInput->getValue();
                    _pendingAction = MenuAction::Host;
                } else if (_hostButtons[1]->handleMouseClick(clickedPos)) {
                    playUiSound("click");
                    _currentScreen = MenuScreen::Root;
                }
            }
        } else if (const auto *key = event.getIf<sf::Event::KeyPressed>()) {
            if (key->code == sf::Keyboard::Key::Escape) {
                playUiSound("click");
                _currentScreen = MenuScreen::Root;
            } else if (key->code == sf::Keyboard::Key::Enter) {
                playUiSound("click");
                _config.gameMode = static_cast<int>(_hostModeSelector->getSelectedIndex());
                _config.difficulty = static_cast<int>(_hostDifficultySelector->getSelectedIndex());
                _config.mission = static_cast<int>(_hostMissionSelector->getSelectedIndex());
                _config.hostPort = _hostPortInput->getValue().empty() ? "4242" : _hostPortInput->getValue();
                _pendingAction = MenuAction::Host;
            } else {
                _hostModeSelector->handleKeyPress(key->code);
                _hostDifficultySelector->handleKeyPress(key->code);
                _hostMissionSelector->handleKeyPress(key->code);
            }
        }
    } else if (_currentScreen == MenuScreen::JoinGame) {
        _joinIpInput->handleEvent(event);
        _joinPortInput->handleEvent(event);

        if (event.is<sf::Event::MouseMoved>()) {
            for (auto &btn : _joinButtons) {
                btn->handleMouseMove(mousePos);
            }
        } else if (const auto *click = event.getIf<sf::Event::MouseButtonPressed>()) {
            if (click->button == sf::Mouse::Button::Left) {
                const auto clickedPos = window.mapPixelToCoords(click->position);
                _joinIpInput->handleMouseClick(clickedPos);
                _joinPortInput->handleMouseClick(clickedPos);

                if (_joinButtons[0]->handleMouseClick(clickedPos)) {
                    playUiSound("click");
                    _config.joinIp = _joinIpInput->getValue().empty() ? "127.0.0.1" : _joinIpInput->getValue();
                    _config.joinPort = _joinPortInput->getValue().empty() ? "4242" : _joinPortInput->getValue();
                    _pendingAction = MenuAction::Connect;
                } else if (_joinButtons[1]->handleMouseClick(clickedPos)) {
                    playUiSound("click");
                    _currentScreen = MenuScreen::Root;
                }
            }
        } else if (const auto *key = event.getIf<sf::Event::KeyPressed>()) {
            if (key->code == sf::Keyboard::Key::Escape) {
                playUiSound("click");
                _currentScreen = MenuScreen::Root;
            } else if (key->code == sf::Keyboard::Key::Enter) {
                playUiSound("click");
                _config.joinIp = _joinIpInput->getValue().empty() ? "127.0.0.1" : _joinIpInput->getValue();
                _config.joinPort = _joinPortInput->getValue().empty() ? "4242" : _joinPortInput->getValue();
                _pendingAction = MenuAction::Connect;
            }
        }
    } else if (_currentScreen == MenuScreen::Settings) {
        if (event.is<sf::Event::MouseMoved>()) {
            _sfxVolumeSelector->handleMouseMove(mousePos);
            _musicVolumeSelector->handleMouseMove(mousePos);
            _autoFireSelector->handleMouseMove(mousePos);
            _contrastSelector->handleMouseMove(mousePos);
            for (auto &btn : _keyButtons) {
                btn->handleMouseMove(mousePos);
            }
            for (auto &btn : _presetButtons) {
                btn->handleMouseMove(mousePos);
            }
            for (auto &btn : _settingsButtons) {
                btn->handleMouseMove(mousePos);
            }
        } else if (const auto *click = event.getIf<sf::Event::MouseButtonPressed>()) {
            if (click->button == sf::Mouse::Button::Left) {
                const auto clickedPos = window.mapPixelToCoords(click->position);

                if (_rebindingAction != -1) {
                    bool rebindAnother = false;
                    for (std::size_t i = 0; i < _keyButtons.size(); ++i) {
                        if (_keyButtons[i]->handleMouseClick(clickedPos)) {
                            playUiSound("click");
                            _rebindingAction = static_cast<int>(i);
                            updateKeyButtonLabels();
                            setStatusMessage("Press key for action or Escape to cancel", sf::Color::Yellow);
                            rebindAnother = true;
                            break;
                        }
                    }
                    if (!rebindAnother) {
                        _rebindingAction = -1;
                        updateKeyButtonLabels();
                        setStatusMessage("Rebinding cancelled", sf::Color::White);
                    }
                    return;
                }

                bool volumeChanged = false;
                if (_sfxVolumeSelector->handleMouseClick(clickedPos)) {
                    playUiSound("click");
                    _config.sfxVolume = static_cast<float>(_sfxVolumeSelector->getSelectedIndex()) * 25.f;
                    volumeChanged = true;
                }
                if (_musicVolumeSelector->handleMouseClick(clickedPos)) {
                    playUiSound("click");
                    _config.musicVolume = static_cast<float>(_musicVolumeSelector->getSelectedIndex()) * 25.f;
                    volumeChanged = true;
                }
                if (_autoFireSelector->handleMouseClick(clickedPos)) {
                    playUiSound("click");
                    _config.autoFire = _autoFireSelector->getSelectedIndex() == 1;
                    volumeChanged = true;
                }
                if (_contrastSelector->handleMouseClick(clickedPos)) {
                    playUiSound("click");
                    _config.highContrast = _contrastSelector->getSelectedIndex() == 1;
                    volumeChanged = true;
                }
                if (volumeChanged) {
                    _config.saveToFile("config.json");
                }

                for (std::size_t i = 0; i < _keyButtons.size(); ++i) {
                    if (_keyButtons[i]->handleMouseClick(clickedPos)) {
                        playUiSound("click");
                        _rebindingAction = static_cast<int>(i);
                        updateKeyButtonLabels();
                        setStatusMessage("Press any key to assign, or Escape to cancel", sf::Color::Yellow);
                        return;
                    }
                }

                if (_presetButtons.size() >= 3) {
                    if (_presetButtons[0]->handleMouseClick(clickedPos)) {
                        playUiSound("click");
                        _config.applyAzerty();
                        _config.saveToFile("config.json");
                        updateKeyButtonLabels();
                        setStatusMessage("AZERTY profile applied (Z, Q, S, D, Space)", sf::Color::Green);
                        return;
                    }
                    if (_presetButtons[1]->handleMouseClick(clickedPos)) {
                        playUiSound("click");
                        _config.applyQwerty();
                        _config.saveToFile("config.json");
                        updateKeyButtonLabels();
                        setStatusMessage("QWERTY profile applied (W, A, S, D, Space)", sf::Color::Green);
                        return;
                    }
                    if (_presetButtons[2]->handleMouseClick(clickedPos)) {
                        playUiSound("click");
                        _config.resetToDefaults();
                        _config.saveToFile("config.json");
                        updateKeyButtonLabels();
                        setStatusMessage("Controls reset to defaults", sf::Color::Green);
                        return;
                    }
                }

                if (!_settingsButtons.empty() && _settingsButtons[0]->handleMouseClick(clickedPos)) {
                    playUiSound("click");
                    _config.saveToFile("config.json");
                    _currentScreen = MenuScreen::Root;
                }
            }
        } else if (const auto *key = event.getIf<sf::Event::KeyPressed>()) {
            if (_rebindingAction != -1) {
                if (key->code == sf::Keyboard::Key::Escape) {
                    playUiSound("click");
                    _rebindingAction = -1;
                    updateKeyButtonLabels();
                    setStatusMessage("Rebinding cancelled", sf::Color::White);
                } else {
                    const std::vector<std::string> names = {"Up", "Down", "Left", "Right", "Shoot"};
                    std::string actionName =
                        (_rebindingAction >= 0 && _rebindingAction < static_cast<int>(names.size()))
                            ? names[_rebindingAction]
                            : "";
                    std::string conflict = _config.getConflictAction(key->code, actionName);

                    if (!conflict.empty()) {
                        playUiSound("hit");
                        setStatusMessage("Conflict: [" + GameConfig::keyToString(key->code) + "] is already used for " +
                                             conflict + "!",
                                         sf::Color(255, 80, 80));
                    } else if (key->code == sf::Keyboard::Key::Unknown) {
                        playUiSound("hit");
                        setStatusMessage("Unsupported key pressed!", sf::Color(255, 80, 80));
                    } else {
                        if (_rebindingAction == 0)
                            _config.keyUp = key->code;
                        else if (_rebindingAction == 1)
                            _config.keyDown = key->code;
                        else if (_rebindingAction == 2)
                            _config.keyLeft = key->code;
                        else if (_rebindingAction == 3)
                            _config.keyRight = key->code;
                        else if (_rebindingAction == 4)
                            _config.keyShoot = key->code;

                        playUiSound("click");
                        _rebindingAction = -1;
                        _config.saveToFile("config.json");
                        updateKeyButtonLabels();
                        setStatusMessage("Action " + actionName + " bound to [" + GameConfig::keyToString(key->code) +
                                             "]",
                                         sf::Color::Green);
                    }
                }
                return;
            }

            if (key->code == sf::Keyboard::Key::Escape) {
                playUiSound("click");
                _config.saveToFile("config.json");
                _currentScreen = MenuScreen::Root;
            }
        }
    }
}

void MainMenu::navigateMenu(int direction)
{
    if (_currentScreen == MenuScreen::Root) {
        _rootSelectedIndex += direction;
        if (_rootSelectedIndex < 0) {
            _rootSelectedIndex = static_cast<int>(_rootButtons.size()) - 1;
        } else if (_rootSelectedIndex >= static_cast<int>(_rootButtons.size())) {
            _rootSelectedIndex = 0;
        }

        for (std::size_t i = 0; i < _rootButtons.size(); ++i) {
            _rootButtons[i]->setSelected(static_cast<int>(i) == _rootSelectedIndex);
        }
    }
}

void MainMenu::triggerCurrentSelection()
{
    playUiSound("click");
    if (_currentScreen == MenuScreen::Root) {
        if (_rootSelectedIndex == 0) {
            _pendingAction = MenuAction::Play;
        } else if (_rootSelectedIndex == 1) {
            _currentScreen = MenuScreen::HostGame;
        } else if (_rootSelectedIndex == 2) {
            _currentScreen = MenuScreen::JoinGame;
        } else if (_rootSelectedIndex == 3) {
            _currentScreen = MenuScreen::Settings;
        } else if (_rootSelectedIndex == 4) {
            _pendingAction = MenuAction::Exit;
        }
    }
}

void MainMenu::draw(sf::RenderWindow &window)
{
    _starfield.draw(window);

    window.draw(_titleText);
    window.draw(_subtitleText);

    if (_currentScreen == MenuScreen::Root) {
        for (const auto &btn : _rootButtons) {
            btn->draw(window);
        }
    } else if (_currentScreen == MenuScreen::HostGame) {
        _screenTitleText.setString("--- HOST GAME ---");
        const auto b = _screenTitleText.getLocalBounds();
        _screenTitleText.setPosition({960.f - (b.size.x / 2.f), 290.f});
        window.draw(_screenTitleText);

        _hostModeSelector->draw(window);
        _hostDifficultySelector->draw(window);
        _hostMissionSelector->draw(window);
        _hostPortInput->draw(window);
        for (const auto &btn : _hostButtons) {
            btn->draw(window);
        }
    } else if (_currentScreen == MenuScreen::JoinGame) {
        _screenTitleText.setString("--- JOIN GAME ---");
        const auto b = _screenTitleText.getLocalBounds();
        _screenTitleText.setPosition({960.f - (b.size.x / 2.f), 310.f});
        window.draw(_screenTitleText);

        _joinIpInput->draw(window);
        _joinPortInput->draw(window);
        for (const auto &btn : _joinButtons) {
            btn->draw(window);
        }
    } else if (_currentScreen == MenuScreen::Settings) {
        _screenTitleText.setString("--- SETTINGS ---");
        const auto b = _screenTitleText.getLocalBounds();
        _screenTitleText.setPosition({960.f - (b.size.x / 2.f), 215.f});
        window.draw(_screenTitleText);

        _sfxVolumeSelector->draw(window);
        _musicVolumeSelector->draw(window);
        _autoFireSelector->draw(window);
        _contrastSelector->draw(window);
        window.draw(_controlsTitleText);

        for (const auto &btn : _keyButtons) {
            btn->draw(window);
        }
        for (const auto &btn : _presetButtons) {
            btn->draw(window);
        }
        for (const auto &btn : _settingsButtons) {
            btn->draw(window);
        }
    }

    if (!_statusText.getString().isEmpty()) {
        window.draw(_statusText);
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
