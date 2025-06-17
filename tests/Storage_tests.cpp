// storage_test.cpp
// Google Test suite for eeng::Storage

#include <gtest/gtest.h>
#include <entt/meta/factory.hpp>
#include <entt/meta/meta.hpp>
#include <vector>
#include "Storage.hpp"
#include "Guid.h"

// Mock resource type: must be larger than index type (size_t) and include dynamic allocation
struct MockResource1 {
    size_t x = 0;
    std::vector<int> data;

    MockResource1() {
        // initialize dynamic member with some values
        data = { 10, 20, 30 };
    }
};

// Helper to register storage for MockResource1 in entt meta system
template<typename T>
void assure_storage(eeng::Storage& storage) {
    storage.assure_storage<T>();
}

// Static registration of MockResource1 with EnTT meta using meta_factory
static bool meta_registered = []() {
    entt::meta_factory<MockResource1>{}
        .type("MockResource1"_hs)
        .template func<&assure_storage<MockResource1>>("assure_storage"_hs);
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
    auto &val = meta.cast<MockResource1&>();
    val.x = 5;
    val.data.push_back(40);

    // Confirm internal state updated via try_get
    auto opt = storage.try_get(handle);
    ASSERT_TRUE(opt.has_value());
    auto &val2 = opt->cast<MockResource1&>();
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
    const auto &cstorage = storage;
    auto meta = cstorage.get(handle);
    EXPECT_EQ(meta.base().policy(), entt::any_policy::cref);
    const auto &cval = meta.cast<const MockResource1&>();
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

// Additional tests could include:
// - Removing resources explicitly via a public API (if added)
// - Multi-type storage, ensuring separate pools
// - Concurrency safety (locking behavior)

// Not needed since we link with gtest_main 
// int main(int argc, char **argv) {
//     ::testing::InitGoogleTest(&argc, argv);
//     return RUN_ALL_TESTS();
// }