// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#ifndef EventQueue_h
#define EventQueue_h

#include <any>
#include <functional>
#include <typeindex>
#include <mutex>
#include <unordered_map>
#include <algorithm>
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

class EventQueue
{
    using CallbackType = std::function<void(const std::any&)>;

    // Callback registry:
    // - Call register_callback() only during single-threaded initialization.
    // - After init, the registry is read-only and may be read concurrently.
    std::unordered_map<std::type_index, std::vector<CallbackType>> callback_map;

    // Event queue + its mutex
    std::vector<std::any> events;
    mutable std::mutex events_mutex;

    // Invoke all callbacks for a single event
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

    /// Enqueue an event (thread-safe)
    template<typename EventType>
    bool enqueue_event(EventType&& event) noexcept
    {
        try {
            std::lock_guard lk(events_mutex);
            events.emplace_back(
                std::in_place_type<std::decay_t<EventType>>,
                std::forward<EventType>(event)
            );
            return true;
        }
        catch (...) {
            return false;
        }
    }

    /// Dispatch a single event immediately
    template<class EventType>
    void dispatch(const EventType& event)
    {
        dispatch_event(std::any(event));
    }

    /// Dispatch (and remove) only events of type EventType
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

    /// Dispatch and remove all remaining events
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

    /// Check if any events are pending
    bool has_pending_events()
    {
        std::lock_guard lock(events_mutex);
        return !events.empty();
    }

    /// Clear all pending events
    void clear()
    {
        std::lock_guard lock(events_mutex);
        events.clear();
    }

    /// Registers a callback for a specific event type.
    /// Must only be called during initialization, before multiple threads
    /// start producing or dispatching events.
    /// After initialization, the callback registry is read-only and may
    /// be accessed concurrently for dispatch.
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