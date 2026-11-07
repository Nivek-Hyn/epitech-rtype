#include "common/Protocol.hpp"

#include "engine/net/ByteBuffer.hpp"

#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace rtype::protocol {

namespace {

MessageType typeOf(const Message &message)
{
    return std::visit(
        []<typename T>(const T &) {
            if constexpr (std::is_same_v<T, Connect>)
                return MessageType::Connect;
            else if constexpr (std::is_same_v<T, Input>)
                return MessageType::Input;
            else if constexpr (std::is_same_v<T, Disconnect>)
                return MessageType::Disconnect;
            else if constexpr (std::is_same_v<T, Ping>)
                return MessageType::Ping;
            else if constexpr (std::is_same_v<T, Ack>)
                return MessageType::Ack;
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
            else
                return MessageType::Pong;
        },
        message);
}

void writePayload(engine::net::ByteWriter &writer, const Message &message)
{
    std::visit(
        [&writer]<typename T>(const T &value) {
            if constexpr (std::is_same_v<T, Input>) {
                writer.u8(value.flags);
            } else if constexpr (std::is_same_v<T, Ping> || std::is_same_v<T, Pong>) {
                writer.u32(value.timestampMs);
            } else if constexpr (std::is_same_v<T, Ack>) {
                writer.u32(value.reliableId);
            } else if constexpr (std::is_same_v<T, ConnectAccepted>) {
                writer.u8(value.playerId);
            } else if constexpr (std::is_same_v<T, ConnectRejected>) {
                writer.u8(static_cast<std::uint8_t>(value.reason));
            } else if constexpr (std::is_same_v<T, Snapshot>) {
                if (value.entities.size() > MaxEntitiesPerSnapshot)
                    throw std::invalid_argument("snapshot exceeds maximum packet size");
                writer.u32(value.tick);
                writer.u8(static_cast<std::uint8_t>(value.state));
                writer.u32(value.score);
                for (const std::uint8_t lives : value.lives)
                    writer.u8(lives);
                writer.u16(static_cast<std::uint16_t>(value.entities.size()));
                for (const EntityState &entity : value.entities) {
                    writer.u32(entity.netId);
                    writer.u8(static_cast<std::uint8_t>(entity.type));
                    writer.u8(entity.variant);
                    writer.f32(entity.x);
                    writer.f32(entity.y);
                }
            } else if constexpr (std::is_same_v<T, EntityDestroyed>) {
                writer.u32(value.netId);
                writer.u8(static_cast<std::uint8_t>(value.reason));
            } else if constexpr (std::is_same_v<T, PlayerLeft>) {
                writer.u8(value.playerId);
                writer.u8(static_cast<std::uint8_t>(value.reason));
            }
        },
        message);
}

bool validEntityType(std::uint8_t value)
{
    return value >= static_cast<std::uint8_t>(EntityType::Player) &&
           value <= static_cast<std::uint8_t>(EntityType::Bonus);
}

} // namespace

std::vector<std::uint8_t> encode(const Packet &packet, std::uint8_t version)
{
    engine::net::ByteWriter payload;
    if (packet.reliableId)
        payload.u32(*packet.reliableId);
    writePayload(payload, packet.message);

    engine::net::ByteWriter writer;
    writer.u16(Magic);
    writer.u8(version);
    writer.u8(static_cast<std::uint8_t>(typeOf(packet.message)) | (packet.reliableId ? ReliableFlag : 0));
    writer.u32(packet.sequence);
    writer.u16(static_cast<std::uint16_t>(payload.size()));
    writer.data().insert(writer.data().end(), payload.data().begin(), payload.data().end());
    return writer.data();
}

std::vector<Snapshot> splitSnapshot(const Snapshot &snapshot)
{
    std::vector<Snapshot> parts;
    std::size_t offset = 0;
    do {
        const std::size_t count = std::min(MaxEntitiesPerSnapshot, snapshot.entities.size() - offset);
        const auto first = snapshot.entities.begin() + static_cast<std::ptrdiff_t>(offset);
        parts.push_back({snapshot.tick, snapshot.state, snapshot.score,
                         std::vector<EntityState>(first, first + static_cast<std::ptrdiff_t>(count)), snapshot.lives});
        offset += count;
    } while (offset < snapshot.entities.size());
    return parts;
}

std::optional<std::uint8_t> otherVersionConnect(std::span<const std::uint8_t> datagram)
{
    if (datagram.size() < HeaderSize || datagram.size() > MaxPacketSize)
        return std::nullopt;
    engine::net::ByteReader header(datagram.first(HeaderSize));
    const auto magic = header.u16();
    const auto version = header.u8();
    const auto type = header.u8();
    if (magic != Magic || version == Version || type != static_cast<std::uint8_t>(MessageType::Connect))
        return std::nullopt;
    return version;
}

std::optional<Packet> decode(std::span<const std::uint8_t> datagram)
{
    if (datagram.size() < HeaderSize || datagram.size() > MaxPacketSize)
        return std::nullopt;

    try {
        engine::net::ByteReader header(datagram.first(HeaderSize));
        if (header.u16() != Magic || header.u8() != Version)
            return std::nullopt;
        const auto typeByte = header.u8();
        const auto sequence = header.u32();
        const auto payloadSize = header.u16();
        if (!header.ok() || payloadSize != datagram.size() - HeaderSize)
            return std::nullopt;

        // A reliable message starts with its number; the rest is the usual payload of the message type.
        const bool reliable = (typeByte & ReliableFlag) != 0;
        const auto rawType = static_cast<std::uint8_t>(typeByte & ~ReliableFlag);
        engine::net::ByteReader reader(datagram.subspan(HeaderSize));
        std::optional<std::uint32_t> reliableId;
        std::size_t bodySize = payloadSize;
        if (reliable) {
            if (payloadSize < 4 || static_cast<MessageType>(rawType) == MessageType::Ack)
                return std::nullopt; // an acknowledgement is never itself acknowledged
            reliableId = reader.u32();
            bodySize -= 4;
        }

        Message message;
        switch (static_cast<MessageType>(rawType)) {
        case MessageType::Connect:
            if (bodySize != 0)
                return std::nullopt;
            message = Connect{};
            break;
        case MessageType::Input: {
            if (bodySize != 1)
                return std::nullopt;
            const auto flags = reader.u8();
            if ((flags & 0xE0) != 0)
                return std::nullopt;
            message = Input{flags};
            break;
        }
        case MessageType::Disconnect:
            if (bodySize != 0)
                return std::nullopt;
            message = Disconnect{};
            break;
        case MessageType::Ping:
            if (bodySize != 4)
                return std::nullopt;
            message = Ping{reader.u32()};
            break;
        case MessageType::Ack:
            if (bodySize != 4)
                return std::nullopt;
            message = Ack{reader.u32()};
            break;
        case MessageType::ConnectAccepted: {
            if (bodySize != 1)
                return std::nullopt;
            const auto playerId = reader.u8();
            if (playerId >= MaxPlayers)
                return std::nullopt;
            message = ConnectAccepted{playerId};
            break;
        }
        case MessageType::ConnectRejected: {
            if (bodySize != 1)
                return std::nullopt;
            const auto reason = reader.u8();
            if (reason < static_cast<std::uint8_t>(RejectReason::ServerFull) ||
                reason > static_cast<std::uint8_t>(RejectReason::VersionMismatch))
                return std::nullopt;
            message = ConnectRejected{static_cast<RejectReason>(reason)};
            break;
        }
        case MessageType::Snapshot: {
            if (bodySize < SnapshotFixedSize)
                return std::nullopt;
            Snapshot snapshot;
            snapshot.tick = reader.u32();
            const auto state = reader.u8();
            if (state < static_cast<std::uint8_t>(GameState::Waiting) ||
                state > static_cast<std::uint8_t>(GameState::GameOver))
                return std::nullopt;
            snapshot.state = static_cast<GameState>(state);
            snapshot.score = reader.u32();
            for (std::uint8_t &lives : snapshot.lives)
                lives = reader.u8();
            const auto count = reader.u16();
            if (count > MaxEntitiesPerSnapshot || bodySize != SnapshotFixedSize + count * EntityStateSize)
                return std::nullopt;
            snapshot.entities.reserve(count);
            for (std::uint16_t i = 0; i < count; ++i) {
                EntityState entity;
                entity.netId = reader.u32();
                const auto type = reader.u8();
                if (!validEntityType(type))
                    return std::nullopt;
                entity.type = static_cast<EntityType>(type);
                entity.variant = reader.u8();
                entity.x = reader.f32();
                entity.y = reader.f32();
                snapshot.entities.push_back(entity);
            }
            message = std::move(snapshot);
            break;
        }
        case MessageType::EntityDestroyed: {
            if (bodySize != 5)
                return std::nullopt;
            const auto netId = reader.u32();
            const auto reason = reader.u8();
            if (reason < static_cast<std::uint8_t>(DestroyReason::Killed) ||
                reason > static_cast<std::uint8_t>(DestroyReason::Collected))
                return std::nullopt;
            message = EntityDestroyed{netId, static_cast<DestroyReason>(reason)};
            break;
        }
        case MessageType::PlayerLeft: {
            if (bodySize != 2)
                return std::nullopt;
            const auto playerId = reader.u8();
            const auto reason = reader.u8();
            if (playerId >= MaxPlayers || reason < static_cast<std::uint8_t>(LeaveReason::Disconnected) ||
                reason > static_cast<std::uint8_t>(LeaveReason::TimedOut))
                return std::nullopt;
            message = PlayerLeft{playerId, static_cast<LeaveReason>(reason)};
            break;
        }
        case MessageType::Pong:
            if (bodySize != 4)
                return std::nullopt;
            message = Pong{reader.u32()};
            break;
        default:
            return std::nullopt;
        }

        if (!reader.ok() || reader.remaining() != 0)
            return std::nullopt;
        return Packet{sequence, std::move(message), reliableId};
    } catch (...) {
        return std::nullopt;
    }
}

} // namespace rtype::protocol
