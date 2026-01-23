// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once
#include <cstdint>
#include <limits>
#include <vector>
#include <unordered_map>
#include <type_traits> // for std::is_copy_constructible etc.
#include <typeindex>
#include <memory>
#include <string>
#include <optional>
#include <mutex>
#include <shared_mutex>
#include <utility>
#include <stdexcept>

#include "entt/entt.hpp"
#include "MetaLiterals.h"
#include "Handle.h"
#include "Guid.h"
#include "PoolAllocatorTFH.h"

// -----------------------------------------------------------------------------
// Storage overview
//
// Storage is a type-erased, handle-based asset container. It manages a pool per
// asset type (T) and provides:
//  - O(1) add/remove/access via Handle<T> and MetaHandle (runtime-typed).
//  - GUID <-> Handle mapping for asset lookups by ID.
//  - Versioning for stale-handle protection (handle index reuse safety).
//  - Optional EnTT meta access for runtime-typed inspection/modification.
//
// Pool IDs:
//  - Storage keys pools by entt::meta_type::id().
//  - This equals entt::type_hash<T>() unless the type was registered with
//    entt::meta_factory<T>().type("...").
//  - Custom string IDs (if any) are not used by Storage.
//
// What it does:
//  - Stores assets of arbitrary types in per-type pools (PoolAllocatorTFH).
//  - Returns stable handles (index + version) for lookups.
//  - Validates handles against a per-pool VersionMap.
//  - Lets you query by GUID.
//
// What it does NOT do:
//  - No reference counting or ownership policy.
//  - No persistence/serialization.
//  - Not intended for ECS component storage (EnTT registry owns that).
//
// Threading model:
//  - storage_mutex (shared_mutex) protects the pool map only.
//    * Shared lock for lookups; unique lock only when creating a new pool.
//  - Each Pool<T> has its own mutex; operations on different types can run
//    concurrently.
//  - PoolAllocatorTFH may relocate elements on growth; therefore any operation
//    that holds references into a pool must keep the pool mutex locked for the
//    duration of that access (see read/modify/with_ref/with_meta_ref).
//  - Callbacks run while holding the pool lock; avoid re-entering Storage or
//    taking other pool locks inside callbacks to prevent deadlocks.
//
// Policies/assumptions:
//  - Pools are added but never removed; pool pointers remain valid.
//  - EnTT meta registration happens during init (not concurrently with use).
//  - Handles are invalidated on remove via VersionMap; validate() is required
//    when handles may be stale.
//  - Storage must not be moved while other threads may access it.
// -----------------------------------------------------------------------------

namespace eeng
{
    struct MetaHandle
    {
        handle_idx_type ofs;
        handle_ver_type ver;
        entt::meta_type type = {};

        MetaHandle()
            : ofs(handle_idx_null), ver(handle_ver_null) {
        }

        template<typename T>
        MetaHandle(Handle<T> h)
            : ofs(h.idx), ver(h.ver), type(entt::resolve<T>()) {
        }

        bool valid() const noexcept {
            return ofs != handle_idx_null
                //&& ver != handle_ver_null
                && static_cast<bool>(type);
        }

        bool empty() const {
            return !valid();
        }

        auto operator!() const noexcept { return !valid(); }
        explicit operator bool() const noexcept { return valid(); }

        template<typename T>
        std::optional<Handle<T>> cast() const
        {
            // ensure the run‐time type matches
            if (type != entt::resolve<T>()) {
                return std::nullopt;
            }
            return Handle<T>{ ofs, ver };
        }

        bool operator==(const MetaHandle& other) const {
            return ofs == other.ofs && ver == other.ver && type == other.type;
        }
    };
} // namespace eeng

namespace std {
    template<>
    struct hash<eeng::MetaHandle>
    {
        size_t operator()(eeng::MetaHandle const& m) const noexcept
        {
            return ::hash_combine(m.ofs, m.ver, m.type.id());
        }
    };
}

namespace eeng
{
    class VersionMap
    {
        std::vector<handle_ver_type> versions;

    public:
        VersionMap() = default;

        // Assign version to handle (auto‐resizing internally)
        template<typename T>
        void assign_version(Handle<T>& handle)
        {
            assert(handle);
            auto idx = handle.idx;
            if (idx >= versions.size())
                versions.resize(idx + 1, handle_ver_null);

            auto& slot = versions[idx];
            if (slot == handle_ver_null)
                handle.ver = slot = 0;
            else
                handle.ver = slot;
        }

        // Validate that a handle’s version matches what we have
        template<typename T>
        bool validate(const Handle<T>& handle) const
        {
            if (handle.ver == handle_ver_null) return false;
            auto idx = handle.idx;
            if (idx >= versions.size()) return false;
            return handle.ver == versions[idx];
        }

        // Bump the version (e.g. on removal)
        template<typename T>
        void remove(const Handle<T>& handle)
        {
            assert(handle);
            auto idx = handle.idx;
            assert(idx < versions.size());
            ++versions[idx];
        }

        // Return a comma-separated list of all versions
        std::string to_string() const
        {
            std::ostringstream oss;
            for (auto v : versions)
                oss << v << ", ";
            std::string s = oss.str();
            if (!s.empty())
                s.resize(s.size() - 2);  // drop trailing ", "
            return s;
        }
    };

    struct ValidationError : std::runtime_error {
        using std::runtime_error::runtime_error;
    };

    class Storage
    {
        class IPool
        {
        public:
            virtual ~IPool() = default;
            virtual MetaHandle add(const Guid& guid, const entt::meta_any& data) = 0;
            virtual MetaHandle add(const Guid& guid, entt::meta_any&& data) = 0;

            virtual void remove_now(const MetaHandle& handle) = 0;

            // --- Meta typed modify (locked) -----------------------------------
            // Invoke a callback with a meta_ref while holding the pool mutex.
            virtual void with_meta_ref(const MetaHandle& mh, const std::function<void(entt::meta_any&)>& visitor) = 0;
            virtual void with_meta_ref(const MetaHandle& mh, const std::function<void(entt::meta_any&)>& visitor) const = 0;

            virtual size_t element_size() const noexcept = 0;
            virtual size_t count_free() const noexcept = 0;
            virtual size_t capacity() const noexcept = 0;
            virtual bool valid(const MetaHandle& mh) const noexcept = 0;
            virtual void clear() noexcept = 0;

            virtual std::optional<MetaHandle> handle_for_guid(const Guid& guid) const noexcept = 0;
            virtual std::optional<Guid> guid_for_handle(const MetaHandle& mh) const noexcept = 0;

            virtual void visit_any(const std::function<void(entt::meta_any)>& visitor) const noexcept = 0;
            virtual void visit_any(const std::function<void(entt::meta_any)>& visitor) noexcept = 0;
            virtual std::string to_string() const = 0;
        };

        template<typename T>
        class Pool : public IPool
        {
            // using Handle = Handle<T>;
            friend class Storage;

            PoolAllocatorTFH<T> m_pool;

            VersionMap m_versions;

            std::unordered_map<Guid, Handle<T>> m_guid_to_handle;
            std::unordered_map<Handle<T>, Guid> m_handle_to_guid;

            mutable std::mutex m_mutex;

        public:

            // --- Statically typed add ----------------------------------------

            Handle<T> add(const T& object, const Guid& guid)
            {
                std::lock_guard lock{ m_mutex };
                return typed_add_no_lock(guid, T{ object }); // copy to pool
            }

            Handle<T> add(T&& object, const Guid& guid)
            {
                std::lock_guard lock{ m_mutex };
                return typed_add_no_lock(guid, std::move(object)); // move to pool
            }

            // --- Meta typed add ----------------------------------------------

            MetaHandle add(const Guid& guid, const entt::meta_any& data) override
            {
                std::lock_guard lock{ m_mutex };
                T copy = data.cast<T>();                              // copy #1
                return typed_add_no_lock(guid, std::move(copy));      // move #1
            }

            MetaHandle add(const Guid& guid, entt::meta_any&& data) override
            {
                std::lock_guard lock{ m_mutex };
                auto& ref = data.cast<T&>();                          // no copy
                return typed_add_no_lock(guid, std::move(ref));       // move #2
            }

        private:
            Handle<T> typed_add_no_lock(const Guid& guid, T&& object)
            {
                if (m_guid_to_handle.contains(guid)) {
                    throw ValidationError{ "Duplicate Guid in Storage::add" };
                }
                auto handle = m_pool.create(std::forward<T>(object));  // one move
                m_versions.assign_version(handle);
                m_guid_to_handle[guid] = handle;
                m_handle_to_guid[handle] = guid;
                return handle;
            }

        public:

            // --- Meta typed modify (locked) -----------------------------------
            // Hold the pool lock for the lifetime of the meta_any reference.
            void with_meta_ref(const MetaHandle& meta_handle, const std::function<void(entt::meta_any&)>& visitor) override
            {
                std::lock_guard lock{ m_mutex };
                auto opt = validate_handle_no_lock(meta_handle);
                if (!opt) {
                    throw ValidationError{ "Invalid or not-ready MetaHandle" };
                }
                auto& obj = m_pool.get(*opt);
                entt::meta_any any = entt::forward_as_meta(obj);
                visitor(any);
            }

            // Const variant of the locked meta callback.
            void with_meta_ref(const MetaHandle& meta_handle, const std::function<void(entt::meta_any&)>& visitor) const override
            {
                std::lock_guard lock{ m_mutex };
                auto opt = validate_handle_no_lock(meta_handle);
                if (!opt) {
                    throw ValidationError{ "Invalid or not-ready MetaHandle" };
                }
                const auto& obj = m_pool.get(*opt);
                entt::meta_any any = entt::forward_as_meta(obj);
                visitor(any);
            }

            // --- Statically typed modify (locked) -----------------------------
            // Invoke a callback with a reference while holding the pool mutex.
            template<class F>
            auto with_ref(const Handle<T>& h, F&& visitor)
                -> std::invoke_result_t<F, T&>
            {
                std::lock_guard lock{ m_mutex };
                if (!validate_handle_no_lock(h))
                    throw ValidationError{ "Invalid or not-ready Handle in with_ref" };
                T& obj = m_pool.get(h);
                if constexpr (std::is_void_v<std::invoke_result_t<F, T&>>) {
                    std::forward<F>(visitor)(obj);
                }
                else {
                    return std::forward<F>(visitor)(obj);
                }
            }

            // Const variant of the locked typed callback.
            template<class F>
            auto with_cref(const Handle<T>& h, F&& visitor) const
                -> std::invoke_result_t<F, const T&>
            {
                std::lock_guard lock{ m_mutex };
                if (!validate_handle_no_lock(h))
                    throw ValidationError{ "Invalid or not-ready Handle in with_cref" };
                const T& obj = m_pool.get(h);
                if constexpr (std::is_void_v<std::invoke_result_t<F, const T&>>) {
                    std::forward<F>(visitor)(obj);
                }
                else {
                    return std::forward<F>(visitor)(obj);
                }
            }

        private:
        public:
            // -----------------------------------------------------------------

            /// @brief Remove this object immediately (statically typed).
            void remove_now(const Handle<T>& h)
            {
                std::lock_guard lock{ m_mutex };
                if (!validate_handle_no_lock(h)) {
                    throw ValidationError{ "Invalid or not‐ready Handle in remove_now_typed" };
                }
                // erase maps & version/refcount, destroy storage
                auto gid = m_handle_to_guid.at(h);
                m_handle_to_guid.erase(h);
                m_guid_to_handle.erase(gid);

                m_versions.remove(h);
                m_pool.destroy(h);
            }

            void remove_now(const MetaHandle& mh) override
            {
                std::lock_guard lock{ m_mutex };

                auto opt = validate_handle_no_lock(mh);
                if (!opt) throw ValidationError{ "Invalid or not‐ready MetaHandle" };
                auto handle = *opt;

                auto it = m_handle_to_guid.find(handle);
                if (it == m_handle_to_guid.end())
                    throw ValidationError{ "Missing guid mapping for handle" };
                auto gid = it->second;
                m_handle_to_guid.erase(it);
                m_guid_to_handle.erase(gid);

                m_versions.remove(handle);
                m_pool.destroy(handle);
            }

            size_t element_size() const noexcept override
            {
                return sizeof(T);
            }

            size_t count_free() const noexcept override
            {
                std::lock_guard lock{ m_mutex };
                return m_pool.count_free();
            }

            size_t capacity() const noexcept override
            {
                std::lock_guard lock{ m_mutex };
                return m_pool.capacity();
            }

            bool valid(const Handle<T>& h) const noexcept
            {
                std::lock_guard lock{ m_mutex };
                return validate_handle_no_lock(h) && m_handle_to_guid.contains(h);
            }

            bool valid(const MetaHandle& mh) const noexcept override
            {
                std::lock_guard lock{ m_mutex };
                if (auto typed_handle = mh.template cast<T>()) {
                    return validate_handle_no_lock(*typed_handle) && m_handle_to_guid.contains(*typed_handle);
                }
                return false;
            }

            void clear() noexcept override
            {
                std::lock_guard lock{ m_mutex };

                m_pool.clear();

                m_versions = VersionMap{};

                m_guid_to_handle.clear();
                m_handle_to_guid.clear();
            }

            /// @brief Find the handle associated to a GUID, statically typed.
            /// @returns an empty optional if no such GUID or wrong type.
            std::optional<Handle<T>> typed_handle_for_guid(const Guid& guid) const noexcept
            {
                std::lock_guard lock{ m_mutex };
                auto it = m_guid_to_handle.find(guid);
                if (it == m_guid_to_handle.end()) {
                    return std::nullopt;
                }
                Handle<T> h = it->second;
                return validate_handle_no_lock(h)
                    ? std::optional<Handle<T>>(h)
                    : std::nullopt;
            }

            std::optional<MetaHandle> handle_for_guid(const Guid& guid) const noexcept override
            {
                std::lock_guard lock{ m_mutex };
                auto it = m_guid_to_handle.find(guid);
                if (it != m_guid_to_handle.end()) {
                    MetaHandle mh{ it->second };
                    if (auto h = validate_handle_no_lock(mh))
                        return mh;
                }
                return std::nullopt;
            }

            /// @brief Find the GUID associated to a `Handle<T>`.
            /// @returns empty if invalid or not in this pool.
            std::optional<Guid> guid_for_handle_typed(const Handle<T>& h) const noexcept {
                std::lock_guard lock{ m_mutex };
                if (!validate_handle_no_lock(h)) {
                    return std::nullopt;
                }
                auto it = m_handle_to_guid.find(h);
                return it != m_handle_to_guid.end()
                    ? std::optional<Guid>(it->second)
                    : std::nullopt;
            }

            std::optional<Guid> guid_for_handle(const MetaHandle& mh) const noexcept override
            {
                std::lock_guard lock{ m_mutex };
                // First check that this handle is still valid under our versions map
                if (auto h = validate_handle_no_lock(mh)) {
                    auto it = m_handle_to_guid.find(*h);
                    if (it != m_handle_to_guid.end())
                        return it->second;
                }
                return std::nullopt;
            }

            // --- Visitor methods ---------------------------------------------

            /// @brief Visit all objects of this static type in the pool.
            /// @param visitor A function that takes a const reference to T
            /// @note Risk for deadlock if visitor re-enters storage
            template<class F>
            void visit(F&& visitor) const noexcept
            {
                std::lock_guard lock{ m_mutex };
                m_pool.used_visitor(std::forward<F>(visitor));
            }

            /// @brief Visit all objects of this static type in the pool.
            /// @param visitor A function that takes a reference to T
            /// @note Risk for deadlock if visitor re-enters storage
            template<class F>
            void visit(F&& visitor) noexcept
            {
                std::lock_guard lock{ m_mutex };
                m_pool.used_visitor(std::forward<F>(visitor));
            }

            /// @brief Visit all objects of this meta type in the pool (const).
            /// @param visitor A function that takes an entt::meta_any with a const reference to T
            /// @note Risk for deadlock if visitor re-enters storage
            void visit_any(const std::function<void(entt::meta_any)>& visitor) const noexcept override
            {
                std::lock_guard lock{ m_mutex };
                m_pool.used_visitor([&](const T& elem) {
                    visitor(entt::forward_as_meta(elem));
                    });
            }

            /// @brief Visit all objects of this meta type in the pool.
            /// @param visitor A function that takes an entt::meta_any with a reference to T
            /// @note Risk for deadlock if visitor re-enters storage
            void visit_any(const std::function<void(entt::meta_any)>& visitor) noexcept override
            {
                std::lock_guard lock{ m_mutex };
                m_pool.used_visitor([&](T& elem) {
                    visitor(entt::forward_as_meta(elem));
                    });
            }

            // --- Debugging methods -------------------------------------------

            /// @brief Return a string representation of this pool.
            /// @note Thread-safe, but may block if used in a visitor.
            std::string to_string() const override
            {
                std::lock_guard lock{ m_mutex };
                std::ostringstream oss;
                oss << "  entries: " << m_guid_to_handle.size() << "\n";
                oss << "  versions:  " << m_versions.to_string() << "\n";
                oss << "  allocator:\n" << m_pool.to_string();
                return oss.str();
            }

        private:

            inline bool map_contains(const auto& map, const auto& key)
            {
                return map.find(key) != map.end();
            }

            // Validation without locking or throwing
            bool validate_handle_no_lock(
                const Handle<T>& handle) const noexcept
            {
                return handle && m_versions.validate(handle);
            }

            // Validation without locking or throwing
            std::optional<Handle<T>> validate_handle_no_lock(
                const MetaHandle& meta_handle) const noexcept
            {
                if (!meta_handle.valid()) return {};
                // if (auto h = meta_handle.template cast<T>(); h && *h && m_versions.validate(*h))
                // if (auto h = meta_handle.template cast<T>(); h && validate_handle_no_lock(*h))
                //     return *h;
                if (auto h = meta_handle.template cast<T>())
                    if (validate_handle_no_lock(*h))
                        return *h;

                return {};
            }

            std::optional<Handle<T>> validate_handle(
                const MetaHandle& meta_handle) const noexcept
            {
                std::lock_guard lock{ m_mutex };
                return validate_handle_no_lock(meta_handle);
            }
        };

    public:
        Storage() = default;
        Storage(Storage const&) = delete;
        Storage& operator=(Storage const&) = delete;

        // Explicit move-ctor: steal pools under lock, leave a fresh mutex
        Storage(Storage&& other) noexcept {
            std::lock_guard lock{ other.storage_mutex };
            pools = std::move(other.pools);
            // other.storage_mutex stays valid (default-constructed)
        }

        // Explicit move-assign: lock both, then swap
        Storage& operator=(Storage&& other) noexcept {
            if (this != &other) {
                std::scoped_lock lock{ storage_mutex, other.storage_mutex };
                pools = std::move(other.pools);
            }
            return *this;
        }

        // ---------------------------------------------------------------------

        template<typename T>
        bool has_storage() const noexcept
        {
            entt::id_type meta_id = get_id_type<T>();
            std::shared_lock lock{ storage_mutex };
            return pools.find(meta_id) != pools.end();
        }

        /// Make sure a pool exists. Used by meta types.
        template<typename T>
        entt::id_type assure_storage()
        {
            //auto meta_type = entt::resolve<T>();
            entt::id_type meta_id = get_id_type<T>();

            {
                std::shared_lock lock{ storage_mutex };
                if (pools.find(meta_id) != pools.end())
                    return meta_id;
            }

            std::unique_lock lock{ storage_mutex };
            if (pools.find(meta_id) == pools.end())
            {
                pools[meta_id] = std::make_unique<Pool<T>>();

                // Debug log
                // std::string type_name(type.info().name());
                // EENG_LOG(ctx, "Created storage for type %s", type_name.c_str());
            }
            return meta_id;
        }

        // --- Statically typed add --------------------------------------------

        /// @brief Add statically typed object as lvalue (thread-safe)
        /// @return A copy of the requested object
        template<typename T>
            requires(!std::is_same_v<T, entt::meta_any>)
        Handle<T> add(
            const T& t,
            const Guid& guid)
        {
            auto& pool = get_or_create_pool<T>();
            return pool.add(t, guid);
        }

        /// @brief Add statically typed object as rvalue (thread-safe)
        /// @return A copy of the requested object
        template<typename T>
            requires(!std::is_same_v<std::decay_t<T>, entt::meta_any>)
        Handle<T> add(
            T&& t,
            const Guid& guid)
        {
            auto& pool = get_or_create_pool<std::remove_cv_t<std::remove_reference_t<T>>>();
            return pool.add(std::forward<T>(t), guid);
        }

        // --- Meta typed add --------------------------------------------------

        /// @brief Add runtime typed object as lvalue (not thread-safe)
        MetaHandle add(const entt::meta_any& data, const Guid& guid)
        {
            auto& pool = get_or_create_pool(data.type());
            return pool.add(guid, data); // lvalue
        }

        /// @brief Add runtime typed object as rvalue (not thread-safe)
        MetaHandle add(entt::meta_any&& data, const Guid& guid)
        {
            auto& pool = get_or_create_pool(data.type());
            return pool.add(guid, std::move(data));  // rvalue
        }

        // --- Statically typed get --------------------------------------------

        /// @brief Statically typed get (thread-safe)
        /// @return A copy of the requested object
        template<typename T>
        T get_val(const Handle<T>& h) const
        {
            return get_pool<T>().with_cref(h, [](const T& obj) { return obj; });
        }

        // --- Statically typed read -----------------------------------------

        template<typename T, typename Fn>
            requires std::invocable<Fn, const T&>
        auto read(const Handle<T>& h, Fn&& f) const
            -> std::invoke_result_t<Fn, const T&>
        {
            const auto& pool = get_pool<T>();
            return pool.with_cref(h, std::forward<Fn>(f));
        }

        template<typename T, typename U, typename Fn>
            requires (!std::is_same_v<T, U> && std::invocable<Fn, const T&, const U&>)
        auto read2(const Handle<T>& a, const Handle<U>& b, Fn&& f) const
            -> std::invoke_result_t<Fn, const T&, const U&>
        {
            const auto& pool_a = get_pool<T>();
            const auto& pool_b = get_pool<U>();

            std::scoped_lock lock{ pool_a.m_mutex, pool_b.m_mutex };

            if (!pool_a.validate_handle_no_lock(a))
                throw ValidationError{ "Invalid or not-ready Handle<T> in read2" };

            if (!pool_b.validate_handle_no_lock(b))
                throw ValidationError{ "Invalid or not-ready Handle<U> in read2" };

            const T& ref_a = pool_a.m_pool.get(a);
            const U& ref_b = pool_b.m_pool.get(b);

            if constexpr (std::is_void_v<std::invoke_result_t<Fn, const T&, const U&>>) {
                std::forward<Fn>(f)(ref_a, ref_b);
            }
            else {
                return std::forward<Fn>(f)(ref_a, ref_b);
            }
        }

        template<typename T, typename Fn>
            requires std::invocable<Fn, const T&, const T&>
        auto read2(const Handle<T>& a, const Handle<T>& b, Fn&& f) const
            -> std::invoke_result_t<Fn, const T&, const T&>
        {
            const auto& pool = get_pool<T>();

            std::lock_guard lock{ pool.m_mutex };

            if (!pool.validate_handle_no_lock(a))
                throw ValidationError{ "Invalid or not-ready Handle<T> in read2" };

            if (!pool.validate_handle_no_lock(b))
                throw ValidationError{ "Invalid or not-ready Handle<T> in read2" };

            const T& ref_a = pool.m_pool.get(a);
            const T& ref_b = pool.m_pool.get(b);

            if constexpr (std::is_void_v<std::invoke_result_t<Fn, const T&, const T&>>) {
                std::forward<Fn>(f)(ref_a, ref_b);
            }
            else {
                return std::forward<Fn>(f)(ref_a, ref_b);
            }
        }

        // --- Meta typed read -----------------------------------------------

        template<class Fn>
            requires std::invocable<Fn, const entt::meta_any&>
        auto read_meta(const MetaHandle& mh, Fn&& f) const
            -> std::invoke_result_t<Fn, const entt::meta_any&>
        {
            using result_t = std::invoke_result_t<Fn, const entt::meta_any&>;
            if (!mh.valid())
                throw ValidationError{ "Invalid MetaHandle" };

            const auto& pool = get_pool(mh.type.id());
            if constexpr (std::is_void_v<result_t>) {
                pool.with_meta_ref(mh, [&](entt::meta_any& any) {
                    std::forward<Fn>(f)(any);
                });
            }
            else {
                std::optional<result_t> out;
                pool.with_meta_ref(mh, [&](entt::meta_any& any) {
                    out = std::forward<Fn>(f)(any);
                });
                return *out;
            }
        }

        // --- Meta typed modify -----------------------------------------------

        template<class Fn>
            requires std::invocable<Fn, entt::meta_any&>
        auto modify(const MetaHandle& mh, Fn&& f)
            -> std::invoke_result_t<Fn, entt::meta_any&>
        {
            using result_t = std::invoke_result_t<Fn, entt::meta_any&>;
            auto& pool = get_pool(mh.type.id());

            if constexpr (std::is_void_v<result_t>) {
                // Acquire + validate under the pool lock to keep the reference stable.
                pool.with_meta_ref(mh, [&](entt::meta_any& any) {
                    std::forward<Fn>(f)(any);
                });
            }
            else {
                std::optional<result_t> out;
                // Acquire + validate under the pool lock to keep the reference stable.
                pool.with_meta_ref(mh, [&](entt::meta_any& any) {
                    out = std::forward<Fn>(f)(any);
                });
                return *out;
            }
        }

        // --- Statically typed modify -----------------------------------------

        template<typename T, typename Fn>
            requires std::invocable<Fn, T&>
        auto modify(const Handle<T>& h, Fn&& f)
            -> std::invoke_result_t<Fn, T&>
        {
            auto& pool = get_pool<T>();
            return pool.with_ref(h, std::forward<Fn>(f));
        }

        // Not needed - just use get_ref<>
        // template<typename T, typename Fn>
        // auto modify_no_lock(const Handle<T>& h, Fn&& f)
        //     -> std::invoke_result_t<Fn, T&>
        // {
        //     auto& pool = get_pool<T>();
        //     T& obj = pool.get_ref_nolock(h);

        //     if constexpr (std::is_void_v<std::invoke_result_t<Fn, T&>>) {
        //         std::forward<Fn>(f)(obj);
        //     }
        //     else {
        //         return std::forward<Fn>(f)(obj);
        //     }
        // }

        // ---------------------------------------------------------------------

        /// @brief Remove immediately by statically‑typed handle (thread‑safe).
        template<typename T>
        void remove_now(const Handle<T>& h)
        {
            get_pool<T>().remove_now(h);
        }

        /// @brief Immediately destroy the resource referred to by a runtime‑typed handle (thread‑safe).
        void remove_now(const MetaHandle& mh)
        {
            get_pool(mh.type.id()).remove_now(mh);
        }

        // --- Capacity and free slots -----------------------------------------

        template<typename T>
        size_t count_free() const noexcept
        {
            return get_pool<T>().count_free();
        }

        size_t capacity(entt::id_type id_type) const noexcept
        {
            return get_pool(id_type).capacity();
        }

        // --- Validation methods ----------------------------------------------

        template<typename T>
        bool validate(const Handle<T>& h) const noexcept
        {
            auto* pool = find_pool_ptr<T>();
            return pool ? pool->valid(h) : false;
        }

        /// @return true if there's a pool for `mh.type` and the handle’s version is still valid.
        bool validate(const MetaHandle& mh) const noexcept
        {
            auto* pool = find_pool_ptr(mh.type.id());
            return pool ? pool->valid(mh) : false;
        }

        /// @brief Remove *all* resources in *all* pools, but keep the same pool objects.
        void clear() noexcept
        {
            for (auto& [_, pool] : snapshot_pools()) {
                pool->clear();
            }
        }

        // ---------------------------------------------------------------------

        template<class T>
        std::optional<Handle<T>> handle_for_guid(const Guid& guid) const noexcept
        {
            auto* pool = find_pool_ptr<T>();
            return pool ? pool->typed_handle_for_guid(guid) : std::nullopt;
        }

        std::optional<MetaHandle> handle_for_guid(const Guid& guid) const noexcept
        {
            for (const auto& [_, pool] : snapshot_pools()) {
                if (auto h = pool->handle_for_guid(guid))
                    return h;
            }
            return std::nullopt;
        }

        std::optional<Guid> guid_for_handle(const MetaHandle& mh) const noexcept
        {
            auto* pool = find_pool_ptr(mh.type.id());
            if (pool)
                return pool->guid_for_handle(mh);
            return std::nullopt;
        }

        // --- Pool stats snapshot --------------------------------------------
        // Capture pool occupancy stats without holding the storage lock long-term.
        struct PoolStats
        {
            entt::id_type type_id{};
            size_t capacity = 0;
            size_t free_count = 0;
            size_t element_size = 0;
        };

        std::vector<PoolStats> pool_stats() const
        {
            auto snapshot = snapshot_pools();
            std::vector<PoolStats> stats;
            stats.reserve(snapshot.size());
            for (const auto& [type_id, pool_ptr] : snapshot) {
                PoolStats row{};
                row.type_id = type_id;
                row.capacity = pool_ptr->capacity();
                row.free_count = pool_ptr->count_free();
                row.element_size = pool_ptr->element_size();
                stats.push_back(row);
            }
            return stats;
        }

        // Debug string for a single pool (may block on pool mutex).
        std::string pool_debug_string(entt::id_type type_id) const
        {
            auto* pool = find_pool_ptr(type_id);
            return pool ? pool->to_string() : std::string{};
        }

        // -> registry.storage() -> [entt::id_type, entt::meta_type]
        // pool()

        // template<class T>
        // pool()

        // pool(entt::id_type id) 

        // --- Visitor methods -------------------------------------------------

        /// Visit all objects of a statically typed pool (non-const).
        template<typename T, class F>
            requires std::is_invocable_v<F, T&>
        bool visit(F&& visitor) noexcept
        {
            try {
                get_pool<T>().visit(std::forward<F>(visitor));
            }
            catch (const std::out_of_range&) {
                return false;
            }
            return true;
        }

        /// Visit all objects of a statically typed pool (const).
        template<typename T, class F>
            requires std::is_invocable_v<F, const T&>
        bool visit(F&& visitor) const noexcept
        {
            try {
                get_pool<T>().visit(std::forward<F>(visitor));
            }
            catch (const std::out_of_range&) {
                return false;
            }
            return true;
        }

        /// Visit all objects of a runtime typed pool (const).
        template<class F>
            requires std::is_invocable_v<F, entt::meta_any>
        bool visit(entt::id_type id_type, F&& visitor) const noexcept
        {
            try {
                auto& pool = get_pool(id_type);
                pool.visit_any(
                    std::function<void(entt::meta_any)>{
                    [vis = std::forward<F>(visitor)]
                    (entt::meta_any any) mutable {
                        vis(any);
                        }
                }
                );
            }
            catch (const std::runtime_error&) {
                return false;
            }
            return true;
        }

        /// Visit all objects of a runtime typed pool (non-const).
        template<class F>
            requires std::is_invocable_v<F, entt::meta_any>
        bool visit(entt::id_type id_type, F&& visitor) noexcept
        {
            try {
                auto& pool = get_pool(id_type);
                pool.visit_any(
                    std::function<void(entt::meta_any)>{
                    [vis = std::forward<F>(visitor)]
                    (entt::meta_any any) mutable {
                        vis(any);
                        }
                }
                );
            }
            catch (const std::runtime_error&) {
                return false;
            }
            return true;
        }

        // --- Debugging methods -----------------------------------------------

        std::string to_string() const
        {
            auto snapshot = snapshot_pools();
            std::ostringstream oss;
            oss << "Storage summary:\n";
            for (auto const& [type_id, pool_ptr] : snapshot)
            {
                auto type_name = entt::resolve(type_id).info().name();
                oss << "- Type " << type_name << " (id = " << type_id << ")\n";
                oss << pool_ptr->to_string() << "\n";
            }
            return oss.str();
        }

    private:

        void assure_storage(entt::meta_type meta_type)
        {
            entt::meta_func meta_func = meta_type.func(eeng::literals::assure_storage_hs);
            assert(meta_func);
            entt::meta_any res = meta_func.invoke({}, entt::forward_as_meta(*this));
            assert(res);
        }

        using PoolEntry = std::pair<entt::id_type, IPool*>;
        using PoolEntryConst = std::pair<entt::id_type, const IPool*>;

        // --- Pool snapshotting -----------------------------------------------
        // Collect stable pointers to pools while holding the storage lock.
        std::vector<PoolEntry> snapshot_pools()
        {
            std::shared_lock lock{ storage_mutex };
            std::vector<PoolEntry> snapshot;
            snapshot.reserve(pools.size());
            for (auto& [type_id, pool_ptr] : pools)
                snapshot.emplace_back(type_id, pool_ptr.get());
            return snapshot;
        }

        // Const snapshot variant.
        std::vector<PoolEntryConst> snapshot_pools() const
        {
            std::shared_lock lock{ storage_mutex };
            std::vector<PoolEntryConst> snapshot;
            snapshot.reserve(pools.size());
            for (const auto& [type_id, pool_ptr] : pools)
                snapshot.emplace_back(type_id, pool_ptr.get());
            return snapshot;
        }

        // --- Pool lookup helpers --------------------------------------------
        // Find a pool pointer without holding the lock beyond lookup.
        template<typename T>
        Pool<T>* find_pool_ptr()
        {
            auto meta_id = get_id_type<T>();
            std::shared_lock lock{ storage_mutex };
            auto it = pools.find(meta_id);
            if (it == pools.end()) return nullptr;
            return static_cast<Pool<T>*>(it->second.get());
        }

        // Const variant for typed lookup.
        template<typename T>
        const Pool<T>* find_pool_ptr() const
        {
            auto meta_id = get_id_type<T>();
            std::shared_lock lock{ storage_mutex };
            auto it = pools.find(meta_id);
            if (it == pools.end()) return nullptr;
            return static_cast<const Pool<T>*>(it->second.get());
        }

        // Runtime-typed lookup.
        IPool* find_pool_ptr(entt::id_type id_type)
        {
            std::shared_lock lock{ storage_mutex };
            auto it = pools.find(id_type);
            if (it == pools.end()) return nullptr;
            return it->second.get();
        }

        // Const runtime-typed lookup.
        const IPool* find_pool_ptr(entt::id_type id_type) const
        {
            std::shared_lock lock{ storage_mutex };
            auto it = pools.find(id_type);
            if (it == pools.end()) return nullptr;
            return it->second.get();
        }

        // --- Statically typed pool getters -----------------------------------

        template<typename T>
        const Pool<T>& get_pool() const
        {
            auto* pool = find_pool_ptr<T>();
            if (!pool) throw std::out_of_range("Pool not found");
            return *pool;
        }

        template<typename T>
        Pool<T>& get_pool()
        {
            auto* pool = find_pool_ptr<T>();
            if (!pool) throw std::out_of_range("Pool not found");
            return *pool;
        }

        template<typename T>
        Pool<T>& get_or_create_pool()
        {
            (void)assure_storage<T>();
            auto* pool = find_pool_ptr<T>();
            if (!pool) throw std::runtime_error("Pool not found");
            return *pool;
        }

        // --- Meta typed pool getters -----------------------------------------

        const IPool& get_pool(entt::id_type id_type) const
        {
            auto* pool = find_pool_ptr(id_type);
            if (!pool) throw std::runtime_error("Pool not found");
            return *pool;
        }

        IPool& get_pool(entt::id_type id_type)
        {
            auto* pool = find_pool_ptr(id_type);
            if (!pool) throw std::runtime_error("Pool not found");
            return *pool;
        }

        IPool& get_or_create_pool(entt::meta_type meta_type)
        {
            if (!meta_type) throw std::runtime_error("No meta type found");
            assure_storage(meta_type);
            auto* pool = find_pool_ptr(meta_type.id());
            if (!pool) throw std::runtime_error("Pool not found");
            return *pool;
        }

        // --- Private helpers -------------------------------------------------

        template<class T>
        constexpr entt::id_type get_id_type() const noexcept
        {
            return entt::resolve<T>().id();

            // auto meta_type = entt::resolve<T>();
            // return meta_type ? meta_type.id() : entt::type_hash<T>::value();
        }

        // --- Private members -------------------------------------------------

        mutable std::shared_mutex storage_mutex;
        std::unordered_map<entt::id_type, std::unique_ptr<IPool>> pools;

    };

    static_assert(!std::is_copy_constructible_v<Storage>);
    static_assert(!std::is_copy_assignable_v<Storage>);
    static_assert(std::is_move_constructible_v<Storage>);
    static_assert(std::is_move_assignable_v<Storage>);

} // namespace eeng
