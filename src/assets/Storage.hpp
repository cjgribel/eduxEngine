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

#include "entt/entt.hpp"
#include "MetaLiterals.h"
#include "Handle.h"
#include "Guid.h"
#include "PoolAllocatorTFH.h"

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

    class RefCountMap
    {
        std::vector<size_t> refs;

        template<typename T>
        size_t& ref_slot(const Handle<T>& handle) {
            if (!handle)
                throw std::invalid_argument{ "Null handle in RefCountMap" };
            auto idx = handle.idx;
            if (idx >= refs.size())
                throw std::out_of_range{ "Handle index out of range in RefCountMap" };
            return refs[idx];
        }

    public:

        // Increment refcount for this handle (auto-resizing)
        template<typename T>
        size_t add_ref(const Handle<T>& handle)
        {
            assert(handle);
            auto idx = handle.idx;
            if (idx >= refs.size()) refs.resize(idx + 1, 0);
            return ++refs[idx];
        }

        // Decrement refcount
        template<typename T>
        size_t release(const Handle<T>& handle) {
            size_t& cnt = ref_slot(handle);
            if (cnt == 0)
                throw std::logic_error{ "Reference count underflow in release" };
            return --cnt;
        }

        // Reset counter to zero
        template<typename T>
        void reset(const Handle<T>& handle) {
            size_t& cnt = ref_slot(handle);
            // if (cnt == 0)
            //     throw std::logic_error{ "Reference count underflow in reset" };
            cnt = 0;
        }

        template<typename T>
        size_t count(const Handle<T>& handle) const
        {
            auto idx = handle.idx;
            if (idx >= refs.size()) return 0;
            return refs[idx];
        }

        // Return a comma-separated list of all refcounts
        std::string to_string() const
        {
            std::ostringstream oss;
            for (auto c : refs)
                oss << c << ", ";
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

            virtual entt::meta_any get(const MetaHandle& meta_handle) = 0;
            virtual entt::meta_any get(const MetaHandle& meta_handle) const = 0;

            virtual std::optional<entt::meta_any> try_get(const MetaHandle& mh) noexcept = 0;
            virtual std::optional<entt::meta_any> try_get(const MetaHandle& mh) const noexcept = 0;

            virtual void remove_now(const MetaHandle& handle) = 0;
            virtual size_t retain(const MetaHandle& mh) = 0;
            virtual size_t release_and_destroy(const MetaHandle& mh) = 0;

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

            PoolAllocatorTFH<T> m_pool;

            VersionMap m_versions;
            RefCountMap m_ref_counts;

            std::unordered_map<Guid, Handle<T>> m_guid_to_handle;
            std::unordered_map<Handle<T>, Guid> m_handle_to_guid;

            // mutable std::mutex m_mutex;
            mutable std::recursive_mutex m_mutex;

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
                auto handle = m_pool.create(std::forward<T>(object));  // one move
                m_versions.assign_version(handle);
                m_ref_counts.add_ref(handle);
                m_guid_to_handle[guid] = handle;
                m_handle_to_guid[handle] = guid;
                return handle;
            }

        public:

            // --- Meta typed get & try_get ------------------------------------

            T& get_ref(const Handle<T>& handle)
            {
                std::lock_guard lock{ m_mutex };
                if (!validate_handle_no_lock(handle)) throw ValidationError{ "Invalid or not-ready Handle in get_ref_nolock" };
                return m_pool.get(handle);
            }

            const T& get_ref(const Handle<T>& handle) const
            {
                std::lock_guard lock{ m_mutex };
                if (!validate_handle_no_lock(handle)) throw ValidationError{ "Invalid or not-ready Handle in get_ref_nolock" };
                return m_pool.get(handle);
            }

            /// @brief Unsafe, no‑lock reference access. Caller must ensure no concurrent
            ///        add/release on the same slot.
            T& get_ref_nolock(const Handle<T>& handle)
            {
                if (!validate_handle_no_lock(handle)) throw ValidationError{ "Invalid or not-ready Handle in get_ref_nolock" };
                // validate without locking
                // MetaHandle mh{ handle };
                // auto opt = validate_handle_no_lock(mh);
                // if (!opt || *opt != handle) {
                //     throw ValidationError{ "Invalid or not-ready Handle in get_ref_nolock" };
                // }
                return m_pool.get(handle);
            }

            /// @brief Unsafe const reference access.
            const T& get_ref_nolock(const Handle<T>& handle) const
            {
                if (!validate_handle_no_lock(handle)) throw ValidationError{ "Invalid or not-ready Handle in get_ref_nolock" };

                // MetaHandle mh{ handle };
                // auto opt = validate_handle_no_lock(mh);
                // if (!opt || *opt != handle) {
                //     throw ValidationError{ "Invalid or not-ready Handle in get_ref_nolock" };
                // }
                return m_pool.get(handle);
            }

            // --- Meta typed get & try_get ------------------------------------

            entt::meta_any get(const MetaHandle& meta_handle) override
            {
                std::lock_guard lock{ m_mutex };
                return get_impl(*this, meta_handle);
            }

            entt::meta_any get(const MetaHandle& meta_handle) const override
            {
                std::lock_guard lock{ m_mutex };
                return get_impl(*this, meta_handle);
            }

            std::optional<entt::meta_any> try_get(const MetaHandle& meta_handle) noexcept override
            {
                return try_get_impl(*this, meta_handle);
            }

            std::optional<entt::meta_any> try_get(const MetaHandle& meta_handle) const noexcept override
            {
                return try_get_impl(*this, meta_handle);
            }

        private:

            template<typename PoolType>
            static entt::meta_any get_impl(
                PoolType& self,
                const MetaHandle& meta_handle)
            {
                if (auto opt = self.validate_handle_no_lock(meta_handle); !opt)
                {
                    throw ValidationError{ "Invalid or not‐ready MetaHandle" };
                }
                else
                {
                    auto& obj = self.m_pool.get(*opt);
                    return entt::forward_as_meta(obj);
                }
            }

            template<typename PoolType>
            static std::optional<entt::meta_any> try_get_impl(
                PoolType& self,
                const MetaHandle& meta_handle) noexcept
            {
                std::lock_guard lock{ self.m_mutex };
                if (auto opt = self.validate_handle_no_lock(meta_handle)) {
                    return entt::forward_as_meta(self.m_pool.get(*opt));
                }
                return std::nullopt;
            }

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
                m_ref_counts.reset(h);
                m_pool.destroy(h);
            }

            void remove_now(const MetaHandle& mh) override
            {
                std::lock_guard lock{ m_mutex };

                auto opt = validate_handle_no_lock(mh);
                if (!opt) throw ValidationError{ "Invalid or not‐ready MetaHandle" };
                auto handle = *opt;

                auto gid = m_handle_to_guid[handle];
                m_handle_to_guid.erase(handle);
                m_guid_to_handle.erase(gid);

                m_versions.remove(handle);
                m_ref_counts.reset(handle);
                m_pool.destroy(handle);
            }

            size_t retain(const Handle<T>& h)
            {
                std::lock_guard lock{ m_mutex };
                if (!validate_handle_no_lock(h)) throw ValidationError{ "Bad handle" };
                return m_ref_counts.add_ref(h);
            }

            size_t retain(const MetaHandle& mh) override
            {
                std::lock_guard lock{ m_mutex };
                auto opt = validate_handle_no_lock(mh);
                if (!opt) throw ValidationError{ "Invalid or not-ready MetaHandle" };
                return m_ref_counts.add_ref(*opt);
            }

            size_t release_and_destroy(const Handle<T>& h)
            {
                std::lock_guard lock{ m_mutex };
                if (!validate_handle_no_lock(h)) throw ValidationError{ "Bad handle" };
                auto cnt = m_ref_counts.release(h);
                if (cnt == 0) {
                    auto guid = m_handle_to_guid[h];
                    m_guid_to_handle.erase(guid);
                    m_handle_to_guid.erase(h);
                    m_versions.remove(h);
                    m_ref_counts.reset(h);
                    m_pool.destroy(h);
                }
                return cnt;
            }

            size_t release_and_destroy(const MetaHandle& mh) override
            {
                std::lock_guard lock{ m_mutex };

                auto opt = validate_handle_no_lock(mh);
                if (!opt) {
                    throw ValidationError{ "Invalid or not-ready MetaHandle" };
                }

                auto handle = *opt;
                auto cnt = m_ref_counts.release(handle);

                // if this was the last reference, tear everything down
                if (cnt == 0) {
                    auto gid = m_handle_to_guid[handle];
                    m_handle_to_guid.erase(handle);
                    m_guid_to_handle.erase(gid);
                    m_versions.remove(handle);
                    m_ref_counts.reset(handle);
                    m_pool.destroy(handle);
                }

                return cnt;
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
                return validate_handle_no_lock(h);
            }

            bool valid(const MetaHandle& mh) const noexcept override
            {
                std::lock_guard lock{ m_mutex };
                return static_cast<bool>(validate_handle_no_lock(mh));
            }

            void clear() noexcept override
            {
                std::lock_guard lock{ m_mutex };

                m_pool.clear();

                m_versions = VersionMap{};
                m_ref_counts = RefCountMap{};

                m_guid_to_handle.clear();
                m_handle_to_guid.clear();
            }

            /// @brief Find the handle associated to a GUID, statically typed.
            /// @returns an empty optional if no such GUID or wrong type.
            std::optional<Handle<T>> handle_for_guid_typed(const Guid& guid) const noexcept
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
            template<class F>
            void visit(F&& visitor) const noexcept
            {
                std::lock_guard lock{ m_mutex };
                m_pool.used_visitor(std::forward<F>(visitor));
            }

            /// @brief Visit all objects of this static type in the pool.
            /// @param visitor A function that takes a reference to T
            template<class F>
            void visit(F&& visitor) noexcept
            {
                std::lock_guard lock{ m_mutex };
                m_pool.used_visitor(std::forward<F>(visitor));
            }

            /// @brief Visit all objects of this runtime type in the pool (const).
            /// @param visitor A function that takes an entt::meta_any with a const reference to T
            void visit_any(const std::function<void(entt::meta_any)>& visitor) const noexcept override
            {
                std::lock_guard lock{ m_mutex };
                m_pool.used_visitor([&](const T& elem) {
                    visitor(entt::forward_as_meta(elem));
                    });
            }

            /// @brief Visit all objects of this runtime type in the pool.
            /// @param visitor A function that takes an entt::meta_any with a reference to T
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
                oss << "  ref-counts:" << m_ref_counts.to_string() << "\n";
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

        /// Make sure a pool exists. Used by meta types. Not thread-safe.
        template<typename T>
        entt::id_type assure_storage()
        {
            //auto meta_type = entt::resolve<T>();
            entt::id_type meta_id = get_id_type<T>();

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
            std::lock_guard lock{ storage_mutex };
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
            std::lock_guard lock{ storage_mutex };
            auto& pool = get_or_create_pool<std::remove_cv_t<std::remove_reference_t<T>>>();
            return pool.add(std::forward<T>(t), guid);
        }

        // --- Meta typed add --------------------------------------------------

        /// @brief Add runtime typed object as lvalue (not thread-safe)
        MetaHandle add(const entt::meta_any& data, const Guid& guid)
        {
            std::lock_guard lock{ storage_mutex };
            auto& pool = get_or_create_pool(data.type());
            return pool.add(guid, data); // lvalue
        }

        /// @brief Add runtime typed object as rvalue (not thread-safe)
        MetaHandle add(entt::meta_any&& data, const Guid& guid)
        {
            std::lock_guard lock{ storage_mutex };
            auto& pool = get_or_create_pool(data.type());
            return pool.add(guid, std::move(data));  // rvalue
        }

        // --- Meta typed get --------------------------------------------------

        /// @brief Runtime-typed get (not thread-safe)
        /// @return An entt::metaany with a const reference to the requested object
        entt::meta_any get(const MetaHandle& meta_handle) const
        {
            std::lock_guard lock{ storage_mutex };
            return get_pool(meta_handle.type.id()).get(meta_handle);
        }

        /// @brief Runtime-typed get (not thread-safe)
        /// @return An entt::metaany with a reference to the requested object
        entt::meta_any get(const MetaHandle& meta_handle)
        {
            std::lock_guard lock{ storage_mutex };
            return get_pool(meta_handle.type.id()).get(meta_handle);
        }

        // --- Statically typed get --------------------------------------------

        /// @brief Statically typed get (thread-safe)
        /// @return A copy of the requested object
        template<typename T>
        T get_val(const Handle<T>& h) const
        {
            std::lock_guard lk{ storage_mutex };
            return get_pool<T>().get_ref(h);
        }

        /// @brief Reference‑returning, statically‑typed get (unsafe if you allow
///        concurrent add/release on the same type).
        template<typename T>
        T& get_ref(const Handle<T>& h)
        {
            // no internal lock: user must externally guard against races on this slot
            return get_pool<T>().get_ref_nolock(h);
        }

        template<typename T>
        const T& get_ref(const Handle<T>& h) const
        {
            return get_pool<T>().get_ref_nolock(h);
        }

        // --- try_get ---------------------------------------------------------

        std::optional<entt::meta_any> try_get(const MetaHandle& meta_handle) const noexcept
        {
            std::lock_guard lock{ storage_mutex };
            auto it = pools.find(meta_handle.type.id());
            if (it == pools.end()) return std::nullopt;

            const IPool& pool = *it->second;
            return pool.try_get(meta_handle);
        }

        std::optional<entt::meta_any> try_get(const MetaHandle& meta_handle) noexcept
        {
            std::lock_guard lock{ storage_mutex };
            auto it = pools.find(meta_handle.type.id());
            if (it == pools.end()) return std::nullopt;

            auto& pool = get_pool(meta_handle.type.id());
            return pool.try_get(meta_handle);
        }

        // --- modify ----------------------------------------------------------

        template<typename T, typename Fn>
        auto modify(const Handle<T>& h, Fn&& f)
            -> std::invoke_result_t<Fn, T&>
        {
            std::lock_guard lock{ storage_mutex };
            auto& pool = get_pool<T>();
            T& obj = pool.get_ref(h);

            if constexpr (std::is_void_v<std::invoke_result_t<Fn, T&>>) {
                std::forward<Fn>(f)(obj);
            }
            else {
                return std::forward<Fn>(f)(obj);
            }
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
            std::lock_guard lock{ storage_mutex };
            get_pool<T>().remove_now_typed(h);
        }

        /// @brief Immediately destroy the resource referred to by a runtime‑typed handle (thread‑safe).
        void remove_now(const MetaHandle& mh)
        {
            std::lock_guard lock{ storage_mutex };
            get_pool(mh.type.id()).remove_now(mh);
        }

        /// Increase the ref-count for a resource.
        /// @returns the new ref-count.
        template<typename T>
        size_t retain(const Handle<T>& h)
        {
            std::lock_guard lock{ storage_mutex };
            auto& pool = get_pool<T>();
            return pool.retain(h);
        }

        /// Increase the ref-count for a resource.
        /// @returns the new ref-count.
        size_t retain(const MetaHandle& mh)
        {
            std::lock_guard lock{ storage_mutex };
            auto& pool = get_pool(mh.type.id());
            return pool.retain(mh);
        }

        template<typename T>
        size_t release(const Handle<T>& h)
        {
            std::lock_guard lock{ storage_mutex };
            auto& pool = get_pool<T>();
            return pool.release_and_destroy(h);
        }

        size_t release(const MetaHandle& mh)
        {
            std::lock_guard lock{ storage_mutex };
            auto& pool = get_pool(mh.type.id());
            return pool.release_and_destroy(mh);
        }

        // --- Capacity and free slots -----------------------------------------

        template<typename T>
        size_t count_free() const noexcept
        {
            std::lock_guard lock{ storage_mutex };
            return get_pool<T>().count_free();
        }

        size_t capacity(entt::id_type id_type) const noexcept
        {
            std::lock_guard lock{ storage_mutex };
            return get_pool(id_type).capacity();
        }

        // --- Validation methods ----------------------------------------------

        template<typename T>
        bool validate(const Handle<T>& h) const noexcept
        {
            std::lock_guard lock{ storage_mutex };
            const auto& pool = get_pool<T>();
            return pool.valid(h);
        }

        /// @return true if there's a pool for `mh.type` and the handle’s version is still valid.
        bool validate(const MetaHandle& mh) const noexcept
        {
            std::lock_guard lock{ storage_mutex };
            auto it = pools.find(mh.type.id());
            if (it == pools.end()) return false;
            return it->second->valid(mh);
        }

        /// @brief Remove *all* resources in *all* pools, but keep the same pool objects.
        void clear() noexcept
        {
            std::lock_guard lock{ storage_mutex };
            for (auto& [_, pool] : pools) {
                pool->clear();
            }
        }

        // ---------------------------------------------------------------------

        std::optional<MetaHandle> handle_for_guid(const Guid& guid) const noexcept
        {
            std::lock_guard lock(storage_mutex);
            for (auto const& [_, pool] : pools) {
                if (auto h = pool->handle_for_guid(guid))
                    return h;
            }
            return std::nullopt;
        }

        std::optional<Guid> guid_for_handle(const MetaHandle& mh) const noexcept
        {
            std::lock_guard lock(storage_mutex);
            auto it = pools.find(mh.type.id());
            if (it != pools.end())
                return it->second->guid_for_handle(mh);
            return std::nullopt;
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
            std::lock_guard lock{ storage_mutex };
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
            std::lock_guard lock{ storage_mutex };
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
            std::lock_guard lock{ storage_mutex };
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
            std::lock_guard lock{ storage_mutex };
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
            std::lock_guard lock{ storage_mutex };
            std::ostringstream oss;
            oss << "Storage summary:\n";
            for (auto const& [type_id, pool_ptr] : pools)
            {
                auto name = entt::resolve(type_id).info().name();
                oss << "- Type " << name << " (id = " << type_id << ")\n";
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

        // --- Statically typed pool getters -----------------------------------

        template<typename T>
        const Pool<T>& get_pool() const
        {
            auto meta_id = get_id_type<T>();
            return *static_cast<const Pool<T>*>(pools.at(meta_id).get());
        }

        template<typename T>
        Pool<T>& get_pool()
        {
            auto meta_id = get_id_type<T>();
            return *static_cast<Pool<T>*>(pools.at(meta_id).get());
        }

        template<typename T>
        Pool<T>& get_or_create_pool()
        {
            auto meta_id = assure_storage<T>();
            return *static_cast<Pool<T>*>(pools.at(meta_id).get());
        }

        // --- Meta typed pool getters -----------------------------------------

        const IPool& get_pool(entt::id_type id_type) const
        {
            auto id = id_type; //meta_type.id();
            auto it = pools.find(id);
            if (it == pools.end()) throw std::runtime_error("Pool not found");
            return *it->second;
        }

        IPool& get_pool(entt::id_type id_type)
        {
            auto id = id_type; //meta_type.id();
            auto it = pools.find(id);
            if (it == pools.end()) throw std::runtime_error("Pool not found");
            return *it->second;
        }

        IPool& get_or_create_pool(entt::meta_type meta_type)
        {
            if (!meta_type) throw std::runtime_error("No meta type found");
            assure_storage(meta_type);
            return *pools.at(meta_type.id());
        }

        // --- Private helpers -------------------------------------------------

        template<class T>
        constexpr entt::id_type get_id_type() const noexcept
        {
            auto meta_type = entt::resolve<T>();
            return meta_type ? meta_type.id() : entt::type_hash<T>::value();
        }

        // --- Private members -------------------------------------------------

        // mutable std::mutex storage_mutex;
        mutable std::recursive_mutex storage_mutex;
        std::unordered_map<entt::id_type, std::unique_ptr<IPool>> pools;

    public:

        // --- Iterators -------------------------------------------------------

        using iterator = decltype(pools)::iterator;
        using const_iterator = decltype(pools)::const_iterator;

        iterator begin() {
            std::lock_guard lock{ storage_mutex };
            return pools.begin();
        }
        iterator end() {
            std::lock_guard lock{ storage_mutex };
            return pools.end();
        }

        const_iterator begin() const {
            std::lock_guard lock{ storage_mutex };
            return pools.begin();
        }
        const_iterator end() const {
            std::lock_guard lock{ storage_mutex };
            return pools.end();
        }

        const_iterator cbegin() const { return begin(); }
        const_iterator cend()   const { return end(); }
    };

    static_assert(!std::is_copy_constructible_v<Storage>);
    static_assert(!std::is_copy_assignable_v<Storage>);
    static_assert(std::is_move_constructible_v<Storage>);
    static_assert(std::is_move_assignable_v<Storage>);

} // namespace eeng