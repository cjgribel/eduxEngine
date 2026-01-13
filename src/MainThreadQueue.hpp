#include <queue>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <atomic>
#include <stdexcept>

class MainThreadQueue
{
public:
    explicit MainThreadQueue(std::atomic<bool>* shutdown_flag = nullptr)
        : owner_(std::this_thread::get_id())
        , shutdown_flag_(shutdown_flag) {
    }

    // Enqueue a task (non-blocking). Callable must be noexcept or handle its own exceptions.
    void push(std::function<void()> fn)
    {
        if (shutdown_requested())
            return;
        {
            std::lock_guard<std::mutex> lk(mtx_);
            q_.push(std::move(fn));
        }
        cv_.notify_one();
    }

    // Enqueue and wait until the task has executed on the main thread.
    template<typename F>
    auto push_and_wait(F&& fn) -> std::invoke_result_t<F&>
    {
        using R = std::invoke_result_t<F&>;

        if (std::this_thread::get_id() == owner_) {
            // Already on main thread: run inline
            if constexpr (std::is_void_v<R>) { fn(); return; }
            else { return fn(); }
        }
        if (shutdown_requested())
            throw std::runtime_error("MainThreadQueue is shutting down");

        // otherwise enqueue + wait (Fix 1 or Fix 2 here)
        auto prom = std::make_shared<std::promise<R>>();
        auto fut = prom->get_future();

        push([prom, f = std::forward<F>(fn)]() mutable {
            try {
                if constexpr (std::is_void_v<R>) { f(); prom->set_value(); }
                else { prom->set_value(f()); }
            }
            catch (...) {
                prom->set_exception(std::current_exception());
            }
            });

        if constexpr (std::is_void_v<R>) { fut.get(); return; }
        else { return fut.get(); }
    }

    // Called by the main thread, once per frame or more often.
    void execute_all()
    {
        assert(std::this_thread::get_id() == owner_);

        std::queue<std::function<void()>> local;
        {
            std::lock_guard<std::mutex> lk(mtx_);
            local.swap(q_);
        }

        while (!local.empty())
        {
            auto fn = std::move(local.front());
            local.pop();
            try { fn(); }
            catch (...) { /* log and continue */ }
        }
    }

    // Optional: blocking wait
    void wait_for_work()
    {
        std::unique_lock<std::mutex> lk(mtx_);
        cv_.wait(lk, [&] { return !q_.empty(); });
    }

    bool empty() const
    {
        std::lock_guard<std::mutex> lk(mtx_);
        return q_.empty();
    }

private:
    bool shutdown_requested() const noexcept
    {
        return shutdown_flag_ && shutdown_flag_->load(std::memory_order_relaxed);
    }

    std::thread::id         owner_;
    mutable std::mutex      mtx_;
    std::condition_variable  cv_;
    std::queue<std::function<void()>> q_;
    std::atomic<bool>*      shutdown_flag_ = nullptr;
};
