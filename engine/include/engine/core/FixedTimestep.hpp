#pragma once

#include <chrono>

namespace engine::core {

// Turns real elapsed time into a number of fixed-size simulation steps,
// so the game runs at the same speed whatever the CPU speed or frame rate.
//
//   FixedTimestep clock(60.0);
//   while (running) {
//       for (int i = clock.advance(); i > 0; --i)
//           registry.update(clock.step());
//   }
class FixedTimestep {
public:
    using Clock = std::chrono::steady_clock;

    explicit FixedTimestep(double ticksPerSecond, int maxTicksPerAdvance = 5);

    // Number of steps to simulate since the last call (capped, to avoid a "spiral of death" after a freeze).
    int advance();

    // Duration of one step, in seconds.
    [[nodiscard]] float step() const { return static_cast<float>(_step.count()); }

    // Time left before the next step is due, to sleep instead of busy-waiting.
    [[nodiscard]] Clock::duration untilNextTick() const;

private:
    std::chrono::duration<double> _step;
    int _maxTicks;
    Clock::time_point _last;
    std::chrono::duration<double> _accumulator{0.0};
};

} // namespace engine::core
