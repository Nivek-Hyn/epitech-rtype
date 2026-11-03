#include "MainMenu.hpp"

namespace rtype::client::ui {

MainMenu::MainMenu(const sf::Font &font)
    : _font(font),
      _titleText(font, "R - T Y P E", 80),
      _subtitleText(font, "A SPACE SHOOTER", 22),
      _screenTitleText(font, "", 36),
      _statusText(font, "", 22),
      _controlsInfo(font, "", 22)
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
    const std::vector<std::string> labels = {"PLAY", "HOST GAME", "JOIN GAME", "SETTINGS", "QUIT"};
    const float startY = 320.f;
    const float btnWidth = 380.f;
    const float btnHeight = 55.f;
    const float posX = 960.f - (btnWidth / 2.f);

    for (std::size_t i = 0; i < labels.size(); ++i) {
        auto btn = std::make_unique<Button>(_font, labels[i], sf::Vector2f(posX, startY + (static_cast<float>(i) * 75.f)), sf::Vector2f(btnWidth, btnHeight));
        _rootButtons.push_back(std::move(btn));
    }
    _rootButtons[0]->setSelected(true);
}

void MainMenu::initHostScreen()
{
    const float posX = 680.f;
    float startY = 360.f;
    const float spacing = 80.f;

    _hostModeSelector = std::make_unique<OptionSelector>(_font, "GAME MODE", std::vector<std::string>{"SCENARIO", "ENDLESS"}, sf::Vector2f(posX, startY));
    startY += spacing;

    _hostDifficultySelector = std::make_unique<OptionSelector>(_font, "DIFFICULTY", std::vector<std::string>{"EASY", "NORMAL", "HARD"}, sf::Vector2f(posX, startY), 1);
    startY += spacing;

    _hostMissionSelector = std::make_unique<OptionSelector>(_font, "MISSION", std::vector<std::string>{"1 : DEEP SPACE", "2 : ASTEROID BELT", "3 : ALIEN CORE"}, sf::Vector2f(posX, startY));
    startY += spacing;

    _hostPortInput = std::make_unique<TextInput>(_font, "SERVER PORT", _config.hostPort, sf::Vector2f(posX, startY), sf::Vector2f(340.f, 50.f), 6);
    startY += 110.f;

    const float btnWidth = 260.f;
    auto startBtn = std::make_unique<Button>(_font, "LAUNCH SERVER", sf::Vector2f(680.f, startY), sf::Vector2f(btnWidth, 55.f));
    auto backBtn = std::make_unique<Button>(_font, "BACK", sf::Vector2f(980.f, startY), sf::Vector2f(btnWidth, 55.f));

    _hostButtons.push_back(std::move(startBtn));
    _hostButtons.push_back(std::move(backBtn));
}

void MainMenu::initJoinScreen()
{
    const float posX = 680.f;
    float startY = 400.f;

    _joinIpInput = std::make_unique<TextInput>(_font, "SERVER IP", _config.joinIp, sf::Vector2f(posX, startY), sf::Vector2f(340.f, 50.f), 20);
    startY += 90.f;

    _joinPortInput = std::make_unique<TextInput>(_font, "SERVER PORT", _config.joinPort, sf::Vector2f(posX, startY), sf::Vector2f(340.f, 50.f), 6);
    startY += 120.f;

    const float btnWidth = 260.f;
    auto connectBtn = std::make_unique<Button>(_font, "CONNECT", sf::Vector2f(680.f, startY), sf::Vector2f(btnWidth, 55.f));
    auto backBtn = std::make_unique<Button>(_font, "BACK", sf::Vector2f(980.f, startY), sf::Vector2f(btnWidth, 55.f));

    _joinButtons.push_back(std::move(connectBtn));
    _joinButtons.push_back(std::move(backBtn));
}

void MainMenu::initSettingsScreen()
{
    const float posX = 680.f;
    float startY = 380.f;

    _sfxVolumeSelector = std::make_unique<OptionSelector>(_font, "SFX VOLUME", std::vector<std::string>{"MUTE", "25%", "50%", "75%", "100%"}, sf::Vector2f(posX, startY), 4);
    startY += 80.f;

    _musicVolumeSelector = std::make_unique<OptionSelector>(_font, "MUSIC VOLUME", std::vector<std::string>{"MUTE", "25%", "50%", "75%", "100%"}, sf::Vector2f(posX, startY), 4);
    startY += 100.f;

    _controlsInfo.setString("CONTROLS:\n  Move: [Z][Q][S][D] or [Arrow Keys]\n  Shoot: [Space]");
    _controlsInfo.setFillColor(sf::Color(170, 185, 215));
    _controlsInfo.setPosition({posX + 40.f, startY});

    startY += 130.f;
    auto backBtn = std::make_unique<Button>(_font, "BACK", sf::Vector2f(830.f, startY), sf::Vector2f(260.f, 55.f));
    _settingsButtons.push_back(std::move(backBtn));
}

void MainMenu::setStatusMessage(const std::string &message, sf::Color color)
{
    _statusText.setString(message);
    _statusText.setFillColor(color);
    const auto bounds = _statusText.getLocalBounds();
    _statusText.setPosition({960.f - (bounds.size.x / 2.f), 970.f});
    _statusTimer = 5.0f;
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
                _hostModeSelector->handleMouseClick(clickedPos);
                _hostDifficultySelector->handleMouseClick(clickedPos);
                _hostMissionSelector->handleMouseClick(clickedPos);
                _hostPortInput->handleMouseClick(clickedPos);

                if (_hostButtons[0]->handleMouseClick(clickedPos)) {
                    _config.gameMode = static_cast<int>(_hostModeSelector->getSelectedIndex());
                    _config.difficulty = static_cast<int>(_hostDifficultySelector->getSelectedIndex());
                    _config.mission = static_cast<int>(_hostMissionSelector->getSelectedIndex());
                    _config.hostPort = _hostPortInput->getValue().empty() ? "4242" : _hostPortInput->getValue();

                    setStatusMessage("Server configured: [Mode: " + _hostModeSelector->getSelectedValue() +
                                     " | Difficulty: " + _hostDifficultySelector->getSelectedValue() +
                                     " | Port: " + _config.hostPort + "]", sf::Color::Green);
                } else if (_hostButtons[1]->handleMouseClick(clickedPos)) {
                    _currentScreen = MenuScreen::Root;
                }
            }
        } else if (const auto *key = event.getIf<sf::Event::KeyPressed>()) {
            if (key->code == sf::Keyboard::Key::Escape) {
                _currentScreen = MenuScreen::Root;
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
                    _config.joinIp = _joinIpInput->getValue().empty() ? "127.0.0.1" : _joinIpInput->getValue();
                    _config.joinPort = _joinPortInput->getValue().empty() ? "4242" : _joinPortInput->getValue();
                    _pendingAction = MenuAction::Connect;
                } else if (_joinButtons[1]->handleMouseClick(clickedPos)) {
                    _currentScreen = MenuScreen::Root;
                }
            }
        } else if (const auto *key = event.getIf<sf::Event::KeyPressed>()) {
            if (key->code == sf::Keyboard::Key::Escape) {
                _currentScreen = MenuScreen::Root;
            } else if (key->code == sf::Keyboard::Key::Enter) {
                _config.joinIp = _joinIpInput->getValue().empty() ? "127.0.0.1" : _joinIpInput->getValue();
                _config.joinPort = _joinPortInput->getValue().empty() ? "4242" : _joinPortInput->getValue();
                _pendingAction = MenuAction::Connect;
            }
        }
    } else if (_currentScreen == MenuScreen::Settings) {
        if (event.is<sf::Event::MouseMoved>()) {
            _sfxVolumeSelector->handleMouseMove(mousePos);
            _musicVolumeSelector->handleMouseMove(mousePos);
            for (auto &btn : _settingsButtons) {
                btn->handleMouseMove(mousePos);
            }
        } else if (const auto *click = event.getIf<sf::Event::MouseButtonPressed>()) {
            if (click->button == sf::Mouse::Button::Left) {
                const auto clickedPos = window.mapPixelToCoords(click->position);
                _sfxVolumeSelector->handleMouseClick(clickedPos);
                _musicVolumeSelector->handleMouseClick(clickedPos);

                if (_settingsButtons[0]->handleMouseClick(clickedPos)) {
                    _currentScreen = MenuScreen::Root;
                }
            }
        } else if (const auto *key = event.getIf<sf::Event::KeyPressed>()) {
            if (key->code == sf::Keyboard::Key::Escape) {
                _currentScreen = MenuScreen::Root;
            } else {
                _sfxVolumeSelector->handleKeyPress(key->code);
                _musicVolumeSelector->handleKeyPress(key->code);
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
    if (_currentScreen == MenuScreen::Root) {
        if (_rootSelectedIndex == 0) {
            _pendingAction = MenuAction::Connect;
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
        _screenTitleText.setPosition({960.f - (b.size.x / 2.f), 300.f});
        window.draw(_screenTitleText);

        _sfxVolumeSelector->draw(window);
        _musicVolumeSelector->draw(window);
        window.draw(_controlsInfo);
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

}
