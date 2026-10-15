#pragma once

#include <chrono>

namespace engine::core {

// Turns real elapsed time into a number of fixed-size simulation steps,
// so the game runs at the same speed whatever the CPU speed or frame rate.
//
//   FixedTimestep clock(60.0);
//   while (running) {
//       for (int ticks = clock.advance(); ticks > 0; ticks--)
//           registry.update(clock.step());
//       std::this_thread::sleep_for(clock.untilNextTick());
//   }
class FixedTimestep {
public:
    using Clock = std::chrono::steady_clock;

    // ticksPerSecond = 60 means one step every 1/60 s. Reject invalid rates and limits.
    explicit FixedTimestep(double ticksPerSecond, int maxTicksPerAdvance = 5);

    // Consume elapsed time and return at most maxTicksPerAdvance steps.
    // Discard excess whole steps after a stall, keeping only the fractional remainder.
    int advance();

    // Duration of one step, in seconds.
    float step() const;

    // Time left before the next step is due, accounting for time since advance().
    Clock::duration untilNextTick() const;

private:
    double _timeStepSeconds;
    double _accumulatedSeconds = 0.0;
    int _maxTicksPerAdvance;
    Clock::time_point _lastAdvance;
};

} // namespace engine::core
