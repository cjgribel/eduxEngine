// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once
#include "IExecutor.hpp"
#include <functional>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>

namespace eeng
{
    /// A strand/serializing adapter that runs posted tasks one-at-a-time in FIFO order,
    /// using an *upstream* executor for actual execution. Thread-safe.
    class SerialExecutor : public IExecutor
    {
    public:
        using Fn = std::function<void()>;

        explicit SerialExecutor(IExecutor& upstream) noexcept
            : upstream_(upstream)
        {
        }

        SerialExecutor(const SerialExecutor&) = delete;
        SerialExecutor& operator=(const SerialExecutor&) = delete;

        // IExecutor
        void post(Fn fn) override
        {
            {
                std::lock_guard<std::mutex> lk(mutex_);
                task_queue_.push(std::move(fn));
                queued_count_.fetch_add(1, std::memory_order_relaxed);
            }
            schedule_worker_once();
        }

        // --- Introspection (thread-safe) -------------------------------------

        /// True while the strand's worker loop is executing tasks
        bool running() const noexcept
        {
            return running_.load(std::memory_order_relaxed);
        }

        /// Number of queued tasks, not including the task currently executing
        std::size_t queued() const noexcept
        {
            return queued_count_.load(std::memory_order_relaxed);
        }

        /// True if a task is running or there are tasks queued
        bool is_busy() const noexcept
        {
            return running() || queued() > 0;
        }

        /// Block until no task is running and the queue is empty
        void wait_idle()
        {
            std::unique_lock<std::mutex> lk(mutex_);
            cv_idle_.wait(lk, [&]
                {
                    return !running_.load(std::memory_order_relaxed) && task_queue_.empty();
                });
        }

    private:
        void schedule_worker_once()
        {
            bool expect = false;
            if (worker_scheduled_.compare_exchange_strong(expect, true, std::memory_order_acq_rel))
            {
                // Schedule a single drain on the upstream executor
                upstream_.post([this] { this->drain(); });
            }
        }

        void drain()
        {
            running_.store(true, std::memory_order_relaxed);

            for (;;)
            {
                Fn f;
                {
                    std::lock_guard<std::mutex> lk(mutex_);
                    if (task_queue_.empty()) 
                    {
                        // About to go idle
                        running_.store(false, std::memory_order_relaxed);
                        worker_scheduled_.store(false, std::memory_order_release);
                        cv_idle_.notify_all();

                        // Race check: tasks may have been enqueued after we saw empty()
                        if (!task_queue_.empty()) 
                        {
                            // Try to re-schedule ourselves to continue draining.
                            bool expect = false;
                            if (worker_scheduled_.compare_exchange_strong(expect, true, std::memory_order_acq_rel)) {
                                running_.store(true, std::memory_order_relaxed);
                                continue; // re-enter loop and keep draining
                            }
                        }
                        return;
                    }
                    f = std::move(task_queue_.front());
                    task_queue_.pop();
                    queued_count_.fetch_sub(1, std::memory_order_relaxed);
                }

                // Execute task outside the lock so new tasks can be enqueued
                try {
                    f();
                }
                catch (...) {
                    // Swallow exceptions ...
                }
            }
        }

    private:
        IExecutor& upstream_;

        // Queue & coordination
        mutable std::mutex mutex_;
        std::condition_variable cv_idle_;
        std::queue<Fn> task_queue_;

        // State flags / counters
        std::atomic<bool>   worker_scheduled_{ false }; // ensures only one drain() is posted
        std::atomic<bool>   running_{ false };          // true while drain() is active
        std::atomic<size_t> queued_count_{ 0 };         // approximate size of task_queue_ (excludes running)
    };

} // namespace eeng
