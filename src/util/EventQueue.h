#ifndef EventDispatcher_h
#define EventDispatcher_h

#include <any>
#include <functional>
#include <typeindex>
#include <unordered_map>
#include <vector>
#include <iostream>

namespace internal
{
    template <typename T>
    struct get_signature;

    template <typename R, typename Arg>
    struct get_signature<std::function<R(Arg)>>
    {
        using FunctionType = R(Arg);
        using ReturnType = R;
        using ArgType = Arg;
    };
}

// EventDispatcher class
class EventQueue
{
    using CallbackType = std::function<void(const std::any&)>;

    // callback storage (assumed single‑threaded after init)
    std::unordered_map<std::type_index, std::vector<CallbackType>> callback_map;

    // event queue + its mutex
    std::vector<std::any> events;
    mutable std::mutex events_mutex;

    // invoke all callbacks for a single event
    void dispatch_event(const std::any& eventData) const
    {
        std::type_index type = std::type_index(eventData.type());
        auto it = callback_map.find(type);
        if (it != callback_map.end())
        {
            for (const auto& callback : it->second)
            {
                callback(eventData);
            }
        }
    }

public:

    /// Thread‑safe: enqueue from any thread
    template<typename EventType>
    void enqueue_event(EventType&& event)
    {
        std::lock_guard lock(events_mutex);
        events.emplace_back(std::make_any<EventType>(std::forward<EventType>(event)));
    }

    /// Dispatches an event immediately.
    template<class EventType>
    void dispatch(const EventType& event)
    {
        dispatch_event(std::any(event));
    }

    /// Dispatch (and remove) only events of type EventType
#if 1
    template<class EventType>
    void dispatch_event_type()
    {
        std::vector<std::any> work;
        {
            std::lock_guard lock(events_mutex);
            // Partition into "to dispatch" vs "to keep"
            auto it = std::stable_partition(
                events.begin(), events.end(),
                [](const std::any& e) { return e.type() != typeid(EventType); }
            );
            // [ events.begin(), it ) = non-matching (keep)
            // [ it, events.end() ) = matching   (dispatch)
            // Move the matching ones out:
            work.assign(std::make_move_iterator(it),
                std::make_move_iterator(events.end()));
            // Erase them from the queue:
            events.erase(it, events.end());
        }
        // Now dispatch without holding the lock:
        for (auto& e : work)
            dispatch_event(e);
    }
#else
    template<class EventType>
    void dispatch_event_type()
    {
        std::type_index type = typeid(EventType);
        for (const auto& event : events)
        {
            if (event.type() == type)
            {
                dispatch_event(event);
            }
        }
    }
#endif

    /// Dispatch and remove all remaining events
#if 1
    void dispatch_all_events()
    {
        // swap out the whole queue under lock, then process unlocked
        std::vector<std::any> work;
        {
            std::lock_guard lock(events_mutex);
            work.swap(events);
        }
        for (auto& e : work)
            dispatch_event(e);
    }
#else
    void dispatch_all_events()
    {
        for (const auto& event : events)
        {
            dispatch_event(event);
        }
        events.clear();
    }
#endif

    /// Check if any events are pending
    bool has_pending_events()
    {
        std::lock_guard lock(events_mutex);
        return events.size();
    }

    /// Clear all pending events
    void clear()
    {
        std::lock_guard lock(events_mutex);
        events.clear();
    }

    /// Register a callback (assumed single‑threaded after init)
    template<typename Callable>
    void register_callback(Callable&& callable)
    {
        using EventType = std::decay_t<typename internal::get_signature<decltype(std::function{ callable }) > ::ArgType > ;

        std::function<void(const EventType&)> callback{ std::forward<Callable>(callable) };

        auto wrapped_callback = [callback](const std::any& eventData) {
            if (auto castedEvent = std::any_cast<EventType>(&eventData))
            {
                callback(*castedEvent);
            }
            else
            {
                std::cerr << "Error: Mismatched event type detected." << std::endl;
            }
            };

        callback_map[typeid(EventType)].push_back(wrapped_callback);
    }
};

#endif