#include "common/Protocol.hpp"

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
