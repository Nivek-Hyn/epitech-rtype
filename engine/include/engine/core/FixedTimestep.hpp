#pragma once

#include <chrono>

namespace engine::core {

/// @brief Turns real elapsed time into a number of fixed-size simulation steps,
/// so the game runs at the same speed whatever the CPU speed or frame rate.
///
/// @code
/// FixedTimestep clock(60.0);
/// while (running) {
///     for (int ticks = clock.advance(); ticks > 0; ticks--)
///         registry.update(clock.step());
///     std::this_thread::sleep_for(clock.untilNextTick());
/// }
/// @endcode
class FixedTimestep {
public:
    /// @brief Monotonic clock: never goes backwards when the system time changes.
    using Clock = std::chrono::steady_clock;

    /// @brief Create a clock producing @p ticksPerSecond steps per second.
    ///
    /// @param ticksPerSecond Number of steps per second: 60 means one step every 1/60 s.
    /// @param maxTicksPerAdvance Maximum number of steps advance() returns at once (catch-up limit).
    /// @throws std::invalid_argument if a value is not finite and positive, or if the step does not fit the clock.
    explicit FixedTimestep(double ticksPerSecond, int maxTicksPerAdvance = 5);

    /// @brief Consume the time elapsed since the last call.
    ///
    /// After a stall, the extra whole steps are dropped and only the fraction of a step is kept.
    ///
    /// @return The number of steps to simulate now, at most maxTicksPerAdvance.
    int advance();

    /// @brief Duration of one step, in seconds.
    float step() const;

    /// @brief Time left before the next step is due, counting the time spent since advance().
    ///
    /// @return A duration to pass to `std::this_thread::sleep_for`, zero if a step is already due.
    Clock::duration untilNextTick() const;

private:
    double _timeStepSeconds;
    double _accumulatedSeconds = 0.0;
    int _maxTicksPerAdvance;
    Clock::time_point _lastAdvance;
};

} // namespace engine::core
