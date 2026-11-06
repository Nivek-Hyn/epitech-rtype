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

    if (!resources.hasFont("default")) {
        std::cerr << "Error: Default font could not be loaded.\n";
        return 84;
    }

    rtype::client::ui::Starfield starfield;
    rtype::client::ui::MainMenu menu(resources.getFont("default"), &resources);
    const auto &initialConfig = menu.getConfig();
    resources.setSfxVolume(initialConfig.sfxVolume);
    resources.setMusicVolume(initialConfig.musicVolume);
    resources.playMusic("assets/audio/music.wav", initialConfig.musicVolume, true);

    rtype::client::AsioNetworkClient network;
    AppState appState = AppState::Menu;

    sf::Text waitingText(resources.getFont("default"), "CONNECTING TO SERVER...\nEnsure ./r-type_server is running\n\n[ESC] Return to Menu", 30);
    waitingText.setFillColor(sf::Color(255, 204, 0));
    waitingText.setPosition({600.f, 480.f});

    sf::Text scoreText(resources.getFont("default"), "SCORE: 000000", 28);
    scoreText.setFillColor(sf::Color(0, 230, 255));
    scoreText.setPosition({50.f, 30.f});

    sf::Text waveHudText(resources.getFont("default"), "WAVE: 1", 28);
    waveHudText.setFillColor(sf::Color(255, 204, 0));
    waveHudText.setPosition({rtype::WorldWidth - 220.f, 30.f});

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

    float damageFlashTimer = 0.f;
    constexpr float DamageFlashDuration = 0.45f;
    float screenShakeTimer = 0.f;
    float screenShakeIntensity = 0.f;

    int currentWave = 1;
    float waveDisplayTimer = 0.f;
    constexpr float WaveDisplayDuration = 2.8f;
    float waveCooldownTimer = 0.f;
    bool waveStarted = false;
    int lastEnemyCount = 0;

    bool gameOver = false;
    bool gameWon = false;
    std::uint32_t lastScore = 0;
    std::optional<std::uint8_t> localPlayerId;
    float localPlayerTilt = 0.0f;
    std::unordered_map<std::uint32_t, float> playerYHistory;
    std::unordered_map<std::uint32_t, float> playerTiltMap;

    auto resetGameVisualState = [&]() {
        damageFlashTimer = 0.f;
        screenShakeTimer = 0.f;
        waveDisplayTimer = 0.f;
        waveCooldownTimer = 0.f;
        currentWave = 1;
        waveStarted = false;
        lastEnemyCount = 0;
        gameOver = false;
        gameWon = false;
        playerWasSpawned = false;
        lastScore = 0;
        localPlayerId.reset();
        localPlayerTilt = 0.0f;
        playerYHistory.clear();
        playerTiltMap.clear();
        entities.clear();
        explosions.clear();
        reportedConnected = false;
    };

    while (window.isOpen()) {
        const float dt = clock.restart().asSeconds();
        totalTime += dt;
        const auto &cfg = menu.getConfig();
        resources.setSfxVolume(cfg.sfxVolume);
        resources.setMusicVolume(cfg.musicVolume);
        static float lastSfxVolume = cfg.sfxVolume;
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
                        resetGameVisualState();
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

            if (damageFlashTimer > 0.f)
                damageFlashTimer -= dt;
            if (screenShakeTimer > 0.f)
                screenShakeTimer -= dt;
            if (waveDisplayTimer > 0.f)
                waveDisplayTimer -= dt;
            if (waveCooldownTimer > 0.f)
                waveCooldownTimer -= dt;

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
            float targetTilt = 0.0f;
            if (!gameOver) {
                if (sf::Keyboard::isKeyPressed(cfg.keyUp) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Up)) {
                    inputFlags |= rtype::protocol::InputFlag::Up;
                    targetTilt += 2.0f;
                }
                if (sf::Keyboard::isKeyPressed(cfg.keyDown) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Down)) {
                    inputFlags |= rtype::protocol::InputFlag::Down;
                    targetTilt -= 2.0f;
                }
                if (sf::Keyboard::isKeyPressed(cfg.keyLeft) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left))
                    inputFlags |= rtype::protocol::InputFlag::Left;
                if (sf::Keyboard::isKeyPressed(cfg.keyRight) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Right))
                    inputFlags |= rtype::protocol::InputFlag::Right;

                static float shootCooldown = 0.f;
                shootCooldown -= dt;
                if (sf::Keyboard::isKeyPressed(cfg.keyShoot)) {
                    inputFlags |= rtype::protocol::InputFlag::Shoot;
                    if (shootCooldown <= 0.f) {
                        resources.playSound("shoot", 80.f);
                        shootCooldown = 0.22f;
                    }
                }

                if (network.connected())
                    network.send(rtype::protocol::Input{inputFlags});
            }

            if (localPlayerTilt < targetTilt)
                localPlayerTilt = std::min(targetTilt, localPlayerTilt + dt * 14.f);
            else if (localPlayerTilt > targetTilt)
                localPlayerTilt = std::max(targetTilt, localPlayerTilt - dt * 14.f);

            while (auto message = network.poll()) {
                if (const auto *accepted = std::get_if<rtype::protocol::ConnectAccepted>(&*message)) {
                    localPlayerId = accepted->playerId;
                } else if (const auto *snapshot = std::get_if<rtype::protocol::Snapshot>(&*message)) {
                    lastScore = snapshot->score;

                    int enemyCount = 0;
                    for (const auto &ent : snapshot->entities) {
                        if (ent.type == rtype::protocol::EntityType::Enemy)
                            enemyCount++;
                    }

                    // The server decides: a dead player may still have lives left.
                    if (snapshot->state == rtype::protocol::GameState::GameOver) {
                        gameOver = true;
                        gameWon = false;
                    }
                    if (playerCount > 0) {
                        playerWasSpawned = true;
                    }

                    if (snapshot->state == rtype::protocol::GameState::Finished) {
                        gameOver = true;
                        gameWon = true;
                    } else if (playerWasSpawned && playerCount == 0 && !gameOver) {
                        gameOver = true;
                        gameWon = false;
                    }

                    if (reportedConnected && !waveStarted) {
                        waveStarted = true;
                        currentWave = 1;
                        waveDisplayTimer = WaveDisplayDuration;
                        waveCooldownTimer = 6.0f;
                    } else if (waveStarted) {
                        if (waveCooldownTimer <= 0.f && lastEnemyCount == 0 && enemyCount > 0) {
                            currentWave++;
                            waveDisplayTimer = WaveDisplayDuration;
                            waveCooldownTimer = 8.0f;
                        }
                    }
                    lastEnemyCount = enemyCount;

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
                                if ((prev.type == rtype::protocol::EntityType::Enemy ||
                                     prev.type == rtype::protocol::EntityType::Obstacle) &&
                                    prev.x > 30.f && prev.x < rtype::WorldWidth + 100.f) {
                                    resources.playSound("explosion", 90.f);
                                    explosions.push_back({sf::Vector2f(prev.x, prev.y), 0.f, 0.35f});
                                } else if (prev.type == rtype::protocol::EntityType::Player) {
                                    resources.playSound("hit", 100.f);
                                    resources.playSound("explosion", 100.f);
                                    explosions.push_back({sf::Vector2f(prev.x, prev.y), 0.f, 0.45f});
                                    damageFlashTimer = DamageFlashDuration;
                                    screenShakeTimer = 0.35f;
                                    screenShakeIntensity = 15.f;
                                } else if (prev.type == rtype::protocol::EntityType::EnemyMissile) {
                                    for (const auto &pPos : playersPos) {
                                        float dx = prev.x - pPos.x;
                                        float dy = prev.y - pPos.y;
                                        if (dx * dx + dy * dy < 75.f * 75.f) {
                                            resources.playSound("hit", 95.f);
                                            explosions.push_back({sf::Vector2f(prev.x, prev.y), 0.f, 0.25f});
                                            damageFlashTimer = std::max(damageFlashTimer, 0.25f);
                                            screenShakeTimer = std::max(screenShakeTimer, 0.2f);
                                            screenShakeIntensity = std::max(screenShakeIntensity, 8.f);
                                            break;
                                        }
                                    }
                                } else if (prev.type == rtype::protocol::EntityType::PlayerMissile) {
                                    for (const auto &ent : snapshot->entities) {
                                        if (ent.type == rtype::protocol::EntityType::Enemy) {
                                            float hitRadius = 65.f;
                                            float offsetY = 0.f;
                                            if (ent.variant == 6) {
                                                hitRadius = 180.f;
                                                offsetY = -60.f;
                                            } else if (ent.variant == 5) {
                                                hitRadius = 130.f;
                                                offsetY = -20.f;
                                            } else if (ent.variant == 4) {
                                                hitRadius = 80.f;
                                            }
                                            float dx = prev.x - ent.x;
                                            float dy = prev.y - (ent.y + offsetY);
                                            if (dx * dx + dy * dy < hitRadius * hitRadius) {
                                                resources.playSound("hit", 95.f);
                                                explosions.push_back({sf::Vector2f(prev.x, prev.y), 0.f, 0.2f});
                                                break;
                                            }
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

        sf::View currentView(sf::FloatRect({0.f, 0.f}, {rtype::WorldWidth, rtype::WorldHeight}));
        if (appState == AppState::Playing && screenShakeTimer > 0.f) {
            const float shakeFactor = (screenShakeTimer / 0.35f) * screenShakeIntensity;
            const float offsetX = (static_cast<float>(rand() % 200 - 100) / 100.f) * shakeFactor;
            const float offsetY = (static_cast<float>(rand() % 200 - 100) / 100.f) * shakeFactor;
            currentView.setCenter({rtype::WorldWidth / 2.f + offsetX, rtype::WorldHeight / 2.f + offsetY});
        }
        window.setView(currentView);

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
                        float tilt = 0.0f;
                        if (localPlayerId.has_value() && entity.variant == *localPlayerId) {
                            tilt = localPlayerTilt;
                        } else {
                            auto it = playerYHistory.find(entity.netId);
                            float desiredTilt = 0.0f;
                            if (it != playerYHistory.end()) {
                                const float dy = entity.y - it->second;
                                if (dy < -0.4f) desiredTilt = 2.0f;
                                else if (dy > 0.4f) desiredTilt = -2.0f;
                            }
                            playerYHistory[entity.netId] = entity.y;
                            float &curTilt = playerTiltMap[entity.netId];
                            if (curTilt < desiredTilt) curTilt = std::min(desiredTilt, curTilt + dt * 14.f);
                            else if (curTilt > desiredTilt) curTilt = std::max(desiredTilt, curTilt - dt * 14.f);
                            tilt = curTilt;
                        }

                        int col = 2;
                        if (tilt > 1.1f) col = 4;
                        else if (tilt > 0.25f) col = 3;
                        else if (tilt < -1.1f) col = 0;
                        else if (tilt < -0.25f) col = 1;

                        sf::Sprite sprite(resources.getTexture("player"));
                        const int variant = static_cast<int>(entity.variant % 5);
                        sprite.setTextureRect(sf::IntRect({col * 33, variant * 17}, {33, 17}));
                        sprite.setScale({2.f, 2.f});
                        sprite.setPosition({entity.x, entity.y});
                        window.draw(sprite);

                        const int flameCycle = static_cast<int>(totalTime * 24.f) % 3;
                        sf::CircleShape thrusterGlow(flameCycle == 0 ? 3.5f : (flameCycle == 1 ? 5.2f : 4.2f));
                        thrusterGlow.setFillColor(flameCycle == 1 ? sf::Color(100, 220, 255, 230) : sf::Color(0, 160, 255, 175));
                        float thrusterY = entity.y + 13.f;
                        if (col == 4 || col == 3) thrusterY = entity.y + 15.f;
                        else if (col == 0 || col == 1) thrusterY = entity.y + 11.f;
                        thrusterGlow.setPosition({entity.x - 5.f, thrusterY});
                        window.draw(thrusterGlow);

                        // Authoritative player identification tag above ship
                        const std::uint8_t pId = entity.variant;
                        std::string tagStr = "P" + std::to_string(pId + 1);
                        if (localPlayerId.has_value() && localPlayerId.value() == pId)
                            tagStr += " (YOU)";
                        sf::Text tagText(resources.getFont("default"), tagStr, 15);
                        static const std::array<sf::Color, 4> playerTagColors = {
                            sf::Color(0, 230, 255),    // P1 Cyan
                            sf::Color(255, 170, 40),   // P2 Orange
                            sf::Color(80, 255, 120),   // P3 Green
                            sf::Color(240, 110, 255)   // P4 Magenta
                        };
                        tagText.setFillColor(playerTagColors[pId % 4]);
                        const auto tBounds = tagText.getLocalBounds();
                        tagText.setOrigin({tBounds.size.x / 2.f, tBounds.size.y});
                        tagText.setPosition({entity.x + 33.f, entity.y - 8.f});
                        window.draw(tagText);
                    } else if (entity.type == rtype::protocol::EntityType::PlayerMissile) {
                        sf::Sprite sprite(resources.getTexture("bullet"));
                        const int bulletFrame = static_cast<int>(totalTime * 18.f) % 2;
                        sprite.setTextureRect(bulletFrame == 0 ? sf::IntRect({249, 102}, {16, 12}) : sf::IntRect({266, 102}, {16, 12}));
                        sprite.setScale({2.f, 2.f});
                        sprite.setColor(sf::Color(120, 230, 255));
                        sprite.setPosition({entity.x, entity.y});
                        window.draw(sprite);
                    } else if (entity.type == rtype::protocol::EntityType::EnemyMissile) {
                        const float mTime = totalTime + static_cast<float>(entity.netId % 7) * 0.15f;
                        if (entity.variant == 0) {
                            sf::Sprite sprite(resources.getTexture("enemy_bullet"));
                            const int animFrame = static_cast<int>(mTime * 12.f) % 4;
                            sprite.setTextureRect(sf::IntRect({animFrame * 17, 1}, {16, 14}));
                            sprite.setOrigin({8.f, 7.f});
                            sprite.setScale({2.5f, 2.5f});
                            sprite.setPosition({entity.x, entity.y});
                            window.draw(sprite);
                        } else if (entity.variant == 1) {
                            sf::Sprite sprite(resources.getTexture("bullet"));
                            sprite.setTextureRect(sf::IntRect({232, 103}, {16, 12}));
                            sprite.setColor(sf::Color(255, 60, 60));
                            sprite.setOrigin({8.f, 6.f});
                            const float pulse = 1.0f + 0.15f * std::sin(mTime * 20.f);
                            sprite.setScale({2.6f * pulse, 2.2f});
                            sprite.setPosition({entity.x, entity.y});
                            window.draw(sprite);
                        } else if (entity.variant == 2) {
                            sf::Sprite sprite(resources.getTexture("bullet"));
                            sprite.setTextureRect(sf::IntRect({101, 3}, {32, 14}));
                            sprite.setColor(sf::Color(210, 90, 255));
                            sprite.setOrigin({16.f, 7.f});
                            const float waveScale = 1.8f + 0.35f * std::sin(mTime * 18.f);
                            sprite.setScale({2.0f, waveScale});
                            sprite.setPosition({entity.x, entity.y});
                            window.draw(sprite);
                        } else if (entity.variant == 3) {
                            sf::Sprite sprite(resources.getTexture("bullet"));
                            sprite.setTextureRect(sf::IntRect({205, 24}, {27, 18}));
                            sprite.setColor(sf::Color(80, 255, 80));
                            sprite.setOrigin({13.f, 9.f});
                            sprite.setRotation(sf::degrees(mTime * 360.f));
                            sprite.setScale({2.0f, 2.0f});
                            sprite.setPosition({entity.x, entity.y});
                            window.draw(sprite);
                        } else {
                            sf::Sprite sprite(resources.getTexture("bullet"));
                            sprite.setTextureRect(sf::IntRect({167, 22}, {32, 24}));
                            sprite.setColor(sf::Color(255, 130, 20));
                            sprite.setOrigin({16.f, 12.f});
                            const float pulse = 1.0f + 0.12f * std::sin(mTime * 15.f);
                            sprite.setScale({2.4f * pulse, 2.4f * pulse});
                            sprite.setPosition({entity.x, entity.y});
                            window.draw(sprite);
                        }
                    } else if (entity.type == rtype::protocol::EntityType::Obstacle) {
                        // Placeholder until the tiles have sprites: 0 debris, 1 Mars rock, 2 singularity shard.
                        static const std::array<sf::Color, 3> tileColors = {
                            sf::Color(120, 135, 150), sf::Color(165, 85, 50), sf::Color(120, 70, 190)};
                        sf::RectangleShape tile({rtype::ObstacleSize - 4.f, rtype::ObstacleSize - 4.f});
                        tile.setPosition({entity.x + 2.f, entity.y + 2.f});
                        tile.setFillColor(tileColors[entity.variant % tileColors.size()]);
                        tile.setOutlineColor(sf::Color(235, 235, 245));
                        tile.setOutlineThickness(2.f);
                        window.draw(tile);
                    } else if (entity.type == rtype::protocol::EntityType::Enemy) {
                        const float entityTime = totalTime + static_cast<float>(entity.netId % 11) * 0.23f;
                        const int animFrame = static_cast<int>(entityTime * 9.f);
                        if (entity.variant == 0) {
                            const auto &tex = resources.hasTexture("enemy_standard") ? resources.getTexture("enemy_standard") : resources.getTexture("enemy");
                            sf::Sprite sprite(tex);
                            const int frame = animFrame % 8;
                            sprite.setTextureRect(sf::IntRect({frame * 33, 0}, {33, 36}));
                            sprite.setScale({2.0f, 2.0f});
                            sprite.setPosition({entity.x, entity.y});
                            window.draw(sprite);
                        } else if (entity.variant == 1) {
                            const auto &tex = resources.hasTexture("enemy_patrol") ? resources.getTexture("enemy_patrol") : resources.getTexture("enemy");
                            sf::Sprite sprite(tex);
                            // Scout ship: Frame 0 faces left towards players (Frame 1 in r-typesheet27 is 180° flipped facing backwards)
                            sprite.setTextureRect(sf::IntRect({1, 2}, {32, 28}));
                            sprite.setScale({2.0f, 2.0f});
                            sprite.setPosition({entity.x, entity.y});
                            window.draw(sprite);

                            // Active engine glow behind the scout ship
                            const float enginePulse = 1.0f + 0.2f * std::sin(entityTime * 18.f);
                            sf::CircleShape glow(3.5f * enginePulse);
                            glow.setOrigin({3.5f, 3.5f});
                            glow.setFillColor(sf::Color(255, 130, 40, 210));
                            glow.setPosition({entity.x + 58.f, entity.y + 26.f});
                            window.draw(glow);
                        } else if (entity.variant == 2) {
                            const auto &tex = resources.hasTexture("enemy_orbital_probe") ? resources.getTexture("enemy_orbital_probe") : resources.getTexture("enemy");
                            sf::Sprite sprite(tex);
                            // Probe scans forward towards the player without turning backwards or snapping
                            static const std::array<sf::IntRect, 4> probeFrames = {
                                sf::IntRect({1, 2}, {16, 32}),
                                sf::IntRect({19, 2}, {15, 32}),
                                sf::IntRect({36, 2}, {15, 32}),
                                sf::IntRect({19, 2}, {15, 32})
                            };
                            const int frame = (animFrame / 2) % 4;
                            sprite.setTextureRect(probeFrames[frame]);
                            sprite.setScale({2.2f, 2.2f});
                            sprite.setPosition({entity.x, entity.y});
                            window.draw(sprite);
                        } else if (entity.variant == 3) {
                            const auto &tex = resources.hasTexture("enemy_solar_relay") ? resources.getTexture("enemy_solar_relay") : (resources.hasTexture("enemy_diver") ? resources.getTexture("enemy_diver") : resources.getTexture("enemy"));
                            sf::Sprite sprite(tex);
                            // Rotating orbital solar relay satellite: full 16-frame 360° rotation across Row 0 (0-7) and Row 1 (8-15)
                            const int frame = (animFrame / 2) % 16;
                            const int col = frame % 8;
                            const int rowY = (frame < 8) ? 2 : 36;
                            sprite.setTextureRect(sf::IntRect({col * 33 + 1, rowY}, {32, 32}));
                            sprite.setScale({2.2f, 2.2f});
                            sprite.setPosition({entity.x, entity.y});
                            window.draw(sprite);
                        } else if (entity.variant == 4) {
                            const auto &tex = resources.hasTexture("enemy_interceptor") ? resources.getTexture("enemy_interceptor") : resources.getTexture("enemy");
                            sf::Sprite sprite(tex);
                            const int frame = (animFrame / 2) % 5;
                            sprite.setTextureRect(sf::IntRect({frame * 65 + 9, 9}, {48, 48}));
                            sprite.setScale({1.8f, 1.8f});
                            sprite.setPosition({entity.x, entity.y});
                            window.draw(sprite);
                        } else if (entity.variant == 7) {
                            const auto &tex = resources.hasTexture("enemy_tentacle") ? resources.getTexture("enemy_tentacle") : resources.getTexture("enemy");
                            sf::Sprite sprite(tex);
                            // Surface jumping Martian alien on Mars (ping-pong 4 frames facing forward: 0, 1, 2, 1)
                            static const std::array<sf::IntRect, 4> tentacleFrames = {
                                sf::IntRect({2, 1}, {47, 56}),
                                sf::IntRect({52, 1}, {56, 56}),
                                sf::IntRect({116, 1}, {49, 56}),
                                sf::IntRect({52, 1}, {56, 56})
                            };
                            const int frame = (animFrame / 2) % 4;
                            sprite.setTextureRect(tentacleFrames[frame]);
                            sprite.setScale({1.8f, 1.8f});
                            sprite.setPosition({entity.x, entity.y});
                            window.draw(sprite);
                        } else if (entity.variant == 5) {
                            const auto &tex = resources.hasTexture("enemy_boss_heavy") ? resources.getTexture("enemy_boss_heavy") : resources.getTexture("enemy");
                            sf::Sprite sprite(tex);
                            // Heavy War Cruiser Boss with frontal cannon and top/bottom missile silos (130x50, r-typesheet28)
                            sprite.setTextureRect(sf::IntRect({0, 0}, {130, 50}));
                            sprite.setScale({2.2f, 2.2f});
                            sprite.setPosition({entity.x, entity.y - 20.f});
                            window.draw(sprite);
                        } else {
                            const auto &tex = resources.hasTexture("enemy_boss") ? resources.getTexture("enemy_boss") : resources.getTexture("enemy");
                            sf::Sprite sprite(tex);
                            static const std::array<sf::IntRect, 2> bossFrames = {
                                sf::IntRect({2, 2}, {176, 144}),
                                sf::IntRect({191, 2}, {176, 144})
                            };
                            const int frame = (animFrame / 6) % 2;
                            sprite.setTextureRect(bossFrames[frame]);
                            sprite.setScale({1.6f, 1.6f});
                            sprite.setPosition({entity.x, entity.y - 60.f});
                            window.draw(sprite);
                        }
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

            // Authoritative Score HUD (Top Left)
            scoreText.setString("SCORE: " + std::to_string(lastScore));
            scoreText.setPosition({45.f, 25.f});
            window.draw(scoreText);

            // Stage & Wave Progression (Top Left, under score)
            sf::Text stageText(resources.getFont("default"), "", 20);
            stageText.setFillColor(sf::Color(255, 204, 0));
            stageText.setString("STAGE 1 - WAVE " + std::to_string(currentWave));
            stageText.setPosition({45.f, 62.f});
            window.draw(stageText);

            // Boss Alert banner if boss is present
            bool bossPresent = false;
            for (const auto &ent : entities) {
                if (ent.type == rtype::protocol::EntityType::Enemy && ent.variant == 5) {
                    bossPresent = true;
                    break;
                }
            }
            if (bossPresent) {
                sf::RectangleShape bossBg({360.f, 24.f});
                bossBg.setOrigin({180.f, 12.f});
                bossBg.setPosition({rtype::WorldWidth / 2.f, 45.f});
                bossBg.setFillColor(sf::Color(20, 10, 15, 220));
                bossBg.setOutlineThickness(2.f);
                bossBg.setOutlineColor(sf::Color(255, 50, 70));
                window.draw(bossBg);

                sf::Text bossLabel(resources.getFont("default"), "WARNING - BOSS ENCOUNTER", 16);
                bossLabel.setFillColor(sf::Color(255, 70, 70));
                const auto bBounds = bossLabel.getLocalBounds();
                bossLabel.setOrigin({bBounds.size.x / 2.f, bBounds.size.y / 2.f});
                bossLabel.setPosition({rtype::WorldWidth / 2.f, 43.f});
                window.draw(bossLabel);
            }

            // Player Identification HUD (Top Right)
            {
                float rightOffset = rtype::WorldWidth - 45.f;
                float startY = 25.f;
                static const std::array<sf::Color, 4> pColors = {
                    sf::Color(0, 230, 255),
                    sf::Color(255, 170, 40),
                    sf::Color(80, 255, 120),
                    sf::Color(240, 110, 255)
                };

                std::array<bool, 4> playerActive{false, false, false, false};
                for (const auto &ent : entities) {
                    if (ent.type == rtype::protocol::EntityType::Player && ent.variant < 4) {
                        playerActive[ent.variant] = true;
                    }
                }

                for (std::size_t i = 0; i < 4; ++i) {
                    const bool isLocal = (localPlayerId.has_value() && localPlayerId.value() == i);
                    if (!playerActive[i] && !isLocal)
                        continue;

                    std::string pLabel = "P" + std::to_string(i + 1);
                    if (isLocal)
                        pLabel += " (YOU)";

                    std::string statusStr = playerActive[i] ? "ACTIVE" : "DOWN";

                    sf::Text pText(resources.getFont("default"), pLabel + " : " + statusStr, 20);
                    pText.setFillColor(playerActive[i] ? pColors[i] : sf::Color(140, 140, 140));
                    const auto bounds = pText.getLocalBounds();
                    pText.setOrigin({bounds.size.x, 0.f});
                    pText.setPosition({rightOffset, startY});
                    window.draw(pText);
                    startY += 26.f;
                }
            }

            if (damageFlashTimer > 0.f) {
                sf::RectangleShape flash({rtype::WorldWidth, rtype::WorldHeight});
                const float alphaRatio = std::clamp(damageFlashTimer / DamageFlashDuration, 0.f, 1.f);
                flash.setFillColor(sf::Color(255, 0, 0, static_cast<std::uint8_t>(alphaRatio * 135.f)));
                window.draw(flash);
            }

            if (waveDisplayTimer > 0.f) {
                const float progress = waveDisplayTimer / WaveDisplayDuration;
                float alpha = 1.f;
                if (progress > 0.8f)
                    alpha = (1.f - progress) / 0.2f;
                else if (progress < 0.25f)
                    alpha = progress / 0.25f;
                alpha = std::clamp(alpha, 0.f, 1.f);

                sf::RectangleShape banner({640.f, 100.f});
                banner.setOrigin({320.f, 50.f});
                banner.setPosition({rtype::WorldWidth / 2.f, 250.f});
                banner.setFillColor(sf::Color(10, 15, 30, static_cast<std::uint8_t>(alpha * 220.f)));
                banner.setOutlineThickness(2.f);
                banner.setOutlineColor(sf::Color(255, 204, 0, static_cast<std::uint8_t>(alpha * 255.f)));
                window.draw(banner);

                sf::Text waveText(resources.getFont("default"), "--- WAVE " + std::to_string(currentWave) + " ---", 44);
                waveText.setFillColor(sf::Color(255, 204, 0, static_cast<std::uint8_t>(alpha * 255.f)));
                const auto wBounds = waveText.getLocalBounds();
                waveText.setOrigin({wBounds.size.x / 2.f, wBounds.size.y / 2.f});
                waveText.setPosition({rtype::WorldWidth / 2.f, 235.f});
                window.draw(waveText);

                sf::Text subWaveText(resources.getFont("default"), "INCOMING ENEMY SQUADRON", 18);
                subWaveText.setFillColor(sf::Color(0, 230, 255, static_cast<std::uint8_t>(alpha * 240.f)));
                const auto swBounds = subWaveText.getLocalBounds();
                subWaveText.setOrigin({swBounds.size.x / 2.f, swBounds.size.y / 2.f});
                subWaveText.setPosition({rtype::WorldWidth / 2.f, 275.f});
                window.draw(subWaveText);
            }

            if (gameOver) {
                sf::RectangleShape endOverlay({rtype::WorldWidth, rtype::WorldHeight});
                endOverlay.setFillColor(sf::Color(5, 5, 15, 205));
                window.draw(endOverlay);

                sf::Text endTitle(resources.getFont("default"), gameWon ? "MISSION ACCOMPLISHED" : "GAME OVER", 64);
                endTitle.setFillColor(gameWon ? sf::Color(0, 230, 255) : sf::Color(255, 50, 50));
                const auto tBounds = endTitle.getLocalBounds();
                endTitle.setOrigin({tBounds.size.x / 2.f, tBounds.size.y / 2.f});
                endTitle.setPosition({rtype::WorldWidth / 2.f, 430.f});
                window.draw(endTitle);

                sf::Text finalScoreText(resources.getFont("default"), "FINAL SCORE: " + std::to_string(lastScore), 32);
                finalScoreText.setFillColor(sf::Color(255, 204, 0));
                const auto fsBounds = finalScoreText.getLocalBounds();
                finalScoreText.setOrigin({fsBounds.size.x / 2.f, fsBounds.size.y / 2.f});
                finalScoreText.setPosition({rtype::WorldWidth / 2.f, 520.f});
                window.draw(finalScoreText);

                sf::Text returnText(resources.getFont("default"), "Press [ESC] to Return to Menu", 22);
                returnText.setFillColor(sf::Color(170, 185, 215));
                const auto rBounds = returnText.getLocalBounds();
                returnText.setOrigin({rBounds.size.x / 2.f, rBounds.size.y / 2.f});
                returnText.setPosition({rtype::WorldWidth / 2.f, 600.f});
                window.draw(returnText);
            }
        }

        window.display();
    }

    if (network.connected())
        network.disconnect();

    menu.getConfig().saveToFile("config.json");

    return EXIT_SUCCESS;
}
