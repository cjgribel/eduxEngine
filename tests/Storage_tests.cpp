// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include <gtest/gtest.h>
#include <entt/meta/factory.hpp>
#include <entt/meta/meta.hpp>
#include <vector>
#include <thread>
#include "Storage.hpp"
#include "Guid.h"

struct MockResource1 {
    size_t x = 0;
    std::vector<int> data;

    MockResource1() {
        data = { 10, 20, 30 };
    }

    bool operator==(const MockResource1& other) const
    {
        return x == other.x && data == other.data;
    }
};

std::ostream& operator<<(std::ostream& os, const MockResource1& mr)
{
    return os << mr.x;
}

struct MockResource2
{
    size_t y = 0;
    std::vector<double> values;

    MockResource2()
    {
        values = { 1.1, 2.2, 3.3 };
    }

    bool operator==(const MockResource2& other) const
    {
        return y == other.y && values == other.values;
    }
};

// Helper to register storage for types in EnTT meta system
template<typename T>
void assure_storage(eeng::Storage& storage) {
    storage.assure_storage<T>();
}

// Static registration of MockResource1 & MockResource2 with EnTT meta using meta_factory
// static bool meta_registered = []() {
//     entt::meta_factory<MockResource1>{}
//     .type("MockResource1"_hs)
//         .template func<&assure_storage<MockResource1>>("assure_storage"_hs);
//     entt::meta_factory<MockResource2>{}
//     .type("MockResource2"_hs)
//         .template func<&assure_storage<MockResource2>>("assure_storage"_hs);
//     return true;
//     }();

class StorageTest : public ::testing::Test
{
protected:
    eeng::Storage storage;

    static void SetUpTestSuite()
    {
        entt::meta_factory<MockResource1>{}
        .type("MockResource1"_hs)
            .template func<&assure_storage<MockResource1>>("assure_storage"_hs);

        entt::meta_factory<MockResource2>{}
        .type("MockResource2"_hs)
            .template func<&assure_storage<MockResource2>>("assure_storage"_hs);
    }
};

TEST_F(StorageTest, AddAndValidate) {
    // Common setup
    MockResource1 mr; mr.x = 42;
    auto guid1 = eeng::Guid::generate();
    auto guid2 = eeng::Guid::generate();

    // ————————————————
    // 1) lvalue‐meta_any overload
    // ————————————————
    entt::meta_any any = mr;                 // lvalue
    auto handle1 = storage.add(any, guid1);
    EXPECT_TRUE(handle1.valid());
    EXPECT_TRUE(storage.validate(handle1));

    // ————————————————
    // 2) rvalue‐meta_any overload
    // ————————————————
    {
        // create a temporary meta_any and move it into add()
        auto handle2 = storage.add(entt::forward_as_meta(mr), guid2);
        EXPECT_TRUE(handle2.valid());
        EXPECT_TRUE(storage.validate(handle2));
    }

    // ————————————————
    // 3) the empty‐handle case
    // ————————————————
    eeng::MetaHandle empty;
    EXPECT_FALSE(empty);
    EXPECT_FALSE(storage.validate(empty));
}

TEST_F(StorageTest, GetAndMutateNonConst) {
    // Add and then get non-const, mutate value
    MockResource1 mr; mr.x = 1;
    entt::meta_any any = mr;
    auto guid = eeng::Guid::generate();
    auto handle = storage.add(any, guid);

    auto meta = storage.get_meta_ref(handle);
    EXPECT_EQ(meta.base().policy(), entt::any_policy::ref);
    auto& val = meta.cast<MockResource1&>();
    val.x = 5;
    val.data.push_back(40);

    // Confirm internal state updated via try_get
    auto opt = storage.try_get_meta_ref(handle);
    ASSERT_TRUE(opt.has_value());
    auto& val2 = opt->cast<MockResource1&>();
    EXPECT_EQ(val2.x, 5);
    EXPECT_EQ(val2.data.back(), 40);
}

TEST_F(StorageTest, GetConstPolicyAndValue) {
    // Add and mutate, then get via const storage
    MockResource1 mr; mr.x = 7;
    entt::meta_any any = mr;
    auto guid = eeng::Guid::generate();
    auto handle = storage.add(any, guid);

    // Mutate via non-const get
    storage.get_meta_ref(handle).cast<MockResource1&>().x = 9;

    // Now fetch as const storage
    const auto& cstorage = storage;
    auto meta = cstorage.get_meta_ref(handle);
    EXPECT_EQ(meta.base().policy(), entt::any_policy::cref);
    const auto& cval = meta.cast<const MockResource1&>();
    EXPECT_EQ(cval.x, 9);
}

TEST_F(StorageTest, TryGetInvalid) {
    // try_get on invalid handle returns nullopt
    eeng::MetaHandle bad;
    EXPECT_FALSE(storage.try_get_meta_ref(bad).has_value());
}

TEST_F(StorageTest, RetainAndReleaseReferenceCount) {
    // Test that retain and release update ref-count and auto-remove
    MockResource1 mr;
    entt::meta_any any = mr;
    auto guid = eeng::Guid::generate();
    auto handle = storage.add(any, guid);

    // Initial ref from add -> count 1, retain -> 2
    EXPECT_EQ(storage.retain(handle), 2u);
    // Release once -> 1
    EXPECT_EQ(storage.release(handle), 1u);
    // Release second -> 0 and resource removed
    EXPECT_EQ(storage.release(handle), 0u);

    // Now handle should be invalid
    EXPECT_FALSE(storage.validate(handle));
    EXPECT_FALSE(storage.try_get_meta_ref(handle).has_value());
}

TEST_F(StorageTest, TypeMismatchThrows) {
    // Creating a handle for MockResource1 but changing its type
    MockResource1 mr;
    entt::meta_any any = mr;
    auto guid = eeng::Guid::generate();
    auto handle = storage.add(any, guid);

    // Create wrong handle by altering type
    eeng::MetaHandle wrong = handle;
    wrong.type = entt::resolve<int>();

    // Expect std::runtime_error("Pool not found") when type mismatches
    EXPECT_THROW(storage.get_meta_ref(wrong), std::runtime_error);
}

TEST_F(StorageTest, VersionInvalidAfterRemoval)
{
    // Remove via release to zero and confirm version invalid
    MockResource1 mr;
    entt::meta_any any = mr;
    auto guid = eeng::Guid::generate();
    auto handle = storage.add(any, guid);

    // Remove all references
    storage.release(handle);

    // Version bump on removal should make handle invalid
    EXPECT_FALSE(storage.validate(handle));
}

TEST_F(StorageTest, MultiTypeStorage) {
    MockResource1 mr1; mr1.x = 100;
    entt::meta_any any1 = mr1;
    auto guid1 = eeng::Guid::generate();
    auto h1 = storage.add(any1, guid1);

    MockResource2 mr2; mr2.y = 200;
    entt::meta_any any2 = mr2;
    auto guid2 = eeng::Guid::generate();
    auto h2 = storage.add(any2, guid2);

    EXPECT_TRUE(storage.validate(h1));
    EXPECT_TRUE(storage.validate(h2));

    auto& val1 = storage.get_meta_ref(h1).cast<MockResource1&>();
    auto& val2 = storage.get_meta_ref(h2).cast<MockResource2&>();
    EXPECT_EQ(val1.x, 100u);
    EXPECT_EQ(val2.y, 200u);
}

TEST_F(StorageTest, RetainInvalidThrows) {
    eeng::MetaHandle bad;
    EXPECT_THROW(storage.retain(bad), std::runtime_error);
}

TEST_F(StorageTest, ReleaseInvalidThrows) {
    eeng::MetaHandle bad;
    EXPECT_THROW(storage.release(bad), std::runtime_error);
}

#if 1
/**
 * ConcurrencySafety (by o4-mini-high)
 *
 * Methodology:
 * Spawn N threads, each performing a full add/get/release cycle on Storage
 * to verify thread-safety under concurrent use. Each worker thread:
 *  1. Creates its own MockResource1 and unique GUID.
 *  2. Calls storage.add(), storage.validate(), storage.get(), and storage.release().
 *  3. Verifies that the handle is valid before and invalid after release.
 *
 * Results are reported via GTest assertions inside each thread.
 */
TEST_F(StorageTest, ConcurrencySafety) {

    std::mutex m;
    for (int i = 0; i < 1000; i++)
    {
        storage.clear();
        // storage = eeng::Storage{};
        const int N = 50;
        std::vector<eeng::Guid> guids(N);
        for (auto& g : guids) g = eeng::Guid::generate();

        std::vector<std::thread> threads;
        std::vector<std::atomic<bool>> results(N);
        for (auto& r : results) r.store(false, std::memory_order_relaxed);
        std::vector<eeng::Handle<MockResource1>> handles(N);

        for (int i = 0; i < N; ++i)
        {
            threads.emplace_back([this, i, &guids, &results, &m, &handles] {
                try {
                    MockResource1 mr; mr.x = size_t(i);
#if 1
                    // -- Partially meta based workflow --

                    entt::meta_any any = mr;
                    auto h = storage.add(any, guids[i]);

                    auto casted = h.cast<MockResource1>();
                    EXPECT_TRUE(casted.has_value());
                    auto h_ = *casted;
                    EXPECT_TRUE(h_);
                    EXPECT_TRUE(storage.validate(h_));
                    handles[i] = h_;

                    // Thread-safe mutatation 
                    storage.modify(h_, [](MockResource1& t) { t.x = 0; });
                    storage.modify(h_, [i](MockResource1& t) { t.x = size_t(i); });

                    // Retain & release
                    storage.retain(h);
                    storage.release(h);
                    if (!storage.validate(h)) return;
#else
                    // -- Templated workflow --

                    // Thread-safe add
                    auto h = storage.add<MockResource1>(mr, guids[i]);
                    handles[i] = h;
                    // // entt::meta_any any = mr;
                    // // auto h2 = storage.add(any, guids[i]);
                    // auto h2 = storage.add(entt::forward_as_meta(mr), guids[i]);
                    // auto h = *h2.cast<MockResource1>();
                    // handles[i] = h;

                    // Thread-safe validation
                    if (!storage.validate(h)) return;

                    // Thread-safe mutatation 
                    storage.modify(h, [](MockResource1& t) { t.x = 0; });
                    storage.modify(h, [i](MockResource1& t) { t.x = size_t(i); });

                    // Thread-safe get by value
                    auto val = storage.get_val<MockResource1>(h);
                    if (val.x != size_t(i)) {
                        std::cout << "Expected (val) " << size_t(i) << " at ofs " << h.idx << ", got " << val.x << std::endl;
                        return;
                    }

                    // Thread-safe retain & release
                    storage.retain(h);
                    storage.release(h);
                    if (!storage.validate(h)) return;

                    // Can't do this: race can occur as from_ref is assigned
                    // MockResource1 from_ref = storage.get_typed_ref<MockResource1>(h);
                    // if (from_ref.x != size_t(i)) {
                    //     std::cout << "Expected (from_ref) " << size_t(i) << " at ofs " << h.idx << ", got " << from_ref.x << std::endl;
                    //     return;
                    // }

                    // (We release and validate post-test)
                    // storage.release(h);
                    //if (storage.validate(h)) return;
#endif
                    results[i].store(true, std::memory_order_relaxed);
                }
                catch (...) {
                    /* swallow: results[i] stays false */
                    std::cout << "Exception" << std::endl;
                }
                });
        }

        for (auto& t : threads) t.join();
        int c = 0;
        for (int i = 0; i < N; ++i) {
            // EXPECT_TRUE(results[i]) << "Thread " << i << " failed";
            EXPECT_TRUE(results[i].load(std::memory_order_relaxed));
            if (!results[i].load(std::memory_order_relaxed)) c++;
        }
        if (c > 0) {
            std::cout << "nbr errors " << c << std::endl;
            std::cout << storage.to_string();
            for (auto& r : results) std::cout << r << ", "; std::cout << std::endl;
            for (auto& h : handles) std::cout << h.idx << ", "; std::cout << std::endl;
        }
        // Check handles
#if 1
        for (int j = 0; j < N; j++)
        {
            // auto& h = handles[j];
            // EXPECT_TRUE(h);
            // if (!storage.validate(h))
            //     std::cout << "Invalid handle: " << h.idx << ", ver: " << h.ver << std::endl;
            // EXPECT_TRUE(storage.validate(h));
            // auto any = storage.get(h);
            // EXPECT_TRUE(any);
            // auto val = any.cast<MockResource1&>().x;
            // if (val != size_t(j)) {
            //     std::cout << "Expected " << size_t(j) << " at ofs " << h.idx << ", got " << val << std::endl;
            // }
            // std::cout << j << ", " << storage.get(h).cast<MockResource1&>().x << std::endl;

            auto& h = handles[j];
            EXPECT_EQ(j, storage.get_meta_ref(h).cast<MockResource1&>().x);
            storage.release(h);
            EXPECT_FALSE(storage.validate(h));
        }
#else
        for (int j = 0; j < N; j++)
        {
            auto& h = handles[j];
            std::cout << "handle: " << h.idx << ", ver: " << h.ver << std::endl;
            EXPECT_EQ(j, storage.get(h).cast<MockResource1&>().x);
            storage.release(h);
            EXPECT_FALSE(storage.validate(h));
        }
        std::cout << "------" << std::endl;
#endif
    }
}
#endif

TEST_F(StorageTest, HandleForGuid_Valid) {
    // Add a resource and lookup its handle by GUID
    MockResource1 mr; mr.x = 123;
    entt::meta_any any = mr;
    auto guid = eeng::Guid::generate();
    auto handle = storage.add(any, guid);

    auto lookup = storage.handle_for_guid(guid);
    ASSERT_TRUE(lookup.has_value());
    EXPECT_EQ(lookup->ofs, handle.ofs);
    EXPECT_EQ(lookup->ver, handle.ver);
    EXPECT_EQ(lookup->type, handle.type);
}

TEST_F(StorageTest, HandleForGuid_Invalid) {
    // Lookup with unknown GUID returns nullopt
    auto guid = eeng::Guid::generate(); // never added
    auto lookup = storage.handle_for_guid(guid);
    EXPECT_FALSE(lookup.has_value());
}

TEST_F(StorageTest, GuidForHandle_Valid) {
    // Add a resource and lookup its GUID by handle
    MockResource2 mr; mr.y = 456;
    entt::meta_any any = mr;
    auto guid = eeng::Guid::generate();
    auto handle = storage.add(any, guid);

    auto lookup = storage.guid_for_handle(handle);
    ASSERT_TRUE(lookup.has_value());
    EXPECT_EQ(*lookup, guid);
}

TEST_F(StorageTest, GuidForHandle_Invalid) {
    // Use an invalid handle to lookup GUID
    eeng::MetaHandle bad;
    auto lookup = storage.guid_for_handle(bad);
    EXPECT_FALSE(lookup.has_value());
}

// Not needed since we link with gtest_main 
// int main(int argc, char **argv) {
//     ::testing::InitGoogleTest(&argc, argv);
//     return RUN_ALL_TESTS();
// }