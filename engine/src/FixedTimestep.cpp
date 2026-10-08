#include "engine/core/FixedTimestep.hpp"

#include <stdexcept>

namespace engine::core {

FixedTimestep::FixedTimestep([[maybe_unused]] double ticksPerSecond, [[maybe_unused]] int maxTicksPerAdvance)
{
    throw std::logic_error("FixedTimestep not implemented");
}

int FixedTimestep::advance()
{
    throw std::logic_error("FixedTimestep::advance not implemented");
}

float FixedTimestep::step() const
{
    throw std::logic_error("FixedTimestep::step not implemented");
}

FixedTimestep::Clock::duration FixedTimestep::untilNextTick() const
{
    throw std::logic_error("FixedTimestep::untilNextTick not implemented");
}

} // namespace engine::core
