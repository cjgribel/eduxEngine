#include <gtest/gtest.h>
#include <entt/meta/factory.hpp>
#include <entt/meta/meta.hpp>
#include <thread>
#include <vector>
#include <atomic>
#include <numeric>
#include <array>

using namespace entt::literals;

// A simple struct to reflect
struct MyStruct {
    int value;
    MyStruct(int v = 0) : value(v) {}
    int getValue() const { return value; }
};

// Register metadata before any tests run
namespace {
    static const auto metaRegistration = []() {
        entt::meta_factory<MyStruct>()
            .type("MyStruct"_hs)
            .ctor<int>()
            .data<&MyStruct::value>("value"_hs)
            .func<&MyStruct::getValue>("getValue"_hs);
        return 0;
        }();
}

constexpr int THREAD_COUNT = 50;
constexpr int ITERATIONS = 1000;

TEST(ThreadSafetyMeta, ConcurrentResolve) {
    std::atomic<bool> error{ false };
    std::vector<std::thread> threads;
    for (int i = 0; i < THREAD_COUNT; ++i) {
        threads.emplace_back([&]() {
            for (int j = 0; j < ITERATIONS; ++j) {
                try {
                    auto type = entt::resolve<MyStruct>();
                    if (!type) { error.store(true); }
                    if (type.id() != entt::type_hash<MyStruct>()) { error.store(true); }
                }
                catch (...) { error.store(true); }
            }
            });
    }
    for (auto& t : threads) t.join();
    ASSERT_FALSE(error.load());
}

TEST(ThreadSafetyMeta, ConcurrentMetaAnyCreation) {
    std::atomic<bool> error{ false };
    std::vector<std::thread> threads;
    for (int i = 0; i < THREAD_COUNT; ++i) {
        threads.emplace_back([&]() {
            for (int j = 0; j < ITERATIONS; ++j) {
                try {
                    entt::meta_any any{ MyStruct(j) };
                    auto ms = any.cast<MyStruct>();
                    if (ms.value != j) { error.store(true); }
                }
                catch (...) { error.store(true); }
            }
            });
    }
    for (auto& t : threads) t.join();
    ASSERT_FALSE(error.load());
}

TEST(ThreadSafetyMeta, ConcurrentFunctionInvoke) {
    std::atomic<bool> error{ false };
    std::vector<std::thread> threads;
    for (int i = 0; i < THREAD_COUNT; ++i) {
        threads.emplace_back([&]() {
            for (int j = 0; j < ITERATIONS; ++j) {
                try {
                    entt::meta_any any{ MyStruct(j) };
                    auto func = entt::resolve<MyStruct>().func("getValue"_hs);
                    auto result = func.invoke(any);
                    if (!result || result.cast<int>() != j) { error.store(true); }
                }
                catch (...) { error.store(true); }
            }
            });
    }
    for (auto& t : threads) t.join();
    ASSERT_FALSE(error.load());
}

TEST(ThreadSafetyMeta, ConcurrentFactoryUsage) {
    std::atomic<bool> error{ false };
    std::vector<std::thread> threads;
    for (int i = 0; i < THREAD_COUNT; ++i) {
        threads.emplace_back([&]() {
            for (int j = 0; j < ITERATIONS; ++j) {
                try {
                    auto any = entt::resolve<MyStruct>().construct(j);
                    auto ms = any.cast<MyStruct>();
                    if (ms.value != j) { error.store(true); }
                }
                catch (...) { error.store(true); }
            }
            });
    }
    for (auto& t : threads) t.join();
    ASSERT_FALSE(error.load());
}

TEST(ThreadSafetyMeta, ConcurrentMetaAnyModification) {
    std::atomic<bool> error{ false };
    std::vector<std::thread> threads;
    for (int i = 0; i < THREAD_COUNT; ++i) {
        threads.emplace_back([&]() {
            for (int j = 0; j < ITERATIONS; ++j) {
                try {
                    entt::meta_any any{ MyStruct(j) };
                    auto data = entt::resolve<MyStruct>().data("value"_hs);
                    data.set(any, j * 2);
                    auto ms = any.cast<MyStruct>();
                    if (ms.value != j * 2) { error.store(true); }
                }
                catch (...) { error.store(true); }
            }
            });
    }
    for (auto& t : threads) t.join();
    ASSERT_FALSE(error.load());
}

// Test with a larger struct to force out-of-line storage
struct BigStruct {
    std::array<long, 64> data;
    BigStruct(long v = 0) { data.fill(v); }
    long sum() const { return std::accumulate(data.begin(), data.end(), 0L); }
};

namespace {
    static const auto bigMetaRegistration = []() {
        entt::meta_factory<BigStruct>()
            .type("BigStruct"_hs)
            .ctor<long>()
            .func<&BigStruct::sum>("sum"_hs);
        return 0;
        }();
}

TEST(ThreadSafetyMeta, ConcurrentBigMetaAnyCreation) {
    std::atomic<bool> error{ false };
    std::vector<std::thread> threads;
    for (int i = 0; i < THREAD_COUNT; ++i) {
        threads.emplace_back([&]() {
            for (int j = 0; j < ITERATIONS; ++j) {
                try {
                    entt::meta_any any{ BigStruct(j) };
                    auto bs = any.cast<BigStruct>();
                    if (bs.data.front() != j) { error.store(true); }
                    auto func = entt::resolve<BigStruct>().func("sum"_hs);
                    auto result = func.invoke(any);
                    if (!result || result.cast<long>() != j * static_cast<long>(bs.data.size())) {
                        error.store(true);
                    }
                }
                catch (...) { error.store(true); }
            }
            });
    }
    for (auto& t : threads) t.join();
    ASSERT_FALSE(error.load());
}

// Isolate forward_as_meta and cast usage without any pool or storage
TEST(ThreadSafetyMeta, ConcurrentForwardAsMeta) {
    std::atomic<bool> error{ false };
    std::vector<std::thread> threads;
    for (int t = 0; t < THREAD_COUNT; ++t) {
        threads.emplace_back([&]() {
            for (int j = 0; j < ITERATIONS; ++j) {
                try {
                    MyStruct tmp{ static_cast<int>(j) };
                    auto any = entt::forward_as_meta(tmp);
                    auto& ms = any.cast<MyStruct&>();
                    if (ms.value != j) { error.store(true); }
                }
                catch (...) { error.store(true); }
            }
            });
    }
    for (auto& t : threads) t.join();
    ASSERT_FALSE(error.load());
}

// Test concurrent dynamic meta registration for a fresh type
struct RaceRegStruct { int x; RaceRegStruct(int v = 0) :x(v) {} };
TEST(ThreadSafetyMeta, ConcurrentMetaRegistration) {
    std::atomic<bool> error{ false };
    std::vector<std::thread> threads;

    entt::meta_ctx ctx; // Use a fresh context. Alt. entt::meta_reset();

    // Initial cold registration.
    // After this, we should be able to re-register existing types concurrently.
    std::once_flag init_flag;
    std::call_once(init_flag, [&]() {
        entt::meta_factory<RaceRegStruct>(ctx)
            .type("RaceRegStruct"_hs)
            .ctor<int>()
            .data<&RaceRegStruct::x>("x"_hs);
        });

    // Todo: also run with ThreadSanitizer to detect data-races
    for (int t = 0; t < THREAD_COUNT; ++t) {
        threads.emplace_back([&]() {
            for (int j = 0; j < ITERATIONS; ++j) {
                try {
                    entt::meta_factory<RaceRegStruct>(ctx)
                        .type("RaceRegStruct"_hs)
                        .ctor<int>()
                        .data<&RaceRegStruct::x>("x"_hs);
                }
                catch (...) { error.store(true); }
            }
            });
    }
    for (auto& t : threads) t.join();
    ASSERT_FALSE(error.load());

    // Post validation

    // 1. Type exists
    auto type = entt::resolve<RaceRegStruct>(ctx);
    ASSERT_TRUE(type);

    // 2. Constructor works
    auto any_obj = type.construct<int>(123);
    ASSERT_TRUE(any_obj);
    auto* ptr = any_obj.try_cast<RaceRegStruct>();
    ASSERT_NE(ptr, nullptr);
    ASSERT_EQ(ptr->x, 123);

    // 3. Data member "x" exists and is readable/writable
    auto member = type.data("x"_hs);
    ASSERT_TRUE(member);

    RaceRegStruct obj{ 7 };
    entt::meta_any wrapper = entt::forward_as_meta(obj); // correct reference-like meta_any

    // Read
    auto value = member.get(wrapper);
    ASSERT_TRUE(value);
    int read = value.cast<int>();
    ASSERT_EQ(read, 7);

    // Write
    member.set(wrapper, 456);
    ASSERT_EQ(obj.x, 456);

    // 4. Idempotency sanity
    auto type2 = entt::resolve<RaceRegStruct>(ctx);
    ASSERT_TRUE(type2);
    ASSERT_EQ(type, type2);
}

// New test: shared meta_any instance mutated/read concurrently without external sync
// This is expected to fail
// TEST(ThreadSafetyMeta, ConcurrentSharedMetaAny) {
//     std::atomic<bool> error{false};
//     // Single shared any
//     entt::meta_any any{ MyStruct(0) };
//     auto data = entt::resolve<MyStruct>().data("value"_hs);
//     std::vector<std::thread> threads;
//     for(int t = 0; t < THREAD_COUNT; ++t) {
//         threads.emplace_back([&]() {
//             for(int j = 0; j < ITERATIONS; ++j) {
//                 try {
//                     // mutate and read the same any
//                     data.set(any, j);
//                     auto &ms = any.cast<MyStruct&>();
//                     if(ms.value != j) { error.store(true); }
//                 } catch(...) { error.store(true); }
//             }
//         });
//     }
//     for(auto &t : threads) t.join();
//     ASSERT_FALSE(error.load());
// }
