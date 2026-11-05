#include "common/Protocol.hpp"
#include "common/SnapshotAssembler.hpp"

#include <gtest/gtest.h>

using namespace rtype::protocol;

TEST(Protocol, RoundTripInput)
{
    const auto bytes = encode({42, Input{InputFlag::Up | InputFlag::Shoot}});
    ASSERT_EQ(bytes.size(), HeaderSize + 1);

    const auto packet = decode(bytes);
    ASSERT_TRUE(packet);
    EXPECT_EQ(packet->sequence, 42u);
    ASSERT_TRUE(std::holds_alternative<Input>(packet->message));
    EXPECT_EQ(std::get<Input>(packet->message).flags, InputFlag::Up | InputFlag::Shoot);
}

TEST(Protocol, RoundTripSnapshot)
{
    Snapshot sent{7,
                  GameState::Playing,
                  1500,
                  {{1, EntityType::Player, 2, 10.5f, 20.f}, {9, EntityType::Enemy, 0, 1800.f, 300.f}}};
    const auto packet = decode(encode({1, sent}));
    ASSERT_TRUE(packet);
    const auto &received = std::get<Snapshot>(packet->message);
    EXPECT_EQ(received.tick, 7u);
    EXPECT_EQ(received.state, GameState::Playing);
    EXPECT_EQ(received.score, 1500u);
    ASSERT_EQ(received.entities.size(), 2u);
    EXPECT_EQ(received.entities[0].variant, 2);
    EXPECT_FLOAT_EQ(received.entities[0].x, 10.5f);
    EXPECT_EQ(received.entities[1].type, EntityType::Enemy);
}

TEST(Protocol, RoundTripsEveryMessageType)
{
    const std::vector<Message> messages = {Connect{},
                                           Input{InputFlag::Down},
                                           Disconnect{},
                                           Ping{123},
                                           ConnectAccepted{3},
                                           ConnectRejected{RejectReason::VersionMismatch},
                                           Snapshot{},
                                           EntityDestroyed{9, DestroyReason::OutOfBounds},
                                           PlayerLeft{2, LeaveReason::TimedOut},
                                           Pong{456}};

    for (const Message &message : messages) {
        const auto decoded = decode(encode({7, message}));
        ASSERT_TRUE(decoded);
        EXPECT_EQ(decoded->sequence, 7u);
        EXPECT_EQ(decoded->message.index(), message.index());
    }
}

TEST(Protocol, HeaderIsBigEndian)
{
    const auto bytes = encode({0x01020304, Connect{}});
    ASSERT_EQ(bytes.size(), HeaderSize);
    EXPECT_EQ(bytes[0], 0x52); // 'R'
    EXPECT_EQ(bytes[1], 0x54); // 'T'
    EXPECT_EQ(bytes[4], 0x01);
    EXPECT_EQ(bytes[7], 0x04);
}

TEST(Protocol, RejectsMalformedPackets)
{
    auto valid = encode({1, Ping{123}});

    EXPECT_FALSE(decode({}));
    EXPECT_FALSE(decode(std::span(valid).first(HeaderSize - 1)));   // truncated header
    EXPECT_FALSE(decode(std::span(valid).first(valid.size() - 1))); // truncated payload

    auto badMagic = valid;
    badMagic[0] = 0;
    EXPECT_FALSE(decode(badMagic));

    auto unknownType = valid;
    unknownType[3] = 0x7F;
    EXPECT_FALSE(decode(unknownType));

    auto trailing = valid;
    trailing.push_back(0);
    EXPECT_FALSE(decode(trailing)); // payloadSize no longer matches
}

TEST(Protocol, RejectsSnapshotWithLyingCount)
{
    auto bytes = encode({1, Snapshot{1, GameState::Playing, 0, {{1, EntityType::Player, 0, 0.f, 0.f}}}});
    bytes[HeaderSize + 10] = 50; // count says 50 entities, payload only holds 1
    EXPECT_FALSE(decode(bytes));
}

TEST(Protocol, RejectsSnapshotWithUnknownState)
{
    auto bytes = encode({1, Snapshot{1, GameState::Playing, 0, {}}});
    bytes[HeaderSize + 4] = 9; // state 9 doesn't exist
    EXPECT_FALSE(decode(bytes));
}

TEST(Protocol, RejectsReservedInputFlagsAndUnknownEnumValues)
{
    auto input = encode({1, Input{}});
    input[HeaderSize] = 0x20;
    EXPECT_FALSE(decode(input));

    auto accepted = encode({1, ConnectAccepted{0}});
    accepted[HeaderSize] = 4;
    EXPECT_FALSE(decode(accepted));

    auto destroyed = encode({1, EntityDestroyed{1, DestroyReason::Killed}});
    destroyed[HeaderSize + 4] = 0;
    EXPECT_FALSE(decode(destroyed));

    auto entityType = encode({1, Snapshot{1, GameState::Playing, 0, {{1, EntityType::Player, 0, 0.f, 0.f}}}});
    entityType[HeaderSize + SnapshotFixedSize + 4] = 0;
    EXPECT_FALSE(decode(entityType));
}

namespace {

Snapshot makeSnapshot(std::uint32_t tick, std::uint32_t firstId, std::size_t count)
{
    Snapshot snapshot{tick, GameState::Playing, 300, {}};
    for (std::size_t i = 0; i < count; ++i)
        snapshot.entities.push_back({firstId + static_cast<std::uint32_t>(i), EntityType::EnemyMissile, 0, 1.f, 2.f});
    return snapshot;
}

} // namespace

TEST(Protocol, SplitSnapshotKeepsSmallSnapshotsWhole)
{
    EXPECT_EQ(splitSnapshot(makeSnapshot(3, 1, 0)).size(), 1u);
    EXPECT_EQ(splitSnapshot(makeSnapshot(3, 1, MaxEntitiesPerSnapshot)).size(), 1u);
}

TEST(Protocol, SplitSnapshotProducesEncodablePartsWithTheSameTick)
{
    const Snapshot big = makeSnapshot(9, 1, MaxEntitiesPerSnapshot * 2 + 5);
    const auto parts = splitSnapshot(big);
    ASSERT_EQ(parts.size(), 3u);

    std::size_t total = 0;
    for (const Snapshot &part : parts) {
        EXPECT_EQ(part.tick, 9u);
        EXPECT_EQ(part.score, 300u);
        EXPECT_LE(part.entities.size(), MaxEntitiesPerSnapshot);
        EXPECT_TRUE(decode(encode({1, part})));
        total += part.entities.size();
    }
    EXPECT_EQ(total, big.entities.size());
}

TEST(SnapshotAssembler, MergesPacketsOfTheSameTick)
{
    SnapshotAssembler assembler;
    for (const Snapshot &part : splitSnapshot(makeSnapshot(5, 1, 200)))
        EXPECT_TRUE(assembler.add(part));
    EXPECT_EQ(assembler.entities().size(), 200u);
    EXPECT_EQ(assembler.score(), 300u);
}

TEST(SnapshotAssembler, NewerTickReplacesEntities)
{
    SnapshotAssembler assembler;
    assembler.add(makeSnapshot(5, 1, 10));
    assembler.add(makeSnapshot(6, 100, 3));
    EXPECT_EQ(assembler.entities().size(), 3u);
    EXPECT_EQ(assembler.tick(), 6u);
}

TEST(SnapshotAssembler, IgnoresOlderTicksAndDuplicatedPackets)
{
    SnapshotAssembler assembler;
    assembler.add(makeSnapshot(6, 1, 4));
    EXPECT_FALSE(assembler.add(makeSnapshot(5, 50, 9)));
    EXPECT_TRUE(assembler.add(makeSnapshot(6, 1, 4)));
    EXPECT_EQ(assembler.entities().size(), 4u);
}

TEST(SnapshotAssembler, ResetAcceptsTicksThatStartAgain)
{
    SnapshotAssembler assembler;
    assembler.add(makeSnapshot(500, 1, 4));
    assembler.reset();
    EXPECT_TRUE(assembler.add(makeSnapshot(1, 7, 2)));
    EXPECT_EQ(assembler.entities().size(), 2u);
}

TEST(Protocol, AckRoundTripsAndCarriesTheReliableNumber)
{
    const auto packet = decode(encode({3, Ack{0x01020304}}));
    ASSERT_TRUE(packet);
    EXPECT_FALSE(packet->reliableId);
    ASSERT_TRUE(std::holds_alternative<Ack>(packet->message));
    EXPECT_EQ(std::get<Ack>(packet->message).reliableId, 0x01020304u);
}

TEST(Protocol, ReliableMessageSetsTheFlagAndPrefixesItsNumber)
{
    const auto bytes = encode({9, EntityDestroyed{77, DestroyReason::Killed}, 5u});
    ASSERT_EQ(bytes.size(), HeaderSize + 4 + 5);
    EXPECT_EQ(bytes[3], static_cast<std::uint8_t>(MessageType::EntityDestroyed) | ReliableFlag);
    EXPECT_EQ(bytes[HeaderSize + 3], 5); // the reliable number comes first, big-endian

    const auto packet = decode(bytes);
    ASSERT_TRUE(packet);
    ASSERT_TRUE(packet->reliableId);
    EXPECT_EQ(*packet->reliableId, 5u);
    const auto &destroyed = std::get<EntityDestroyed>(packet->message);
    EXPECT_EQ(destroyed.netId, 77u);
    EXPECT_EQ(destroyed.reason, DestroyReason::Killed);
}

TEST(Protocol, AnyMessageWithoutPayloadCanBeReliable)
{
    const auto packet = decode(encode({1, Disconnect{}, 0u}));
    ASSERT_TRUE(packet);
    EXPECT_TRUE(std::holds_alternative<Disconnect>(packet->message));
    ASSERT_TRUE(packet->reliableId);
    EXPECT_EQ(*packet->reliableId, 0u);
}

TEST(Protocol, RejectsAnAckThatClaimsToBeReliable)
{
    auto bytes = encode({1, Ack{4}, 2u}); // an Ack is never itself acknowledged
    EXPECT_FALSE(decode(bytes));
}

TEST(Protocol, RejectsAReliablePacketTooShortToHoldItsNumber)
{
    auto bytes = encode({1, Disconnect{}});
    bytes[3] |= ReliableFlag; // flag set, but the 4-byte number is missing
    EXPECT_FALSE(decode(bytes));
}

TEST(Protocol, RejectsAnotherProtocolVersion)
{
    auto bytes = encode({1, Connect{}});
    bytes[2] = Version - 1;
    EXPECT_FALSE(decode(bytes));
}
