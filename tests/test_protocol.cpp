#include "common/Protocol.hpp"
#include "common/SnapshotAssembler.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <limits>

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

TEST(Protocol, SnapshotCarriesTheLivesOfEachPlayer)
{
    Snapshot sent{7, GameState::Playing, 0, {{1, EntityType::Player, 2, 10.5f, 20.f}}};
    sent.lives = {3, 2, 0, 1};
    const auto packet = decode(encode({1, sent}));
    ASSERT_TRUE(packet);
    const auto &received = std::get<Snapshot>(packet->message);
    EXPECT_EQ(received.lives, sent.lives);
    ASSERT_EQ(received.entities.size(), 1u); // the entities still come after the lives
    EXPECT_EQ(received.entities[0].variant, 2);
}

TEST(Protocol, SnapshotCarriesObstaclesAndBonusesButNoUnknownEntityType)
{
    auto bytes = encode({1, Snapshot{1, GameState::Playing, 0, {{1, EntityType::Obstacle, 2, 500.f, 300.f}}}});
    const auto packet = decode(bytes);
    ASSERT_TRUE(packet);
    EXPECT_EQ(std::get<Snapshot>(packet->message).entities[0].type, EntityType::Obstacle);

    bytes[HeaderSize + SnapshotFixedSize + 4] = 6;
    EXPECT_EQ(std::get<Snapshot>(decode(bytes)->message).entities[0].type, EntityType::Bonus);
    bytes[HeaderSize + SnapshotFixedSize + 4] = 7;
    EXPECT_FALSE(decode(bytes));
}

TEST(Protocol, SnapshotCarriesTheGameOverStateButNoUnknownState)
{
    auto bytes = encode({1, Snapshot{1, GameState::GameOver, 0, {}}});
    const auto packet = decode(bytes);
    ASSERT_TRUE(packet);
    EXPECT_EQ(std::get<Snapshot>(packet->message).state, GameState::GameOver);

    bytes[HeaderSize + 4] = 5; // the state comes right after the u32 tick
    EXPECT_FALSE(decode(bytes));
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
                                           EntityDestroyed{10, DestroyReason::Collected},
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
    bytes[HeaderSize + SnapshotFixedSize - 1] = 50; // count says 50 entities, payload only holds 1
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
    input[HeaderSize] = 0x40;
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

TEST(Protocol, RecognisesAConnectWrittenForAnotherVersion)
{
    auto bytes = encode({1, Connect{}});
    EXPECT_FALSE(otherVersionConnect(bytes)) << "same version: a normal Connect";

    bytes[2] = Version + 3;
    ASSERT_TRUE(otherVersionConnect(bytes));
    EXPECT_EQ(*otherVersionConnect(bytes), Version + 3);
    EXPECT_FALSE(decode(bytes));
}

TEST(Protocol, OnlyAConnectOfAnotherVersionGetsAnAnswer)
{
    auto input = encode({1, Input{0}});
    input[2] = Version + 1;
    EXPECT_FALSE(otherVersionConnect(input));

    auto badMagic = encode({1, Connect{}});
    badMagic[2] = Version + 1;
    badMagic[0] = 0;
    EXPECT_FALSE(otherVersionConnect(badMagic));
    EXPECT_FALSE(otherVersionConnect(std::vector<std::uint8_t>{0x52, 0x54}));
}

TEST(Protocol, EncodeCanWriteTheVersionOfTheSenderBeingAnswered)
{
    const auto bytes = encode({4, ConnectRejected{RejectReason::VersionMismatch}}, 3);
    EXPECT_EQ(bytes[2], 3);
    auto asOurs = bytes;
    asOurs[2] = Version;
    EXPECT_TRUE(decode(asOurs));
}

TEST(Protocol, RejectsAnotherProtocolVersion)
{
    auto bytes = encode({1, Connect{}});
    bytes[2] = Version - 1;
    EXPECT_FALSE(decode(bytes));
}

TEST(Quantization, ARoundTripIsWithinHalfAStepOfTheOriginalPosition)
{
    constexpr float HalfStep = 0.5f / PositionScale;
    constexpr float FloatPrecisionNear3000 = 1e-3f; // a float cannot hold a value near 3000 better than that
    for (float value = -1000.f; value < 3000.f; value += 0.37f)
        EXPECT_LE(std::abs(dequantizePosition(quantizePosition(value)) - value), HalfStep + FloatPrecisionNear3000)
            << value;
}

TEST(Quantization, PositionsOfTheWorldAreExactWhenTheyAreMultiplesOfOneSixteenth)
{
    for (float value : {0.f, 100.f, 1919.f, 1080.f, -71.f, 12.5f, 33.0625f})
        EXPECT_FLOAT_EQ(dequantizePosition(quantizePosition(value)), value);
}

TEST(Quantization, ValuesOutsideTheRangeAreClampedAndNeverWrapAround)
{
    EXPECT_EQ(quantizePosition(-5000.f), 0);
    EXPECT_EQ(quantizePosition(100000.f), 65535);
    EXPECT_FLOAT_EQ(dequantizePosition(0), -PositionOffset);
    EXPECT_GT(dequantizePosition(65535), 3071.f);
    // A ship far outside the world is shown at the edge of the range, not on the other side of the screen.
    EXPECT_LT(dequantizePosition(quantizePosition(-2000.f)), -1000.f);
    EXPECT_GT(dequantizePosition(quantizePosition(5000.f)), 3000.f);
}

TEST(Quantization, ANotANumberPositionDoesNotBreakTheEncoder)
{
    const float nan = std::numeric_limits<float>::quiet_NaN();
    EXPECT_EQ(quantizePosition(nan), 0);
    EXPECT_EQ(quantizePosition(std::numeric_limits<float>::infinity()), 65535);
    EXPECT_EQ(quantizePosition(-std::numeric_limits<float>::infinity()), 0);
}

TEST(Quantization, AnEntityCostsTenBytesInASnapshot)
{
    EXPECT_EQ(EntityStateSize, 10u);
    Snapshot snapshot{1, GameState::Playing, 0, {}, {}};
    const auto empty = encode({1, snapshot}).size();
    for (std::uint32_t i = 1; i <= 7; ++i)
        snapshot.entities.push_back({i, EntityType::Enemy, 0, 100.f, 200.f});
    EXPECT_EQ(encode({1, snapshot}).size(), empty + 7 * 10);
}

TEST(Quantization, AFullDatagramHoldsMoreEntitiesThanBefore)
{
    EXPECT_EQ(MaxEntitiesPerSnapshot, (MaxPacketSize - HeaderSize - SnapshotFixedSize) / 10);
    EXPECT_GE(MaxEntitiesPerSnapshot, 110u);

    Snapshot snapshot{1, GameState::Playing, 0, {}, {}};
    for (std::uint32_t i = 0; i < MaxEntitiesPerSnapshot; ++i)
        snapshot.entities.push_back({i, EntityType::PlayerMissile, 0, 1.f * static_cast<float>(i), 2.f});
    const auto bytes = encode({1, snapshot});
    EXPECT_LE(bytes.size(), MaxPacketSize);
    const auto decoded = decode(bytes);
    ASSERT_TRUE(decoded);
    EXPECT_EQ(std::get<Snapshot>(decoded->message).entities.size(), MaxEntitiesPerSnapshot);
}

TEST(Quantization, TheDecodedSnapshotKeepsEveryOtherFieldExactly)
{
    Snapshot sent{4242, GameState::Finished, 987654, {{70000, EntityType::Bonus, 3, 640.25f, 360.75f}}, {3, 0, 2, 1}};
    const auto decoded = decode(encode({1, sent}));
    ASSERT_TRUE(decoded);
    const auto &received = std::get<Snapshot>(decoded->message);
    EXPECT_EQ(received.tick, 4242u);
    EXPECT_EQ(received.state, GameState::Finished);
    EXPECT_EQ(received.score, 987654u);
    EXPECT_EQ(received.lives, (std::array<std::uint8_t, 4>{3, 0, 2, 1}));
    ASSERT_EQ(received.entities.size(), 1u);
    EXPECT_EQ(received.entities[0].netId, 70000u);
    EXPECT_EQ(received.entities[0].type, EntityType::Bonus);
    EXPECT_EQ(received.entities[0].variant, 3);
    EXPECT_FLOAT_EQ(received.entities[0].x, 640.25f);
    EXPECT_FLOAT_EQ(received.entities[0].y, 360.75f);
}

// The example datagrams of docs/protocol.md §7, byte for byte: the specification and the code must agree.
TEST(Protocol, SpecificationExamplesMatchTheEncoder)
{
    const std::vector<std::uint8_t> input = {0x52, 0x54, 0x09, 0x02, 0x00, 0x00, 0x00, 0x07, 0x00, 0x01, 0x11};
    EXPECT_EQ(encode({7, Input{InputFlag::Up | InputFlag::Shoot}}), input);

    const std::vector<std::uint8_t> snapshot = {0x52, 0x54, 0x09, 0x83, 0x00, 0x00, 0x00, 0x08, 0x00, 0x1C,
                                                0x00, 0x00, 0x00, 0x78, 0x02, 0x00, 0x00, 0x01, 0x2C, 0x03,
                                                0x00, 0x00, 0x00, 0x01, 0x02, 0x00, 0x00, 0x01, 0x00, 0x00,
                                                0x00, 0x05, 0x01, 0x00, 0x46, 0x40, 0x52, 0xC0};
    EXPECT_EQ(
        encode(
            {8,
             Snapshot{120, GameState::Playing, 300, {{5, EntityType::Player, 0, 100.f, 300.f}}, {3, 0, 0, 0}, 1, 2}}),
        snapshot);
}
