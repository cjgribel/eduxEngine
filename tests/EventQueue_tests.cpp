#include <gtest/gtest.h>
#include "EventQueue.h"  // now defines EventQueue
#include <atomic>
#include <thread>
#include <vector>

// Define a simple event type
struct TestEvent {
    int value;
};

struct OtherEvent {
    std::string message;
};

// Basic immediate dispatch test
TEST(EventQueueBasic, ImmediateDispatch) {
    EventQueue queue;
    std::atomic<bool> called{false};
    int received = 0;

    queue.register_callback([&](const TestEvent& e) {
        called = true;
        received = e.value;
    });

    TestEvent ev{42};
    queue.dispatch(ev);

    EXPECT_TRUE(called);
    EXPECT_EQ(received, 42);
}

// Test queued dispatch_all_events
TEST(EventQueueBasic, QueuedDispatchAll) {
    EventQueue queue;
    std::vector<int> results;

    queue.register_callback([&](const TestEvent& e) {
        results.push_back(e.value);
    });

    // Enqueue multiple events
    for (int i = 0; i < 5; ++i) {
        queue.enqueue_event(TestEvent{i});
    }

    EXPECT_TRUE(queue.has_pending_events());

    queue.dispatch_all_events();

    EXPECT_FALSE(queue.has_pending_events());
    EXPECT_EQ(results.size(), 5);
    for (int i = 0; i < 5; ++i) {
        EXPECT_EQ(results[i], i);
    }
}

// Test dispatch_event_type extracts only matching events
TEST(EventQueueBasic, DispatchEventType) {
    EventQueue queue;
    std::vector<int> testResults;
    std::vector<std::string> otherResults;

    queue.register_callback([&](const TestEvent& e) {
        testResults.push_back(e.value);
    });
    queue.register_callback([&](const OtherEvent& e) {
        otherResults.push_back(e.message);
    });

    // Enqueue mix of events
    queue.enqueue_event(TestEvent{1});
    queue.enqueue_event(OtherEvent{"foo"});
    queue.enqueue_event(TestEvent{2});
    queue.enqueue_event(OtherEvent{"bar"});

    queue.dispatch_event_type<TestEvent>();

    // Only TestEvent should have been dispatched and removed
    EXPECT_EQ(testResults.size(), 2);
    EXPECT_EQ(testResults[0], 1);
    EXPECT_EQ(testResults[1], 2);
    EXPECT_TRUE(queue.has_pending_events());

    // Now dispatch all remaining (OtherEvent)
    queue.dispatch_all_events();

    EXPECT_EQ(otherResults.size(), 2);
    EXPECT_EQ(otherResults[0], "foo");
    EXPECT_EQ(otherResults[1], "bar");
    EXPECT_FALSE(queue.has_pending_events());
}

// Concurrency test: multiple threads enqueuing events
TEST(EventQueueConcurrency, ConcurrentEnqueue) {
    EventQueue queue;
    const int numThreads = 8;
    const int eventsPerThread = 1000;
    std::atomic<int> counter{0};

    queue.register_callback([&](const TestEvent& e) {
        counter.fetch_add(1, std::memory_order_relaxed);
    });

    // Launch threads to enqueue events concurrently
    std::vector<std::thread> threads;
    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([&]() {
            for (int i = 0; i < eventsPerThread; ++i) {
                queue.enqueue_event(TestEvent{i});
            }
        });
    }

    // Wait for all enqueuers to finish
    for (auto& th : threads) th.join();

    // Now dispatch all events on main thread
    queue.dispatch_all_events();

    EXPECT_EQ(counter.load(), numThreads * eventsPerThread);
}
