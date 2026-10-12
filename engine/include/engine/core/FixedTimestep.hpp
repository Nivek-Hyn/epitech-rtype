#pragma once

#include <chrono>

namespace engine::core {

// Turns real elapsed time into a number of fixed-size simulation steps,
// so the game runs at the same speed whatever the CPU speed or frame rate.
//
//   FixedTimestep clock(60.0);
//   while (running) {
//       for (int ticks = clock.advance(); ticks > 0; --ticks)
//           registry.update(clock.step());
//       std::this_thread::sleep_for(clock.untilNextTick());
//   }
class FixedTimestep {
public:
    using Clock = std::chrono::steady_clock;

    // TODO(engine): ticksPerSecond = 60 means one step every 1/60 s.
    explicit FixedTimestep(double ticksPerSecond, int maxTicksPerAdvance = 5);

    // TODO(engine): number of steps to simulate since the last call, never more than maxTicksPerAdvance
    // (otherwise after a freeze the game would try to catch up forever).
    int advance();

    // TODO(engine): duration of one step, in seconds.
    float step() const;

    // TODO(engine): time left before the next step is due, so the loop can sleep instead of using 100% CPU.
    Clock::duration untilNextTick() const;
};

} // namespace engine::core
