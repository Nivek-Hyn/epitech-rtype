#pragma once

#include <cstdint>

namespace engine::net {

/// @brief Estimates how many numbered items of a stream never arrived (or arrived too late to be useful).
///
/// Feed it the number of every item received, in arrival order. An item with a number lower than or equal to the
/// highest seen so far is a duplicate or a late one and is ignored. Numbers are compared with wrap-around, so the
/// tracker keeps working when a 32-bit counter overflows.
class LossTracker {
public:
    void record(std::uint32_t number)
    {
        if (!_started) {
            _started = true;
            _first = number;
            _highest = number;
            _received = 1;
            return;
        }
        if (static_cast<std::int32_t>(number - _highest) > 0) {
            _highest = number;
            ++_received;
        }
    }

    void reset() { *this = LossTracker{}; }

    /// @return How many items were useful.
    std::uint64_t received() const { return _received; }

    /// @return How many items the sender produced between the first and the highest number seen.
    std::uint64_t expected() const { return _started ? static_cast<std::uint64_t>(_highest - _first) + 1 : 0; }

    /// @return The share of items missing, from 0 to 100.
    double lossPercent() const
    {
        const std::uint64_t total = expected();
        if (total == 0)
            return 0.0;
        return 100.0 * static_cast<double>(total - _received) / static_cast<double>(total);
    }

private:
    bool _started = false;
    std::uint32_t _first = 0;
    std::uint32_t _highest = 0;
    std::uint64_t _received = 0;
};

} // namespace engine::net
