#include "ResourceManager.hpp"
#include "common/Constants.hpp"
#include "common/Protocol.hpp"
#include "common/SnapshotAssembler.hpp"
#include "net/AsioNetworkClient.hpp"
#include "ui/MainMenu.hpp"
#include "ui/Starfield.hpp"

#include <SFML/Graphics.hpp>

#include <cstdio>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>
#include <unordered_set>
#include <vector>

enum class AppState {
    Menu,
    Playing
};

int main(int argc, char **argv)
{
    std::string host = "127.0.0.1";
    std::uint16_t port = rtype::DefaultPort;
    bool directConnect = false;

    try {
        if (argc > 1) {
            host = argv[1];
            directConnect = true;
        }
        if (argc > 2) {
            port = static_cast<std::uint16_t>(std::stoul(argv[2]));
        }
    } catch (const std::exception &) {
        std::cerr << "Usage: " << argv[0] << " [host] [port]\n";
        return 84;
    }

    sf::RenderWindow window(sf::VideoMode({1280u, 720u}), "R-Type");
    window.setFramerateLimit(60);
    // We always draw in world coordinates (1920x1080); the view scales them to the window.
    window.setView(sf::View(sf::FloatRect({0.f, 0.f}, {rtype::WorldWidth, rtype::WorldHeight})));

    rtype::client::ResourceManager resources;
    const bool hasAssets = resources.loadDefaults();
    if (!hasAssets) {
        std::cerr << "Warning: Could not load some assets. Make sure to run from repository root.\n";
    }
    resources.playMusic("assets/audio/music.wav", 50.f, true);

    if (!resources.hasFont("default")) {
        std::cerr << "Error: Default font could not be loaded.\n";
        return 84;
    }

    rtype::client::ui::Starfield starfield;
    rtype::client::ui::MainMenu menu(resources.getFont("default"), &resources);
    rtype::client::AsioNetworkClient network;
    AppState appState = AppState::Menu;

    sf::Text waitingText(resources.getFont("default"), "CONNECTING TO SERVER...\nEnsure ./r-type_server is running\n\n[ESC] Return to Menu", 30);
    waitingText.setFillColor(sf::Color(255, 204, 0));
    waitingText.setPosition({600.f, 480.f});

    if (directConnect) {
        try {
            network.connect(host, port);
            appState = AppState::Playing;
        } catch (const std::exception &e) {
            std::cerr << "Direct connect failed: " << e.what() << '\n';
        }
    }

    sf::Text statsText(resources.getFont("default"), "", 22);
    statsText.setFillColor(sf::Color::White);
    statsText.setOutlineColor(sf::Color::Black);
    statsText.setOutlineThickness(2.f);
    statsText.setPosition({20.f, 20.f});

    rtype::protocol::SnapshotAssembler world;
    bool reportedConnected = false;
    bool showStats = false; // [F3]
    std::string rejectionReason = "the server refused the connection";
    sf::Clock clock;

    // Drops the session and goes back to the menu, which explains why.
    const auto leaveToMenu = [&](const std::string &message) {
        network.disconnect();
        appState = AppState::Menu;
        reportedConnected = false;
        world.reset();
        menu.setStatusMessage(message, sf::Color::Red);
    };

    while (window.isOpen()) {
        const float dt = clock.restart().asSeconds();
        const auto &cfg = menu.getConfig();
        resources.setSfxVolume(cfg.sfxVolume);
        resources.setMusicVolume(cfg.musicVolume);
        static float lastSfxVolume = 100.f;
        if (std::abs(cfg.sfxVolume - lastSfxVolume) > 1.f) {
            resources.playSound("shoot", 80.f);
            lastSfxVolume = cfg.sfxVolume;
        }

        while (const std::optional<sf::Event> event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window.close();
            }

            if (appState == AppState::Menu) {
                menu.handleEvent(*event, window);
            } else if (appState == AppState::Playing) {
                if (const auto *key = event->getIf<sf::Event::KeyPressed>()) {
                    if (key->code == sf::Keyboard::Key::F3)
                        showStats = !showStats;
                    if (key->code == sf::Keyboard::Key::Escape) {
                        network.disconnect();
                        appState = AppState::Menu;
                        reportedConnected = false;
                        world.reset();
                    }
                }
            }
        }

        if (appState == AppState::Menu) {
            menu.update(dt);
            const auto action = menu.getAndResetAction();
            if (action == rtype::client::ui::MenuAction::Exit) {
                window.close();
            } else if (action == rtype::client::ui::MenuAction::Connect) {
                const auto &config = menu.getConfig();
                try {
                    const auto targetPort = static_cast<std::uint16_t>(std::stoul(config.joinPort));
                    network.connect(config.joinIp, targetPort);
                    appState = AppState::Playing;
                    reportedConnected = false;
                } catch (const std::exception &e) {
                    menu.setStatusMessage("Connection failed: " + std::string(e.what()), sf::Color::Red);
                }
            }
        } else if (appState == AppState::Playing) {
            starfield.update(dt);

            if (network.connected() && !reportedConnected) {
                std::cout << "Connected to server!\n";
                reportedConnected = true;
            }

            std::uint8_t inputFlags = 0;
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Z) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Up))
                inputFlags |= rtype::protocol::InputFlag::Up;
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Down))
                inputFlags |= rtype::protocol::InputFlag::Down;
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Q) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left))
                inputFlags |= rtype::protocol::InputFlag::Left;
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Right))
                inputFlags |= rtype::protocol::InputFlag::Right;

            static float shootCooldown = 0.f;
            shootCooldown -= dt;
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Space)) {
                inputFlags |= rtype::protocol::InputFlag::Shoot;
                if (shootCooldown <= 0.f) {
                    resources.playSound("shoot", 80.f);
                    shootCooldown = 0.22f;
                }
            }

            if (network.connected())
                network.send(rtype::protocol::Input{inputFlags});

            const std::vector<rtype::protocol::EntityState> previousEntities = world.entities();
            const std::uint32_t previousTick = world.tick();
            bool newSession = false;
            while (auto message = network.poll()) {
                if (const auto *snapshot = std::get_if<rtype::protocol::Snapshot>(&*message)) {
                    world.add(*snapshot);
                } else if (std::holds_alternative<rtype::protocol::ConnectAccepted>(*message)) {
                    world.reset(); // a new session: the server's ticks may start again from zero
                    newSession = true;
                } else if (const auto *rejected = std::get_if<rtype::protocol::ConnectRejected>(&*message)) {
                    rejectionReason = rejected->reason == rtype::protocol::RejectReason::ServerFull
                                          ? "the server is full"
                                          : "the protocol versions differ";
                    std::cerr << "Connection rejected: " << rejectionReason << ".\n";
                }
            }

            // Compared once per drained batch: the packets of a split tick are only complete after the loop.
            if (!newSession && world.tick() != previousTick) {
                std::unordered_set<std::uint32_t> currentEnemies;
                for (const auto &entity : world.entities()) {
                    if (entity.type == rtype::protocol::EntityType::Enemy)
                        currentEnemies.insert(entity.netId);
                }
                for (const auto &previous : previousEntities) {
                    if (previous.type == rtype::protocol::EntityType::Enemy && !currentEnemies.contains(previous.netId) &&
                        previous.x > 30.f && previous.x < rtype::WorldWidth + 100.f) {
                        resources.playSound("explosion", 90.f);
                        break;
                    }
                }
            }

            switch (network.state()) {
            case rtype::client::ConnectionState::Rejected:
                leaveToMenu("Connection rejected: " + rejectionReason + ".");
                break;
            case rtype::client::ConnectionState::TimedOut:
                leaveToMenu("Could not reach the server. Check the address and that the server is running.");
                break;
            case rtype::client::ConnectionState::Lost:
                leaveToMenu("Connection to the server lost.");
                break;
            default:
                break;
            }
        }
        resources.updateAudio();

        window.clear(sf::Color(10, 10, 25));

        if (appState == AppState::Menu) {
            menu.draw(window);
        } else if (appState == AppState::Playing) {
            starfield.draw(window);

            if (!reportedConnected || world.entities().empty()) {
                window.draw(waitingText);
            }

            for (const auto &entity : world.entities()) {
                if (hasAssets) {
                    if (entity.type == rtype::protocol::EntityType::Player) {
                        sf::Sprite sprite(resources.getTexture("player"));
                        const int variant = static_cast<int>(entity.variant % 5);
                        sprite.setTextureRect(sf::IntRect({0, variant * 17}, {33, 17}));
                        sprite.setScale({2.f, 2.f});
                        sprite.setPosition({entity.x, entity.y});
                        window.draw(sprite);
                    } else if (entity.type == rtype::protocol::EntityType::PlayerMissile ||
                               entity.type == rtype::protocol::EntityType::EnemyMissile) {
                        sf::Sprite sprite(resources.getTexture("bullet"));
                        sprite.setTextureRect(sf::IntRect({248, 102}, {18, 12}));
                        sprite.setScale({2.f, 2.f});
                        sprite.setPosition({entity.x, entity.y});
                        window.draw(sprite);
                    } else if (entity.type == rtype::protocol::EntityType::Enemy) {
                        sf::Sprite sprite(resources.getTexture("enemy"));
                        sprite.setTextureRect(sf::IntRect({0, 0}, {33, 36}));
                        sprite.setScale({2.f, 2.f});
                        sprite.setPosition({entity.x, entity.y});
                        window.draw(sprite);
                    }
                }
            }

            if (showStats) {
                const auto stats = network.stats();
                char rtt[24] = "-- ms";
                if (stats.rttMs)
                    std::snprintf(rtt, sizeof rtt, "%.0f ms", *stats.rttMs);
                char line[160];
                const auto dropped = stats.traffic.totals.duplicatesDropped + stats.traffic.totals.outdatedDropped;
                std::snprintf(line, sizeof line,
                              "ping %s | down %.1f KB/s | up %.1f KB/s | loss %.1f%% | dropped %llu  [F3]", rtt,
                              stats.traffic.rates.receivedBytesPerSecond / 1024.0,
                              stats.traffic.rates.sentBytesPerSecond / 1024.0, stats.lossPercent,
                              static_cast<unsigned long long>(dropped));
                statsText.setString(line);
                window.draw(statsText);
            }
        }

        window.display();
    }

    if (network.connected())
        network.disconnect();

    return EXIT_SUCCESS;
}
