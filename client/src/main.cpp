#include "ResourceManager.hpp"
#include "common/Constants.hpp"
#include "common/Protocol.hpp"
#include "common/SnapshotAssembler.hpp"
#include "net/AsioNetworkClient.hpp"

#include <SFML/Graphics.hpp>

#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>
#include <vector>

int main(int argc, char **argv)
{
    std::string host = "127.0.0.1";
    std::uint16_t port = rtype::DefaultPort;
    try {
        if (argc > 1)
            host = argv[1];
        if (argc > 2)
            port = static_cast<std::uint16_t>(std::stoul(argv[2]));
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

    rtype::client::AsioNetworkClient network;
    try {
        network.connect(host, port);
    } catch (const std::exception &error) {
        std::cerr << "Could not connect to server " << host << ':' << port << ": " << error.what() << '\n';
        return EXIT_FAILURE;
    }
    std::cout << "Connecting to " << host << ':' << port << " over UDP...\n";
    std::cout << "Controls: [ZQSD / Arrows] to Move, [Space] to Shoot.\n";

    rtype::protocol::SnapshotAssembler world;
    bool reportedConnected = false;

    while (window.isOpen()) {
        while (const std::optional<sf::Event> event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>())
                window.close();
        }

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
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Space))
            inputFlags |= rtype::protocol::InputFlag::Shoot;

        if (network.connected())
            network.send(rtype::protocol::Input{inputFlags});

        while (auto message = network.poll()) {
            if (const auto *snapshot = std::get_if<rtype::protocol::Snapshot>(&*message))
                world.add(*snapshot);
            else if (std::holds_alternative<rtype::protocol::ConnectAccepted>(*message))
                world.reset(); // a new session: the server's ticks may start again from zero
            else if (const auto *rejected = std::get_if<rtype::protocol::ConnectRejected>(&*message)) {
                const char *reason = rejected->reason == rtype::protocol::RejectReason::ServerFull
                                         ? "server is full"
                                         : "protocol version mismatch";
                std::cerr << "Connection rejected: " << reason << ".\n";
            }
        }

        window.clear(sf::Color(10, 10, 25));

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
        window.display();
    }
    network.disconnect();
    return EXIT_SUCCESS;
}
