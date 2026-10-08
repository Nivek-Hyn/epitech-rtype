#pragma once

#include <optional>
#include <stdexcept>

namespace engine::core {

// Queue shared between two threads (e.g. network thread -> game thread). Never blocks the reader.
template <typename T> class ThreadSafeQueue {
public:
    // TODO(network): add value at the back. Must be safe to call from one thread while another calls tryPop.
    void push([[maybe_unused]] T value) { throw std::logic_error("ThreadSafeQueue::push not implemented"); }

    // TODO(network): remove and return the front value, or nullopt if the queue is empty. Never waits.
    [[nodiscard]] std::optional<T> tryPop() { throw std::logic_error("ThreadSafeQueue::tryPop not implemented"); }
};

} // namespace engine::core
