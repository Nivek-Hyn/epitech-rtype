#pragma once

#include <cstdint>

namespace engine::net {

/// @brief What a receiver should think of a numbered packet.
enum class SequenceVerdict : std::uint8_t {
    Newest,    ///< Higher than every number seen so far: in order (some numbers before it may be missing).
    Late,      ///< Lower than the highest number but never seen: it arrived out of order.
    Duplicate, ///< Already received.
    TooOld,    ///< So far behind the highest number that it can no longer be told apart from a duplicate.
};

/// @brief Remembers which of the last 32 packet numbers of a peer were received.
///
/// The sender numbers its packets 0, 1, 2... and the receiver feeds every number to receive(). Keeping the highest
/// number and one bit per number below it (a "sliding window") is enough to spot duplicates and reordering with
/// 8 bytes of memory per peer. Numbers wrap around at 2^32 without any special case.
class SequenceWindow {
public:
    /// Packets this far behind the highest number are still recognised; older ones are TooOld.
    static constexpr std::uint32_t Size = 32;

    /// @param sequence The number written in the packet header.
    /// @return The verdict, and the number is remembered as received unless it is a Duplicate or TooOld.
    SequenceVerdict receive(std::uint32_t sequence)
    {
        if (!_started) {
            _started = true;
            _highest = sequence;
            _seen = 1;
            return SequenceVerdict::Newest;
        }

        const auto ahead = static_cast<std::int32_t>(sequence - _highest);
        if (ahead > 0) {
            const auto shift = static_cast<std::uint32_t>(ahead);
            _seen = shift >= Size ? 0 : _seen << shift;
            _seen |= 1;
            _highest = sequence;
            return SequenceVerdict::Newest;
        }

        const std::uint32_t behind = _highest - sequence; // 0 when it is the highest number again
        if (behind >= Size)
            return SequenceVerdict::TooOld;
        const std::uint32_t bit = 1u << behind;
        if (_seen & bit)
            return SequenceVerdict::Duplicate;
        _seen |= bit;
        return SequenceVerdict::Late;
    }

    /// Forgets everything, for a peer that starts a new session and numbers its packets from scratch.
    void reset() { *this = SequenceWindow{}; }

    bool started() const { return _started; }
    std::uint32_t highest() const { return _highest; }

private:
    bool _started = false;
    std::uint32_t _highest = 0;
    std::uint32_t _seen = 0; // bit n set: number (_highest - n) was received
};

} // namespace engine::net
