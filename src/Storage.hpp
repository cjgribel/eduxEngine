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
    /*
    VersionMap<MyType>   version_map;
    RefCountMap<MyType>  refcount_map;

    // When you hand out a new handle:
    Handle<MyType> h = pool.allocate();
    version_map.assign_version(h);
    refcount_map.add_ref(h);    // initial ref

    // When a client grabs another reference to the same handle:
    refcount_map.add_ref(h);

    // When they release:
    if (refcount_map.release(h) == 0) {
        // no more users → you can safely remove/reset
        version_map.remove(h);
        pool.deallocate(h);
    }
    */

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

    public:

        // Increment refcount for this handle (auto-resizing)
        template<typename T>
        size_t add_ref(const Handle<T>& handle)
        {
            assert(handle);
            auto idx = handle.idx;
            if (idx >= refs.size())
                refs.resize(idx + 1, 0);
            return ++refs[idx];
        }

        // Decrement; return new count
        template<typename T>
        size_t release(const Handle<T>& handle)
        {
            if (!handle) throw std::invalid_argument{ "Null handle in release" };
            auto idx = handle.idx;
            if (idx >= refs.size()) throw std::out_of_range{ "Handle index out of range in release" };
            if (refs[idx] == 0) throw std::logic_error{ "Reference count underflow in release" };
            return --refs[idx];
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

            // virtual std::optional<entt::meta_any> get(const MetaHandle& handle) const = 0;
            // virtual entt::meta_any get(const MetaHandle& handle) const = 0;
            // virtual entt::meta_any get(const MetaHandle& meta_handle) = 0;

            // 1) Non-const get() forwards to the const impl
            virtual entt::meta_any get(const MetaHandle& meta_handle) = 0;
            virtual entt::meta_any get(const MetaHandle& meta_handle) const = 0;

            // non-const try_get
            virtual std::optional<entt::meta_any> try_get(const MetaHandle& mh) noexcept = 0;
            virtual std::optional<entt::meta_any> try_get(const MetaHandle& mh) const noexcept = 0;

            // virtual bool valid(const Handle& handle) const = 0;
            // virtual void remove(const Handle& handle) = 0;
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
                std::lock_guard lock(m_mutex); // ??? Do locking in Storage?
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

            // get const

            // entt::meta_any get(const MetaHandle& meta_handle) override
            // {
            //     auto maybe_handle = meta_handle.cast<T>();
            //     if (!maybe_handle) throw std::runtime_error{ "Invalid handle type" };
            //     auto handle = *maybe_handle;
            //     if (!m_versions.validate(handle)) throw std::runtime_error("Invalid handle (version mismatch)");

            //     std::lock_guard lock(m_mutex);
            //     T& obj = m_pool.get(handle);

            //     return entt::forward_as_meta(obj);
            // }

    // 1) Non-const get() forwards to the const impl
            entt::meta_any get(const MetaHandle& meta_handle) override
            {
                return get_impl(*this, meta_handle);
            }

            // 2) Const get() also forwards to the same impl
            entt::meta_any get(const MetaHandle& meta_handle) const override
            {
                return get_impl(*this, meta_handle);
            }

            // non-const try_get
            std::optional<entt::meta_any> try_get(const MetaHandle& meta_handle) noexcept override
            {
                return try_get_impl(*this, meta_handle);
            }

            // const try_get
            std::optional<entt::meta_any> try_get(const MetaHandle& meta_handle) const noexcept override
            {
                return try_get_impl(*this, meta_handle);
            }

#if 0
            // 3) RETAIN (someone else wants to share ownership)
            void retain()
            {
                assert(versions.validate(h));
                refcounts.add_ref(h);
            }

            // RELEASE (drop one ref; if zero, fully remove)
            void release()
            {
                assert(versions.validate(h));
                auto cnt = refcounts.release(h);
                // if (cnt == 0) {
                    //     // no more references → bingo, time to reclaim
                    //     versions.remove(h);          // bump version to invalidate any lingering handles
                    //     free_list.push_back(h.idx);  // recycle the slot
                    //     // (you could also overwrite or destruct storage[h.idx] here)
                    // }
            }

            // 2) REMOVE (explicit “kill this slot” regardless of refcount)
            void remove_now()
            {
                assert(versions.validate(h));
                versions.remove(h);              // bump version, so all existing handles fail validation
                // clear any existing refs so future release() calls won't underflow:
                while (refcounts.count(h) > 0)
                    refcounts.release(h);
                free_list.push_back(h.idx);
            }
#endif

        private:

            inline bool map_contains(const auto& map, const auto& key)
            {
                return map.find(key) != map.end();
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

            // Validation without locking or throwing
            std::optional<Handle> validate_handle(
                const MetaHandle& meta_handle) const noexcept
            {
                if (!meta_handle.valid()) return {};
                if (auto h = meta_handle.template cast<T>(); h && *h && m_versions.validate(*h))
                    return *h;
                return {};
            }
        };

        // template<class StorageType>
        // static auto& get_pool(StorageType& storage, entt::meta_type meta_type)
        // {
        //     std::cout << __PRETTY_FUNCTION__ << std::endl; 

        //     auto id = meta_type.id();
        //     auto it = storage.pools.find(id);
        //     if (it == storage.pools.end()) throw std::runtime_error("Resource type not registered");
        //     return *it->second;
        // }

    public:
        Storage() = default;
        // Storage(const Storage&) = delete;
        // Storage& operator=(const Storage&) = delete;
        // Storage(Storage&&) = delete;
        // Storage& operator=(Storage&&) = delete;
        Storage(Storage const&) = delete;
        Storage& operator=(Storage const&) = delete;
        Storage(Storage&&) noexcept = default;
        Storage& operator=(Storage&& other) noexcept
        {
            pools.swap(other.pools);
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
                std::string type_name(type.info().name());
                eeng::Log("Created storage for type %s", type_name.c_str());
            }
        }

        MetaHandle add(
            entt::meta_any data,
            const Guid& guid)
        {
            auto& pool = get_or_create_pool(data.type());
            return pool.add(guid, std::move(data));
        }

        // get const


        entt::meta_any get(const MetaHandle& meta_handle) const
        {
            return get_pool(meta_handle.type).get(meta_handle);
            //return get_pool(*this, meta_handle.type).get(meta_handle);
        }

        entt::meta_any get(const MetaHandle& meta_handle)
        {
            return get_pool(meta_handle.type).get(meta_handle);
            //return get_pool(*this, meta_handle.type).get(meta_handle);
        }

        std::optional<entt::meta_any> try_get(const MetaHandle& meta_handle) const noexcept
        {
            auto it = pools.find(meta_handle.type.id());
            if (it == pools.end()) return std::nullopt;

            const IPool& pool = *it->second;
            return pool.try_get(meta_handle);
        }

        std::optional<entt::meta_any> try_get(const MetaHandle& meta_handle) noexcept
        {
            auto it = pools.find(meta_handle.type.id());
            if (it == pools.end()) return std::nullopt;

            auto& pool = get_pool(meta_handle.type);
            return pool.try_get(meta_handle);
        }

        // // non-const try_get
        // std::optional<entt::meta_any> try_get(const MetaHandle& meta_handle) noexcept override
        // {
        //     return try_get_impl(*this, meta_handle);
        // }

        // // const try_get
        // std::optional<entt::meta_any> try_get(const MetaHandle& meta_handle) const noexcept override
        // {
        //     return try_get_impl(*this, meta_handle);
        // }


    private:

        // -> registry.storage() -> [entt::id_type, entt::meta_type]
        // pool()

        // template<class T>
        // pool()

        // pool(entt::id_type id) 

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

        std::unordered_map<entt::id_type, std::unique_ptr<IPool>> pools;
    };

    static_assert(!std::is_copy_constructible_v<Storage>);
    static_assert(!std::is_copy_assignable_v<Storage>);
    static_assert(std::is_move_constructible_v<Storage>);
    static_assert(std::is_move_assignable_v<Storage>);

#if 0
#define version_null 0


    class IResourcePool
    {
    public:
        virtual ~IResourcePool() = default;
    };

    template<typename T>
    class ResourcePool : public IResourcePool
    {
    public:
        using Handle = Handle<T>;

        ResourcePool()
            : m_pool(TypeInfo::create<T>())
        {
        }

        template<typename... Args>
        Handle add(const Guid& guid, Args&&... args)
        {
            std::lock_guard lock(m_mutex);

            if (guid == Guid::invalid())
                throw std::runtime_error("Cannot add resource with invalid GUID");
            if (m_guid_map.find(guid) != m_guid_map.end())
                throw std::runtime_error("Resource with this GUID already exists");

            Handle h = m_pool.create<T>(std::forward<Args>(args)...);
            ensure_metadata(h);
            m_versions.versionify(h);
            m_ref_counts[h.ofs / sizeof(T)] = 1;

            m_guid_map[guid] = h;
            m_handle_to_guid[h] = guid;

            return h;
        }

        T& get(Handle h)
        {
            std::lock_guard lock(m_mutex);
            if (!m_versions.validate(h))
                throw std::runtime_error("Invalid handle (version mismatch)");
            return m_pool.get<T>(h);
        }

        const T& get(Handle h) const
        {
            std::lock_guard lock(m_mutex);
            if (!m_versions.validate(h))
                throw std::runtime_error("Invalid handle (version mismatch)");
            return m_pool.get<T>(h);
        }

        void remove(Handle h)
        {
            std::lock_guard lock(m_mutex);
            remove_unlocked(h);
        }

        template<typename F>
        void for_each(F&& f)
        {
            std::lock_guard lock(m_mutex);

            m_pool.template used_visitor<T>([&](T& obj)
                {
                    f(obj);
                });
        }

        void retain(Handle h)
        {
            std::lock_guard lock(m_mutex);
            if (!m_versions.validate(h)) return;
            ++m_ref_counts[h.ofs / sizeof(T)];
        }

        void release(Handle h)
        {
            std::lock_guard lock(m_mutex);
            if (!m_versions.validate(h)) return;

            auto& count = m_ref_counts[h.ofs / sizeof(T)];
            if (--count == 0)
            {
                remove_unlocked(h); // safe: avoids re-locking
            }
        }

        uint32_t use_count(Handle h) const
        {
            std::lock_guard lock(m_mutex);
            if (!m_versions.validate(h)) return 0;
            return m_ref_counts[h.ofs / sizeof(T)];
        }

        bool valid(Handle h) const
        {
            std::lock_guard lock(m_mutex);
            return m_versions.validate(h);
        }

        Guid guid_of(Handle h) const
        {
            std::lock_guard lock(m_mutex);
            auto it = m_handle_to_guid.find(h);
            if (it != m_handle_to_guid.end()) return it->second;
            return Guid::invalid();
        }

        // void bind_guid(Handle h, Guid guid)
        // {
        //     std::lock_guard lock(m_mutex);
        //     m_guid_map[guid] = h;
        //     m_handle_to_guid[h] = guid;
        // }

        Handle find_by_guid(Guid guid) const
        {
            std::lock_guard lock(m_mutex);
            auto it = m_guid_map.find(guid);
            return (it != m_guid_map.end()) ? it->second : Handle{};
        }

    private:
        void remove_unlocked(Handle h)
        {
            if (!m_versions.validate(h)) return;

            m_pool.destroy<T>(h);
            m_versions.remove(h);
            m_ref_counts[h.ofs / sizeof(T)] = 0;
        }

        void ensure_metadata(Handle h)
        {
            size_t i = h.ofs / sizeof(T);
            if (i >= m_ref_counts.size())
            {
                m_ref_counts.resize(i + 1, 0);
                m_versions.resize((i + 1) * sizeof(T));
            }
        }

        PoolAllocatorFH m_pool;
        VersionMap<T> m_versions;
        std::vector<uint32_t> m_ref_counts;

        std::unordered_map<Guid, Handle> m_guid_map;
        std::unordered_map<Handle, Guid> m_handle_to_guid;

        mutable std::mutex m_mutex;
    };


    class ResourceRegistry
    {
    public:
        template<typename T, typename... Args>
        Handle<T> add(const Guid& guid, Args&&... args)
        {
            return get_or_create_pool<T>()->add(guid, std::forward<Args>(args)...);
        }

        template<typename T>
        T& get(Handle<T> h)
        {
            return get_pool<T>()->get(h);
        }

        template<typename T>
        void remove(Handle<T> h)
        {
            get_pool<T>()->remove(h);
        }

        template<typename T>
        void retain(Handle<T> h)
        {
            get_pool<T>()->retain(h);
        }

        template<typename T>
        void release(Handle<T> h)
        {
            get_pool<T>()->release(h);
        }

        template<typename T>
        uint32_t use_count(Handle<T> h) const
        {
            return get_pool<T>()->use_count(h);
        }

        template<typename T>
        bool valid(Handle<T> h) const
        {
            return get_pool<T>()->valid(h);
        }

        template<typename T, typename F>
        void for_all(F&& f)
        {
            get_pool<T>()->for_each(std::forward<F>(f));
        }

        template<typename T>
        Handle<T> find_by_guid(Guid guid) const
        {
            return get_pool<T>()->find_by_guid(guid);
        }

        // template<typename T>
        // void bind_guid(Handle<T> h, Guid g)
        // {
        //     get_pool<T>()->bind_guid(h, g);
        // }

    private:
        std::unordered_map<std::type_index, std::unique_ptr<IResourcePool>> pools;

        template<typename T>
        ResourcePool<T>* get_pool()
        {
            auto it = pools.find(std::type_index(typeid(T)));
            if (it == pools.end())
                throw std::runtime_error("Resource type not registered");
            return static_cast<ResourcePool<T>*>(it->second.get());
        }

        template<typename T>
        const ResourcePool<T>* get_pool() const
        {
            auto it = pools.find(std::type_index(typeid(T)));
            if (it == pools.end())
                throw std::runtime_error("Resource type not registered");
            return static_cast<const ResourcePool<T>*>(it->second.get());
        }

        template<typename T>
        ResourcePool<T>* get_or_create_pool()
        {
            auto type = std::type_index(typeid(T));
            auto it = pools.find(type);
            if (it == pools.end())
            {
                auto pool = std::make_unique<ResourcePool<T>>();
                auto* raw_ptr = pool.get();
                pools[type] = std::move(pool);
                return raw_ptr;
            }
            return static_cast<ResourcePool<T>*>(it->second.get());
}
    };
#endif

} // namespace eeng

// namespace entt {
//   template<>
//   struct is_meta_pointer_like<eeng::Storage*> : std::true_type {};
// }