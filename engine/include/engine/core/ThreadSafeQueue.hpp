#pragma once

#include <deque>
#include <mutex>
#include <optional>
#include <utility>

namespace engine::core {

// Queue shared between two threads (e.g. network thread -> game thread). Never blocks the reader.
template <typename T> class ThreadSafeQueue {
public:
    void push(T value)
    {
        std::lock_guard lock(_mutex);
        _queue.push_back(std::move(value));
    }

    [[nodiscard]] std::optional<T> tryPop()
    {
        std::lock_guard lock(_mutex);
        if (_queue.empty())
            return std::nullopt;
        T value = std::move(_queue.front());
        _queue.pop_front();
        return value;
    }

private:
    std::mutex _mutex;
    std::deque<T> _queue;
};

} // namespace engine::core
