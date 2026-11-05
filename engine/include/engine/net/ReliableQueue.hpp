#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <utility>

namespace engine::net {

/// @brief Messages sent to one peer that must arrive, kept until the peer acknowledges them.
///
/// UDP may lose a datagram, so a critical message is numbered, remembered and sent again at intervals until the
/// peer answers with an acknowledgement carrying its number. The queue never touches a socket: the caller gives
/// resendDue() a function that sends one message, and calls acknowledge() when an acknowledgement arrives.
///
/// The interval between two sends adapts to the network. Every acknowledgement of a message that was sent only once
/// gives a round-trip time sample, smoothed as in TCP (RFC 6298); the interval is that estimate with a safety margin,
/// and it doubles after each unanswered send (up to a limit) so that a dead peer is not flooded.
///
/// Message numbers go up by one. At most Window numbers are in flight at once, counted from the oldest unacknowledged
/// one, so a receiver that remembers the last Window numbers it saw (SequenceWindow) can always tell a copy from a new
/// message. Not thread-safe: use it from one thread.
///
/// @tparam Message Whatever is sent; it is only copied and handed to the send function.
template <typename Message> class ReliableQueue {
public:
    using Clock = std::chrono::steady_clock;
    using Milliseconds = std::chrono::milliseconds;

    /// How many numbers can be in flight, counted from the oldest unacknowledged one. Equals SequenceWindow::Size.
    static constexpr std::uint32_t Window = 32;

    struct Config {
        Milliseconds initialInterval{200}; ///< Before any round-trip time is known.
        Milliseconds minInterval{50};
        Milliseconds maxInterval{1000};
        int maxAttempts = 12; ///< Sends of one message before giving up on it (the peer is probably gone).
    };

    ReliableQueue() = default;
    explicit ReliableQueue(const Config &config) : _config(config) {}

    /// Registers a message. It is sent by the next call to resendDue().
    /// @return The number to put in the packet, and that the peer sends back in its acknowledgement.
    std::uint32_t push(Message message)
    {
        const std::uint32_t id = _nextId++;
        _entries.push_back(Entry{id, std::move(message), Clock::time_point{}, 0});
        return id;
    }

    /// Forgets a message because the peer received it.
    /// @return false when the number is not pending (already acknowledged: a late copy of the acknowledgement).
    bool acknowledge(std::uint32_t id, Clock::time_point now)
    {
        const auto found = std::find_if(_entries.begin(), _entries.end(), [id](const Entry &entry) { return entry.id == id; });
        if (found == _entries.end())
            return false;
        // A message sent several times could be answered by any of its copies: such a sample would be wrong.
        if (found->attempts == 1)
            addRttSample(std::chrono::duration<double, std::milli>(now - found->lastSent).count());
        _entries.erase(found);
        return true;
    }

    /// Sends what has to be sent now: new messages, and old ones whose interval elapsed.
    /// @param now The current time.
    /// @param send Called as send(number, message) for each message to put on the wire.
    /// @return How many of those were retransmissions.
    template <typename Send> std::size_t resendDue(Clock::time_point now, Send &&send)
    {
        std::size_t retransmissions = 0;
        if (_entries.empty())
            return 0;
        const std::uint32_t oldest = _entries.front().id;
        for (auto entry = _entries.begin(); entry != _entries.end();) {
            if (entry->id - oldest >= Window)
                break; // waits until the older ones are acknowledged
            if (entry->attempts > 0 && now - entry->lastSent < waitAfter(entry->attempts)) {
                ++entry;
                continue;
            }
            if (entry->attempts >= _config.maxAttempts) {
                entry = _entries.erase(entry);
                ++_abandoned;
                continue;
            }
            send(entry->id, entry->message);
            if (entry->attempts > 0)
                ++retransmissions;
            entry->lastSent = now;
            ++entry->attempts;
            ++entry;
        }
        return retransmissions;
    }

    /// @return How many messages still wait for their acknowledgement.
    std::size_t pending() const { return _entries.size(); }

    /// @return How many messages were given up after maxAttempts sends.
    std::size_t abandoned() const { return _abandoned; }

    /// @return The smoothed round-trip time in milliseconds, empty before the first sample.
    std::optional<double> smoothedRttMs() const { return _hasRtt ? std::optional<double>(_srtt) : std::nullopt; }

    /// @return How long to wait before sending a message a second time.
    Milliseconds interval() const
    {
        if (!_hasRtt)
            return _config.initialInterval;
        const auto estimate = Milliseconds(static_cast<Milliseconds::rep>(_srtt + 4.0 * _rttVariation));
        return std::clamp(estimate, _config.minInterval, _config.maxInterval);
    }

private:
    struct Entry {
        std::uint32_t id;
        Message message;
        Clock::time_point lastSent;
        int attempts;
    };

    // The interval doubles after each send (1x, 2x, 4x, 8x at most), without exceeding maxInterval.
    Milliseconds waitAfter(int attempts) const
    {
        const int doublings = std::min(attempts - 1, 3);
        return std::min(interval() * (1 << doublings), std::max(_config.maxInterval, interval()));
    }

    void addRttSample(double sampleMs)
    {
        if (!_hasRtt) {
            _hasRtt = true;
            _srtt = sampleMs;
            _rttVariation = sampleMs / 2.0;
            return;
        }
        _rttVariation = 0.75 * _rttVariation + 0.25 * std::abs(_srtt - sampleMs);
        _srtt = 0.875 * _srtt + 0.125 * sampleMs;
    }

    Config _config;
    std::deque<Entry> _entries; // ascending numbers
    std::uint32_t _nextId = 0;
    std::size_t _abandoned = 0;
    bool _hasRtt = false;
    double _srtt = 0.0;
    double _rttVariation = 0.0;
};

} // namespace engine::net
