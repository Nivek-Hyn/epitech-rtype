#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <stdexcept>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <utility>
#include <vector>

namespace engine::core {

// Lets systems react to each other without knowing each other.
// Any copyable struct can be an event; handlers subscribed to that exact type run in subscription order.
// publish delivers an event immediately; enqueue stores it until dispatch, e.g. once all systems have run,
// so handlers that kill entities never run while a system is still iterating over them.
//
//   struct Collision { Entity first; Entity second; };
//   bus.subscribe<Collision>([](const Collision &collision) { ... });
//   bus.enqueue(Collision{missile, enemy});
//   bus.dispatch();
class EventBus {
public:
    template <typename Event> using Handler = std::function<void(const Event &)>;

    // Register a non-empty handler. Do not subscribe from a running handler.
    template <typename Event> void subscribe(Handler<Event> handler)
    {
        if (!handler)
            throw std::invalid_argument("Cannot subscribe an empty event handler");
        handlers<Event>().push_back(std::move(handler));
    }

    // Call every handler of Event right now. Does nothing if nobody subscribed.
    template <typename Event> void publish(const Event &event) const
    {
        auto listEntry = _handlerLists.find(std::type_index(typeid(Event)));
        if (listEntry == _handlerLists.end())
            return;
        const auto &eventHandlers = static_cast<const HandlerList<Event> &>(*listEntry->second).handlers;
        for (const auto &handler : eventHandlers)
            handler(event);
    }

    // Keep a copy of event; its handlers run on the next dispatch.
    template <typename Event> void enqueue(Event event)
    {
        _queuedEvents.push_back([queuedEvent = std::move(event)](const EventBus &bus) { bus.publish(queuedEvent); });
    }

    // Publish queued events in the order they were enqueued, then empty the queue.
    // Events enqueued by a handler during dispatch are delivered in the same call.
    void dispatch()
    {
        for (std::size_t i = 0; i < _queuedEvents.size(); i++) {
            const auto delivery = std::move(_queuedEvents[i]);
            delivery(*this);
        }
        _queuedEvents.clear();
    }

private:
    // Type-erased base so handlers of every event type fit in one map, like ISparseArray in the Registry.
    struct IHandlerList {
        virtual ~IHandlerList() = default;
    };

    template <typename Event> struct HandlerList : IHandlerList {
        std::vector<Handler<Event>> handlers;
    };

    // The handlers of Event (create the list on first use).
    template <typename Event> std::vector<Handler<Event>> &handlers()
    {
        auto &list = _handlerLists[std::type_index(typeid(Event))];
        if (!list)
            list = std::make_unique<HandlerList<Event>>();
        return static_cast<HandlerList<Event> &>(*list).handlers;
    }

    std::unordered_map<std::type_index, std::unique_ptr<IHandlerList>> _handlerLists;
    std::vector<std::function<void(const EventBus &)>> _queuedEvents;
};

} // namespace engine::core
