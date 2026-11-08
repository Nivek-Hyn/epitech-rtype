#pragma once

#include "Game.hpp"
#include "ServerLoop.hpp"
#include "net/AsioNetworkServer.hpp"

#include <atomic>
#include <memory>
#include <thread>

namespace rtype::client {

// A server hosted by the client itself, for solo games and local hosting (a "listen server").
// It runs the same game and loop as r-type_server (server/src/ServerLoop.hpp), on its own thread.
class LocalServer {
public:
    ~LocalServer() { stop(); }

    // Throws when the settings are invalid (e.g. a broken level) or the port cannot be opened,
    // so the menu can say why instead of connecting to nothing.
    void start(const server::ServerSettings &settings);
    void stop();

private:
    std::unique_ptr<server::Game> _game;
    std::unique_ptr<server::AsioNetworkServer> _network;
    server::ServerSettings _settings;
    std::atomic<bool> _running{false};
    std::thread _thread;
};

} // namespace rtype::client
