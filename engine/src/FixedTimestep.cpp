#include "engine/core/FixedTimestep.hpp"

#include <cmath>
#include <stdexcept>

namespace engine::core {

FixedTimestep::FixedTimestep(double ticksPerSecond, int maxTicksPerAdvance)
    : _timeStepSeconds(0.0), _maxTicksPerAdvance(maxTicksPerAdvance), _lastAdvance(Clock::now())
{
    if (!std::isfinite(ticksPerSecond) || ticksPerSecond <= 0.0 || maxTicksPerAdvance <= 0)
        throw std::invalid_argument("Tick rate and catch-up limit must be finite and positive");

    _timeStepSeconds = 1.0 / ticksPerSecond;
    const double clockResolutionSeconds = std::chrono::duration<double>(Clock::duration{1}).count();
    const double maxDurationSeconds = std::chrono::duration<double>(Clock::duration::max()).count();
    if (!std::isfinite(_timeStepSeconds) || _timeStepSeconds < clockResolutionSeconds ||
        _timeStepSeconds >= maxDurationSeconds)
        throw std::invalid_argument("Tick duration is outside the clock's supported range");
}

int FixedTimestep::advance()
{
    const auto currentTime = Clock::now();
    _accumulatedSeconds += std::chrono::duration<double>(currentTime - _lastAdvance).count();
    _lastAdvance = currentTime;

    int ticksToSimulate = 0;
    while (_accumulatedSeconds >= _timeStepSeconds && ticksToSimulate < _maxTicksPerAdvance) {
        _accumulatedSeconds -= _timeStepSeconds;
        ticksToSimulate++;
    }

    if (_accumulatedSeconds >= _timeStepSeconds)
        _accumulatedSeconds = std::fmod(_accumulatedSeconds, _timeStepSeconds);

    return ticksToSimulate;
}

float FixedTimestep::step() const
{
    return static_cast<float>(_timeStepSeconds);
}

FixedTimestep::Clock::duration FixedTimestep::untilNextTick() const
{
    const double elapsedSeconds = std::chrono::duration<double>(Clock::now() - _lastAdvance).count();
    const double remainingSeconds = _timeStepSeconds - _accumulatedSeconds - elapsedSeconds;
    if (remainingSeconds <= 0.0)
        return Clock::duration::zero();
    return std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(remainingSeconds));
}

} // namespace engine::core
