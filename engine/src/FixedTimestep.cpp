#include "engine/core/FixedTimestep.hpp"

namespace engine::core {

FixedTimestep::FixedTimestep(double ticksPerSecond, int maxTicksPerAdvance)
    : _step(1.0 / ticksPerSecond), _maxTicks(maxTicksPerAdvance), _last(Clock::now())
{
}

int FixedTimestep::advance()
{
    const auto now = Clock::now();
    _accumulator += now - _last;
    _last = now;

    int ticks = 0;
    while (_accumulator >= _step && ticks < _maxTicks) {
        _accumulator -= _step;
        ++ticks;
    }
    if (ticks == _maxTicks)
        _accumulator = std::chrono::duration<double>(0.0);
    return ticks;
}

FixedTimestep::Clock::duration FixedTimestep::untilNextTick() const
{
    const auto elapsed = _accumulator + (Clock::now() - _last);
    if (elapsed >= _step)
        return Clock::duration::zero();
    return std::chrono::duration_cast<Clock::duration>(_step - elapsed);
}

} // namespace engine::core
