#include "ResourceManager.hpp"
#include "common/Constants.hpp"
#include "common/Protocol.hpp"
#include "net/AsioNetworkClient.hpp"
#include "ui/MainMenu.hpp"
#include "ui/Starfield.hpp"

#include <SFML/Graphics.hpp>

#include <array>
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

struct Explosion {
    sf::Vector2f position;
    float timer{0.f};
    float maxDuration{0.35f};
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

    sf::Text scoreText(resources.getFont("default"), "SCORE: 000000", 28);
    scoreText.setFillColor(sf::Color(0, 230, 255));
    scoreText.setPosition({50.f, 30.f});

    if (directConnect) {
        try {
            network.connect(host, port);
            appState = AppState::Playing;
        } catch (const std::exception &e) {
            std::cerr << "Direct connect failed: " << e.what() << '\n';
        }
    }

    std::vector<rtype::protocol::EntityState> entities;
    std::vector<Explosion> explosions;
    bool reportedConnected = false;
    sf::Clock clock;
    float totalTime = 0.f;

    while (window.isOpen()) {
        const float dt = clock.restart().asSeconds();
        totalTime += dt;
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
                    if (key->code == sf::Keyboard::Key::Escape) {
                        network.disconnect();
                        appState = AppState::Menu;
                        reportedConnected = false;
                        entities.clear();
                        explosions.clear();
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

            for (auto &exp : explosions) {
                exp.timer += dt;
            }
            std::erase_if(explosions, [](const Explosion &exp) {
                return exp.timer >= exp.maxDuration;
            });

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

            while (auto message = network.poll()) {
                if (const auto *snapshot = std::get_if<rtype::protocol::Snapshot>(&*message)) {
                    scoreText.setString("SCORE: " + std::to_string(snapshot->score));

                    if (!entities.empty()) {
                        std::unordered_set<std::uint32_t> currentIds;
                        for (const auto &ent : snapshot->entities) {
                            currentIds.insert(ent.netId);
                        }

                        std::vector<sf::Vector2f> playersPos;
                        for (const auto &ent : entities) {
                            if (ent.type == rtype::protocol::EntityType::Player)
                                playersPos.push_back({ent.x, ent.y});
                        }

                        for (const auto &prev : entities) {
                            if (!currentIds.contains(prev.netId)) {
                                if (prev.type == rtype::protocol::EntityType::Enemy &&
                                    prev.x > 30.f && prev.x < rtype::WorldWidth + 100.f) {
                                    resources.playSound("explosion", 90.f);
                                    explosions.push_back({sf::Vector2f(prev.x, prev.y), 0.f, 0.35f});
                                } else if (prev.type == rtype::protocol::EntityType::Player) {
                                    resources.playSound("hit", 100.f);
                                    resources.playSound("explosion", 100.f);
                                    explosions.push_back({sf::Vector2f(prev.x, prev.y), 0.f, 0.45f});
                                } else if (prev.type == rtype::protocol::EntityType::EnemyMissile) {
                                    for (const auto &pPos : playersPos) {
                                        float dx = prev.x - pPos.x;
                                        float dy = prev.y - pPos.y;
                                        if (dx * dx + dy * dy < 75.f * 75.f) {
                                            resources.playSound("hit", 95.f);
                                            explosions.push_back({sf::Vector2f(prev.x, prev.y), 0.f, 0.25f});
                                            break;
                                        }
                                    }
                                }
                            }
                        }
                    }
                    entities = snapshot->entities;
                } else if (const auto *rejected = std::get_if<rtype::protocol::ConnectRejected>(&*message)) {
                    std::cerr << "Server rejected connection (reason " << static_cast<int>(rejected->reason) << ").\n";
                }
            }
        }
        resources.updateAudio();

        window.clear(sf::Color(10, 10, 25));

        if (appState == AppState::Menu) {
            menu.draw(window);
        } else if (appState == AppState::Playing) {
            starfield.draw(window);

            if (!reportedConnected || entities.empty()) {
                window.draw(waitingText);
            }

            for (const auto &entity : entities) {
                if (hasAssets) {
                    if (entity.type == rtype::protocol::EntityType::Player) {
                        sf::Sprite sprite(resources.getTexture("player"));
                        const int variant = static_cast<int>(entity.variant % 5);
                        sprite.setTextureRect(sf::IntRect({0, variant * 17}, {33, 17}));
                        sprite.setScale({2.f, 2.f});
                        sprite.setPosition({entity.x, entity.y});
                        window.draw(sprite);
                    } else if (entity.type == rtype::protocol::EntityType::PlayerMissile) {
                        sf::Sprite sprite(resources.getTexture("bullet"));
                        sprite.setTextureRect(sf::IntRect({248, 102}, {18, 12}));
                        sprite.setScale({2.f, 2.f});
                        sprite.setColor(sf::Color(100, 220, 255));
                        sprite.setPosition({entity.x, entity.y});
                        window.draw(sprite);
                    } else if (entity.type == rtype::protocol::EntityType::EnemyMissile) {
                        sf::Sprite sprite(resources.getTexture("enemy_bullet"));
                        const int animFrame = static_cast<int>(totalTime * 14.f) % 6;
                        sprite.setTextureRect(sf::IntRect({animFrame * 17 + 1, 2}, {16, 13}));
                        sprite.setOrigin({8.f, 6.5f});
                        if (entity.variant == 1) {
                            sprite.setScale({4.f, 4.f});
                        } else {
                            sprite.setScale({2.8f, 2.8f});
                        }
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

            static const std::array<sf::IntRect, 6> explosionFrames = {
                sf::IntRect({72, 296}, {32, 24}),
                sf::IntRect({109, 296}, {32, 24}),
                sf::IntRect({146, 296}, {32, 24}),
                sf::IntRect({354, 296}, {32, 24}),
                sf::IntRect({391, 296}, {32, 24}),
                sf::IntRect({428, 296}, {32, 24})
            };

            for (const auto &exp : explosions) {
                sf::Sprite expSprite(resources.getTexture("bullet"));
                const float progress = exp.timer / exp.maxDuration;
                const std::size_t frame = std::clamp(static_cast<std::size_t>(progress * 6.f), std::size_t{0}, std::size_t{5});
                expSprite.setTextureRect(explosionFrames[frame]);
                expSprite.setOrigin({16.f, 12.f});
                expSprite.setScale({2.8f, 2.8f});
                expSprite.setPosition({exp.position.x + 25.f, exp.position.y + 20.f});
                window.draw(expSprite);
            }

            window.draw(scoreText);
        }

        window.display();
    }

    if (network.connected())
        network.disconnect();

    return EXIT_SUCCESS;
}
