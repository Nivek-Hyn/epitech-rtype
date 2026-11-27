#pragma once

#include <algorithm>
#include <chrono>

namespace engine::net {

/// @brief Lets a steady number of events through per second, plus a short burst, and refuses the rest.
///
/// A "token bucket": the bucket holds up to `burst` tokens and gets `perSecond` new ones every second; each allowed
/// event takes one. A well-behaved peer never notices it, while a peer that floods the server is cut down to the steady
/// rate without any memory growing. Not thread-safe: use it from one thread.
class RateLimiter {
public:
    using Clock = std::chrono::steady_clock;

    /// @param perSecond Events allowed per second, in the long run.
    /// @param burst Events allowed at once after a quiet moment (at least 1).
    explicit RateLimiter(double perSecond = 1.0, double burst = 1.0)
        : _perSecond(perSecond), _burst(std::max(burst, 1.0)), _tokens(_burst)
    {
    }

    /// @return true if the event may go on, false if it is over the limit and must be dropped.
    bool allow(Clock::time_point now)
    {
        if (_started) {
            const double elapsed = std::chrono::duration<double>(now - _last).count();
            _tokens = std::min(_burst, _tokens + std::max(elapsed, 0.0) * _perSecond);
        }
        _started = true;
        _last = now;
        if (_tokens < 1.0)
            return false;
        _tokens -= 1.0;
        return true;
    }

private:
    double _perSecond;
    double _burst;
    double _tokens;
    bool _started = false;
    Clock::time_point _last{};
};

} // namespace engine::net
