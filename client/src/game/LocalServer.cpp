#include "game/LocalServer.hpp"

namespace rtype::client {

void LocalServer::start(const server::ServerSettings &settings)
{
    stop();
    _settings = settings;
    _game = std::make_unique<server::Game>();
    server::configureGame(*_game, _settings);
    _network = std::make_unique<server::AsioNetworkServer>();
    _network->start(_settings.port);
    _running = true;
    _thread = std::thread([this] { server::runServer(*_game, *_network, _settings, _running); });
}

void LocalServer::stop()
{
    _running = false;
    if (_thread.joinable())
        _thread.join();
    if (_network)
        _network->stop();
    _network.reset();
    _game.reset();
}

} // namespace rtype::client
