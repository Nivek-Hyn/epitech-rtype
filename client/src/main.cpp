#include "common/Constants.hpp"
#include "net/AsioNetworkClient.hpp"

#include <SFML/Graphics.hpp>

#include <array>
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

    rtype::client::AsioNetworkClient network;
    try {
        network.connect(host, port);
    } catch (const std::exception &error) {
        std::cerr << "Could not connect to server " << host << ':' << port << ": " << error.what() << '\n';
        return EXIT_FAILURE;
    }
    std::cout << "Connecting to " << host << ':' << port << " over UDP...\n";

    std::vector<rtype::protocol::EntityState> entities;
    const std::array<sf::Color, rtype::MaxPlayers> playerColors = {
        sf::Color(70, 205, 255), sf::Color(255, 195, 65), sf::Color(105, 230, 135), sf::Color(255, 100, 125)};
    bool reportedConnected = false;

    while (window.isOpen()) {
        while (const std::optional event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>())
                window.close();
        }

        if (network.connected() && !reportedConnected) {
            std::cout << "Connected to server. Use arrow keys to move and Space to shoot.\n";
            reportedConnected = true;
        }

        std::uint8_t inputFlags = 0;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Up))
            inputFlags |= rtype::protocol::InputFlag::Up;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Down))
            inputFlags |= rtype::protocol::InputFlag::Down;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left))
            inputFlags |= rtype::protocol::InputFlag::Left;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Right))
            inputFlags |= rtype::protocol::InputFlag::Right;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Space))
            inputFlags |= rtype::protocol::InputFlag::Shoot;
        if (network.connected())
            network.send(rtype::protocol::Input{inputFlags});

        while (auto message = network.poll()) {
            if (const auto *snapshot = std::get_if<rtype::protocol::Snapshot>(&*message))
                entities = snapshot->entities;
            else if (const auto *rejected = std::get_if<rtype::protocol::ConnectRejected>(&*message)) {
                const char *reason = rejected->reason == rtype::protocol::RejectReason::ServerFull
                                         ? "server is full"
                                         : "protocol version mismatch";
                std::cerr << "Connection rejected: " << reason << ".\n";
            }
        }

        window.clear(sf::Color::Black);
        for (const auto &entity : entities) {
            sf::RectangleShape sprite;
            if (entity.type == rtype::protocol::EntityType::Player) {
                sprite.setSize({42.f, 28.f});
                sprite.setFillColor(playerColors[entity.variant % playerColors.size()]);
            } else {
                sprite.setSize({18.f, 8.f});
                sprite.setFillColor(sf::Color(255, 235, 120));
            }
            sprite.setPosition({entity.x, entity.y});
            window.draw(sprite);
        }
        window.display();
    }
    network.disconnect();
    return EXIT_SUCCESS;
}
