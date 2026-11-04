#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>

namespace engine::net {

/// @brief Amount of data exchanged since a counter was created or reset.
struct TrafficTotals {
    std::uint64_t bytesSent = 0;
    std::uint64_t bytesReceived = 0;
    std::uint64_t packetsSent = 0;
    std::uint64_t packetsReceived = 0;
};

/// @brief Bandwidth measured between two samples, in bytes per second.
struct TrafficRates {
    double sentBytesPerSecond = 0.0;
    double receivedBytesPerSecond = 0.0;
};

/// @brief Totals and current rates, as exposed to the rest of the program.
struct TrafficStats {
    TrafficTotals totals;
    TrafficRates rates;
};

/// @brief Counts datagrams and bytes; safe to update from a network thread and read from another one.
class TrafficCounter {
public:
    void addSent(std::size_t bytes)
    {
        _bytesSent.fetch_add(bytes, std::memory_order_relaxed);
        _packetsSent.fetch_add(1, std::memory_order_relaxed);
    }

    void addReceived(std::size_t bytes)
    {
        _bytesReceived.fetch_add(bytes, std::memory_order_relaxed);
        _packetsReceived.fetch_add(1, std::memory_order_relaxed);
    }

    TrafficTotals totals() const
    {
        return {_bytesSent.load(std::memory_order_relaxed), _bytesReceived.load(std::memory_order_relaxed),
                _packetsSent.load(std::memory_order_relaxed), _packetsReceived.load(std::memory_order_relaxed)};
    }

    void reset()
    {
        _bytesSent = 0;
        _bytesReceived = 0;
        _packetsSent = 0;
        _packetsReceived = 0;
    }

private:
    std::atomic<std::uint64_t> _bytesSent{0};
    std::atomic<std::uint64_t> _bytesReceived{0};
    std::atomic<std::uint64_t> _packetsSent{0};
    std::atomic<std::uint64_t> _packetsReceived{0};
};

/// @brief Turns successive totals into a bandwidth.
///
/// Call sample() regularly from one thread (e.g. a once-per-second timer); each call returns the rate since the
/// previous one. The first call only records a starting point and returns zeros.
class RateMeter {
public:
    /// @param totals The counter's current totals.
    /// @param nowSeconds Any monotonic clock, in seconds.
    /// @return The average rates since the previous call.
    TrafficRates sample(const TrafficTotals &totals, double nowSeconds)
    {
        TrafficRates rates;
        const double elapsed = nowSeconds - _lastSeconds;
        if (_hasSample && elapsed > 0.0) {
            rates.sentBytesPerSecond = static_cast<double>(totals.bytesSent - _last.bytesSent) / elapsed;
            rates.receivedBytesPerSecond = static_cast<double>(totals.bytesReceived - _last.bytesReceived) / elapsed;
        }
        _hasSample = true;
        _last = totals;
        _lastSeconds = nowSeconds;
        return rates;
    }

private:
    bool _hasSample = false;
    TrafficTotals _last;
    double _lastSeconds = 0.0;
};

} // namespace engine::net
