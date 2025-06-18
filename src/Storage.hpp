// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once
#include <cstdint>
#include <limits>
#include <vector>
#include <unordered_map>
#include <type_traits> // for std::is_copy_constructible, std::is_move_constructible, etc.
#include <typeindex>
#include <memory>
#include <string>

// #include <chrono>
// #include <iostream>
// #include <iostream>
// #include <ostream>
// #include <iomanip>
// #include <mutex>
// #include <string>
// #include <unordered_map>
// #include <optional>
// #include <unordered_map>

#include "entt/entt.hpp"
#include "MetaLiterals.h"
#include "Handle.h"
#include "Guid.h"
#include "PoolAllocatorTFH.h"
#include "Log.hpp"
// #include "Texture.hpp"

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
            virtual size_t release(const MetaHandle& mh) = 0;

            /// @return true if `mh` is a well-formed handle whose version still matches
            virtual bool valid(const MetaHandle& mh) const noexcept = 0;

            virtual std::optional<MetaHandle> handle_for_guid(const Guid& guid) const noexcept = 0;
            virtual std::optional<Guid> guid_for_handle(const MetaHandle& mh) const noexcept = 0;
        };

        template<typename T>
        class Pool : public IPool
        {
            using Handle = Handle<T>;

            PoolAllocatorTFH<T> m_pool;

            VersionMap m_versions;
            RefCountMap m_ref_counts;

            std::unordered_map<Guid, Handle> m_guid_to_handle;
            std::unordered_map<Handle, Guid> m_handle_to_guid;

            mutable std::mutex m_mutex;

        public:

            MetaHandle add(const Guid& guid, const entt::meta_any& data) override
            {
                std::lock_guard lock(m_mutex);
                assert(guid != Guid::invalid());
                assert(!map_contains(m_guid_to_handle, guid));

                // deep-copy here, since user passed an lvalue
                auto any_copy = data;
                auto& obj = any_copy.cast<T&>();
                auto handle = m_pool.create(std::move(obj));

                // Assign version to handle & count reference
                m_versions.assign_version(handle);
                m_ref_counts.add_ref(handle);

                // Map guid <-> handle
                m_guid_to_handle[guid] = handle;
                m_handle_to_guid[handle] = guid;

                return handle;
            }

            MetaHandle add(const Guid& guid, entt::meta_any&& data) override
            {
                std::lock_guard lock(m_mutex);
                assert(guid != Guid::invalid());
                assert(!map_contains(m_guid_to_handle, guid));

                // Data moved in, so its internal buffer is ours
                auto& obj = data.cast<T&>();  // get reference to the stored T
                auto handle = m_pool.create(std::move(obj));

                // Assign version to handle & count reference
                m_versions.assign_version(handle);
                m_ref_counts.add_ref(handle);

                // Map guid <-> handle
                m_guid_to_handle[guid] = handle;
                m_handle_to_guid[handle] = guid;

                return handle;
            }

            entt::meta_any get(const MetaHandle& meta_handle) override
            {
                return get_impl(*this, meta_handle);
            }

            entt::meta_any get(const MetaHandle& meta_handle) const override
            {
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

            void remove_now(const MetaHandle& mh) override
            {
                std::lock_guard lock{ m_mutex };

                auto opt = validate_handle(mh);
                if (!opt) throw ValidationError{ "Invalid or not‐ready MetaHandle" };
                auto handle = *opt;

                auto gid = m_handle_to_guid[handle];
                m_handle_to_guid.erase(handle);
                m_guid_to_handle.erase(gid);

                m_versions.remove(handle);
                m_ref_counts.reset(handle);
                m_pool.destroy(handle);
            }

            size_t retain(const MetaHandle& mh) override
            {
                if (auto opt = validate_handle(mh)) {
                    std::lock_guard lock{ m_mutex };
                    return m_ref_counts.add_ref(*opt);
                }
                throw ValidationError{ "Invalid or not-ready MetaHandle" };
            }

            size_t release(const MetaHandle& mh) override
            {
                if (auto opt = validate_handle(mh)) {
                    std::lock_guard lock{ m_mutex };
                    auto count = m_ref_counts.release(*opt);
                    return count;
                }
                throw ValidationError{ "Invalid or not-ready MetaHandle" };
            }

            bool valid(const MetaHandle& mh) const noexcept override
            {
                return static_cast<bool>(validate_handle(mh));
            }

            std::optional<MetaHandle> handle_for_guid(const Guid& guid) const noexcept override
            {
                auto it = m_guid_to_handle.find(guid);
                if (it != m_guid_to_handle.end() && validate_handle(MetaHandle{ it->second })) {
                    return MetaHandle{ it->second };
                }
                return std::nullopt;
            }

            std::optional<Guid> guid_for_handle(const MetaHandle& mh) const noexcept override
            {
                if (auto opt = validate_handle(mh)) {
                    auto it = m_handle_to_guid.find(*opt);
                    if (it != m_handle_to_guid.end())
                        return it->second;
                }
                return std::nullopt;
            }

        private:

            inline bool map_contains(const auto& map, const auto& key)
            {
                return map.find(key) != map.end();
            }

            // Validation without locking or throwing
            std::optional<Handle> validate_handle(
                const MetaHandle& meta_handle) const noexcept
            {
                if (!meta_handle.valid()) return {};
                if (auto h = meta_handle.template cast<T>(); h && *h && m_versions.validate(*h))
                    return *h;
                return {};
            }

            template<typename PoolType>
            static entt::meta_any get_impl(
                PoolType& self,
                const MetaHandle& meta_handle)
            {
                if (auto opt = self.validate_handle(meta_handle); !opt)
                {
                    throw ValidationError{ "Invalid or not‐ready MetaHandle" };
                }
                else
                {
                    std::lock_guard lock{ self.m_mutex };
                    auto& obj = self.m_pool.get(*opt);
                    return entt::forward_as_meta(obj);
                }
            }

            template<typename PoolType>
            static std::optional<entt::meta_any> try_get_impl(
                PoolType& self,
                const MetaHandle& meta_handle) noexcept
            {
                if (auto opt = self.validate_handle(meta_handle))
                {
                    std::lock_guard lock{ self.m_mutex };
                    auto& obj = self.m_pool.get(*opt);
                    return entt::forward_as_meta(obj);
                }
                return std::nullopt;
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

        template<typename T>
        void assure_storage()
        {
            auto type = entt::resolve<T>();
            auto id = type.id();

            if (pools.find(id) == pools.end())
            {
                pools[id] = std::make_unique<Pool<T>>();

                // Debug log
                // std::string type_name(type.info().name());
                // eeng::Log("Created storage for type %s", type_name.c_str());
            }
        }

        MetaHandle add(entt::meta_any data, const Guid& guid)
        {
            std::lock_guard lock{ storage_mutex };
            auto& pool = get_or_create_pool(data.type());
            return pool.add(guid, std::move(data));
        }

        entt::meta_any get(const MetaHandle& meta_handle) const
        {
            std::lock_guard lock{ storage_mutex };
            return get_pool(meta_handle.type).get(meta_handle);
        }

        entt::meta_any get(const MetaHandle& meta_handle)
        {
            std::lock_guard lock{ storage_mutex };
            return get_pool(meta_handle.type).get(meta_handle);
        }

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

            auto& pool = get_pool(meta_handle.type);
            return pool.try_get(meta_handle);
        }

        /// Increase the ref-count for a resource.
        /// @returns the new ref-count.
        size_t retain(const MetaHandle& mh)
        {
            std::lock_guard lock{ storage_mutex };
            auto& pool = get_pool(mh.type);
            return pool.retain(mh);
        }

        /// Decrease the ref-count for a resource, and if it hits zero
        /// automatically destroy it.
        /// @returns the new ref-count (0 if destroyed).
        size_t release(const MetaHandle& mh)
        {
            std::lock_guard lock{ storage_mutex };
            auto& pool = get_pool(mh.type);
            size_t count = pool.release(mh);
            if (count == 0) {
                // now that nobody holds it, force-remove
                pool.remove_now(mh);
            }
            return count;
        }

        /// @return true if there's a pool for `mh.type` and the handle’s version is still valid.
        bool validate(const MetaHandle& mh) const noexcept
        {
            std::lock_guard lock{ storage_mutex };
            auto it = pools.find(mh.type.id());
            if (it == pools.end()) return false;
            return it->second->valid(mh);
        }

        // In Storage.hpp

        // TODO: create a Handle<T> and get the right pool direclty, rather than searching
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

    private:

        void assure_storage(entt::meta_type meta_type)
        {
            entt::meta_func meta_func = meta_type.func(assure_storage_hs);
            assert(meta_func);
            entt::meta_any res = meta_func.invoke({}, entt::forward_as_meta(*this));
            assert(res);
        }

        const IPool& get_pool(entt::meta_type meta_type) const
        {
            auto id = meta_type.id();
            auto it = pools.find(id);
            if (it == pools.end()) throw std::runtime_error("Pool not found");
            return *it->second;
        }

        IPool& get_pool(entt::meta_type meta_type)
        {
            auto id = meta_type.id();
            auto it = pools.find(id);
            if (it == pools.end()) throw std::runtime_error("Pool not found");
            return *it->second;
        }

        IPool& get_or_create_pool(entt::meta_type meta_type)
        {
            auto id = meta_type.id();
            auto it = pools.find(id);
            if (it == pools.end())
            {
                assure_storage(meta_type);

                it = pools.find(id);
                if (it == pools.end())
                    throw std::runtime_error("Pool creation failed");
            }
            return *it->second;
        }

        mutable std::mutex storage_mutex;
        std::unordered_map<entt::id_type, std::unique_ptr<IPool>> pools;
    };

    static_assert(!std::is_copy_constructible_v<Storage>);
    static_assert(!std::is_copy_assignable_v<Storage>);
    static_assert(std::is_move_constructible_v<Storage>);
    static_assert(std::is_move_assignable_v<Storage>);

} // namespace eeng