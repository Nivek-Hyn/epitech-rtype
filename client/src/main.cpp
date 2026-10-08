#include "common/Constants.hpp"

#include <SFML/Graphics.hpp>

#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>

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

    // TODO(network): create the Asio implementation of INetworkClient and connect(host, port).
    std::cout << "r-type_client: server " << host << ':' << port << " (network not implemented yet)\n";

    while (window.isOpen()) {
        while (const std::optional event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>())
                window.close();
        }
        // TODO(client): inputs -> network.send(Input), network.poll() -> local ECS, render systems.
        window.clear(sf::Color::Black);
        window.display();
    }
    return EXIT_SUCCESS;
}
