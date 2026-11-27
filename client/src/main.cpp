#include "App.hpp"
#include "ResourceManager.hpp"
#include "common/Constants.hpp"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <optional>
#include <string>

namespace {

struct Arguments {
    std::string host = "127.0.0.1";
    std::uint16_t port = rtype::DefaultPort;
    bool directConnect = false; // a host was given: skip the menu
};

std::optional<Arguments> parseArguments(int argc, char **argv)
{
    Arguments arguments;
    try {
        if (argc > 1) {
            arguments.host = argv[1];
            arguments.directConnect = true;
        }
        if (argc > 2) {
            const unsigned long port = std::stoul(argv[2]);
            if (port == 0 || port > 65535)
                return std::nullopt;
            arguments.port = static_cast<std::uint16_t>(port);
        }
    } catch (const std::exception &) {
        return std::nullopt;
    }
    return arguments;
}

} // namespace

int main(int argc, char **argv)
{
    const std::optional<Arguments> arguments = parseArguments(argc, argv);
    if (!arguments) {
        std::cerr << "Usage: " << argv[0] << " [host] [port]\n";
        return 84;
    }

    rtype::client::ResourceManager resources;
    const bool hasAssets = resources.loadDefaults();
    if (!hasAssets)
        std::cerr << "Warning: Could not load some assets. Make sure to run from repository root.\n";
    if (!resources.hasFont("default")) {
        std::cerr << "Error: Default font could not be loaded.\n";
        return 84;
    }

    rtype::client::App app(resources, hasAssets);
    if (arguments->directConnect) {
        try {
            app.connect(arguments->host, arguments->port);
        } catch (const std::exception &e) {
            std::cerr << "Direct connect failed: " << e.what() << '\n';
        }
    }
    app.run();
    return EXIT_SUCCESS;
}
