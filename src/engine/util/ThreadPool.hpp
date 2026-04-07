// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#ifndef THREADPOOL_HPP
#define THREADPOOL_HPP

#include "IExecutor.hpp"
#include <future>
#include <queue>
#include <functional>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <iostream>
#include <type_traits>
#include <utility>
#include <stdexcept>
#include <memory>

class ThreadPool : public IExecutor
{
public:
    explicit ThreadPool(size_t thread_count = std::thread::hardware_concurrency(),
        std::shared_ptr<std::atomic<bool>> shutdown_flag = {});
    ~ThreadPool();

    // IExecutor
    void post(std::function<void()> fn) override;

    template <typename Func>
    auto queue_task(Func task) -> std::future<std::invoke_result_t<Func>>;

    size_t nbr_threads() const;
    size_t nbr_working_threads() const;
    size_t nbr_idle_threads() const;
    size_t task_queue_size() const;
    bool is_task_queue_empty() const;

private:
    bool shutdown_requested() const noexcept
    {
        return shutdown_flag_ && shutdown_flag_->load(std::memory_order_relaxed);
    }

    std::vector<std::thread> workers;
    std::condition_variable cv;

    std::queue<std::function<void()>> task_queue;
    mutable std::mutex queue_mutex;

    std::atomic<bool> stop{ false };
    std::atomic<size_t> working_count{ 0 };

    const size_t thread_count;
    std::shared_ptr<std::atomic<bool>> shutdown_flag_;
};

// Template definition remains in the header
template <typename Func>
auto ThreadPool::queue_task(Func task) -> std::future<std::invoke_result_t<Func>>
{
    using ResultType = std::invoke_result_t<Func>;

    if (shutdown_requested() || stop.load(std::memory_order_relaxed))
    {
        std::promise<ResultType> prom;
        prom.set_exception(std::make_exception_ptr(
            std::runtime_error("ThreadPool is shutting down")));
        return prom.get_future();
    }

    auto packaged_task = std::make_shared<std::packaged_task<ResultType()>>(std::move(task));
    auto future = packaged_task->get_future();

    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        task_queue.emplace([packaged_task]()
            {
                std::invoke(*packaged_task);
            });
    }
    cv.notify_one();
    return future;
}

#endif // THREAD_POOL_HPP
