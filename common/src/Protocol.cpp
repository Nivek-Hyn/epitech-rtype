#include "common/Protocol.hpp"

#include "engine/net/ByteBuffer.hpp"

#include <stdexcept>
#include <type_traits>

namespace rtype::protocol {

using engine::net::ByteReader;
using engine::net::ByteWriter;

namespace {

template <typename T> constexpr bool always_false = false;

// Maps each message struct to its MessageType.
template <typename T> constexpr MessageType typeOf()
{
    if constexpr (std::is_same_v<T, Connect>)
        return MessageType::Connect;
    else if constexpr (std::is_same_v<T, Input>)
        return MessageType::Input;
    else if constexpr (std::is_same_v<T, Disconnect>)
        return MessageType::Disconnect;
    else if constexpr (std::is_same_v<T, Ping>)
        return MessageType::Ping;
    else if constexpr (std::is_same_v<T, ConnectAccepted>)
        return MessageType::ConnectAccepted;
    else if constexpr (std::is_same_v<T, ConnectRejected>)
        return MessageType::ConnectRejected;
    else if constexpr (std::is_same_v<T, Snapshot>)
        return MessageType::Snapshot;
    else if constexpr (std::is_same_v<T, EntityDestroyed>)
        return MessageType::EntityDestroyed;
    else if constexpr (std::is_same_v<T, PlayerLeft>)
        return MessageType::PlayerLeft;
    else if constexpr (std::is_same_v<T, GameOver>)
        return MessageType::GameOver;
    else if constexpr (std::is_same_v<T, Pong>)
        return MessageType::Pong;
    else
        static_assert(always_false<T>, "message type missing in typeOf()");
}

void writePayload(ByteWriter &, const Connect &) {}
void writePayload(ByteWriter &w, const Input &m)
{
    w.u8(m.flags);
}
void writePayload(ByteWriter &, const Disconnect &) {}
void writePayload(ByteWriter &w, const Ping &m)
{
    w.u32(m.timestampMs);
}
void writePayload(ByteWriter &w, const ConnectAccepted &m)
{
    w.u8(m.playerId);
}
void writePayload(ByteWriter &w, const ConnectRejected &m)
{
    w.u8(static_cast<std::uint8_t>(m.reason));
}
void writePayload(ByteWriter &w, const EntityDestroyed &m)
{
    w.u32(m.netId);
    w.u8(static_cast<std::uint8_t>(m.reason));
}
void writePayload(ByteWriter &w, const PlayerLeft &m)
{
    w.u8(m.playerId);
    w.u8(static_cast<std::uint8_t>(m.reason));
}
void writePayload(ByteWriter &w, const GameOver &m)
{
    w.u32(m.score);
}
void writePayload(ByteWriter &w, const Pong &m)
{
    w.u32(m.timestampMs);
}

void writePayload(ByteWriter &w, const Snapshot &m)
{
    if (m.entities.size() > MaxEntitiesPerSnapshot)
        throw std::length_error("Snapshot too big for one packet: split it (see MaxEntitiesPerSnapshot)");
    w.u32(m.tick);
    w.u16(static_cast<std::uint16_t>(m.entities.size()));
    for (const auto &e : m.entities) {
        w.u32(e.netId);
        w.u8(static_cast<std::uint8_t>(e.type));
        w.u8(e.variant);
        w.f32(e.x);
        w.f32(e.y);
    }
}

// ---- decoding: each function returns nullopt if a field value is invalid ----

bool validEntityType(std::uint8_t v)
{
    return v >= 1 && v <= 4;
}
bool validReject(std::uint8_t v)
{
    return v >= 1 && v <= 2;
}
bool validDestroy(std::uint8_t v)
{
    return v >= 1 && v <= 2;
}
bool validLeave(std::uint8_t v)
{
    return v >= 1 && v <= 2;
}

std::optional<Message> readPayload(MessageType type, ByteReader &r)
{
    switch (type) {
    case MessageType::Connect:
        return Connect{};
    case MessageType::Input:
        return Input{r.u8()};
    case MessageType::Disconnect:
        return Disconnect{};
    case MessageType::Ping:
        return Ping{r.u32()};
    case MessageType::ConnectAccepted: {
        const auto id = r.u8();
        if (id >= 4)
            return std::nullopt;
        return ConnectAccepted{id};
    }
    case MessageType::ConnectRejected: {
        const auto reason = r.u8();
        if (!validReject(reason))
            return std::nullopt;
        return ConnectRejected{static_cast<RejectReason>(reason)};
    }
    case MessageType::Snapshot: {
        Snapshot snap;
        snap.tick = r.u32();
        const auto count = r.u16();
        if (count > MaxEntitiesPerSnapshot || r.remaining() != count * EntityStateSize)
            return std::nullopt;
        snap.entities.reserve(count);
        for (std::uint16_t i = 0; i < count; ++i) {
            EntityState e;
            e.netId = r.u32();
            const auto entityType = r.u8();
            if (!validEntityType(entityType))
                return std::nullopt;
            e.type = static_cast<EntityType>(entityType);
            e.variant = r.u8();
            e.x = r.f32();
            e.y = r.f32();
            snap.entities.push_back(e);
        }
        return snap;
    }
    case MessageType::EntityDestroyed: {
        const auto netId = r.u32();
        const auto reason = r.u8();
        if (!validDestroy(reason))
            return std::nullopt;
        return EntityDestroyed{netId, static_cast<DestroyReason>(reason)};
    }
    case MessageType::PlayerLeft: {
        const auto id = r.u8();
        const auto reason = r.u8();
        if (id >= 4 || !validLeave(reason))
            return std::nullopt;
        return PlayerLeft{id, static_cast<LeaveReason>(reason)};
    }
    case MessageType::GameOver:
        return GameOver{r.u32()};
    case MessageType::Pong:
        return Pong{r.u32()};
    }
    return std::nullopt; // unknown type
}

} // namespace

std::vector<std::uint8_t> encode(const Packet &packet)
{
    ByteWriter payload;
    std::visit([&](const auto &m) { writePayload(payload, m); }, packet.message);
    const auto type = std::visit([](const auto &m) { return typeOf<std::decay_t<decltype(m)>>(); }, packet.message);

    ByteWriter out;
    out.u16(Magic);
    out.u8(Version);
    out.u8(static_cast<std::uint8_t>(type));
    out.u32(packet.sequence);
    out.u16(static_cast<std::uint16_t>(payload.size()));
    out.data().insert(out.data().end(), payload.data().begin(), payload.data().end());
    return std::move(out.data());
}

std::optional<Packet> decode(std::span<const std::uint8_t> datagram)
{
    if (datagram.size() < HeaderSize || datagram.size() > MaxPacketSize)
        return std::nullopt;

    ByteReader header(datagram.first(HeaderSize));
    const auto magic = header.u16();
    const auto version = header.u8();
    const auto type = static_cast<MessageType>(header.u8());
    const auto sequence = header.u32();
    const auto payloadSize = header.u16();

    if (magic != Magic || version != Version || payloadSize != datagram.size() - HeaderSize)
        return std::nullopt;

    ByteReader payload(datagram.subspan(HeaderSize));
    auto message = readPayload(type, payload);
    // The payload must be exactly consumed: no missing bytes, no trailing garbage.
    if (!message || !payload.ok() || payload.remaining() != 0)
        return std::nullopt;
    return Packet{sequence, std::move(*message)};
}

} // namespace rtype::protocol
