// storage_test.cpp
// Google Test suite for eeng::Storage

#include <gtest/gtest.h>
#include <entt/meta/factory.hpp>
#include <entt/meta/meta.hpp>
#include <vector>
#include <thread>
#include "Storage.hpp"
#include "Guid.h"

// Mock resource types: must be larger than index type (size_t) and include dynamic allocation
struct MockResource1 {
    size_t x = 0;
    std::vector<int> data;

    MockResource1() {
        data = { 10, 20, 30 };
    }
};

struct MockResource2 {
    size_t y = 0;
    std::vector<double> values;

    MockResource2() {
        values = { 1.1, 2.2, 3.3 };
    }
};

// Helper to register storage for types in EnTT meta system
template<typename T>
void assure_storage(eeng::Storage& storage) {
    storage.assure_storage<T>();
}

// Static registration of MockResource1 & MockResource2 with EnTT meta using meta_factory
static bool meta_registered = []() {
    entt::meta_factory<MockResource1>{}
    .type("MockResource1"_hs)
        .template func<&assure_storage<MockResource1>>("assure_storage"_hs);
    entt::meta_factory<MockResource2>{}
    .type("MockResource2"_hs)
        .template func<&assure_storage<MockResource2>>("assure_storage"_hs);
    return true;
    }();

class StorageTest : public ::testing::Test {
protected:
    eeng::Storage storage;
};

TEST_F(StorageTest, AddAndValidate)
{
    // Add a resource and validate the returned handle
    MockResource1 mr; mr.x = 42;
    entt::meta_any any = mr;
    auto guid = eeng::Guid::generate();

    auto handle = storage.add(any, guid);
    EXPECT_TRUE(handle.valid());
    EXPECT_TRUE(storage.validate(handle));

    // Invalid empty handle should not validate
    eeng::MetaHandle empty;
    EXPECT_FALSE(empty);
    EXPECT_FALSE(storage.validate(empty));
}

TEST_F(StorageTest, GetAndMutateNonConst)
{
    // Add and then get non-const, mutate value
    MockResource1 mr; mr.x = 1;
    entt::meta_any any = mr;
    auto guid = eeng::Guid::generate();
    auto handle = storage.add(any, guid);

    auto meta = storage.get(handle);
    EXPECT_EQ(meta.base().policy(), entt::any_policy::ref);
    auto& val = meta.cast<MockResource1&>();
    val.x = 5;
    val.data.push_back(40);

    // Confirm internal state updated via try_get
    auto opt = storage.try_get(handle);
    ASSERT_TRUE(opt.has_value());
    auto& val2 = opt->cast<MockResource1&>();
    EXPECT_EQ(val2.x, 5);
    EXPECT_EQ(val2.data.back(), 40);
}

TEST_F(StorageTest, GetConstPolicyAndValue)
{
    // Add and mutate, then get via const storage
    MockResource1 mr; mr.x = 7;
    entt::meta_any any = mr;
    auto guid = eeng::Guid::generate();
    auto handle = storage.add(any, guid);

    // Mutate via non-const get
    storage.get(handle).cast<MockResource1&>().x = 9;

    // Now fetch as const storage
    const auto& cstorage = storage;
    auto meta = cstorage.get(handle);
    EXPECT_EQ(meta.base().policy(), entt::any_policy::cref);
    const auto& cval = meta.cast<const MockResource1&>();
    EXPECT_EQ(cval.x, 9);
}

TEST_F(StorageTest, TryGetInvalid)
{
    // try_get on invalid handle returns nullopt
    eeng::MetaHandle bad;
    EXPECT_FALSE(storage.try_get(bad).has_value());
}

TEST_F(StorageTest, RetainAndReleaseReferenceCount)
{
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
    EXPECT_FALSE(storage.try_get(handle).has_value());
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
    EXPECT_THROW(storage.get(wrong), std::runtime_error);
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

TEST_F(StorageTest, MultiTypeStorage)
{
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

    auto& val1 = storage.get(h1).cast<MockResource1&>();
    auto& val2 = storage.get(h2).cast<MockResource2&>();
    EXPECT_EQ(val1.x, 100u);
    EXPECT_EQ(val2.y, 200u);
}

TEST_F(StorageTest, RetainInvalidThrows)
{
    eeng::MetaHandle bad;
    EXPECT_THROW(storage.retain(bad), std::runtime_error);
}

TEST_F(StorageTest, ReleaseInvalidThrows)
{
    eeng::MetaHandle bad;
    EXPECT_THROW(storage.release(bad), std::runtime_error);
}

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
    const int N = 10;
    std::vector<eeng::Guid> guids(N);
    for (auto& g : guids) g = eeng::Guid::generate();

    std::vector<std::thread> threads;
    std::vector<bool> results(N, false);

    for (int i = 0; i < N; ++i) {
        threads.emplace_back([this, i, &guids, &results] {
            try {
                MockResource1 mr; mr.x = size_t(i);
                entt::meta_any any = mr;
                auto h = storage.add(any, guids[i]);
                if (!storage.validate(h)) return;
                auto& val = storage.get(h).cast<MockResource1&>();
                if (val.x != size_t(i)) return;
                storage.release(h);
                if (storage.validate(h)) return;
                results[i] = true;
            }
            catch (...) {
                /* swallow: results[i] stays false */
            }
            });
    }

    for (auto& t : threads) t.join();
    for (int i = 0; i < N; ++i) {
        EXPECT_TRUE(results[i]) << "Thread " << i << " failed";
    }
}

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