#include "common/Protocol.hpp"

#include <stdexcept>

namespace rtype::protocol {

// TODO(network): write the 10-byte header then the payload of packet.message, as described in docs/protocol.md.
// Use engine::net::ByteWriter. Expected behavior is specified by tests/test_protocol.cpp.
std::vector<std::uint8_t> encode([[maybe_unused]] const Packet &packet)
{
    throw std::logic_error("protocol::encode not implemented");
}

// TODO(network): parse a datagram, return nullopt for ANY invalid input (see "Validation" in docs/protocol.md).
// Must never crash, throw or read out of bounds, whatever the bytes received.
std::optional<Packet> decode([[maybe_unused]] std::span<const std::uint8_t> datagram)
{
    throw std::logic_error("protocol::decode not implemented");
}

} // namespace rtype::protocol
