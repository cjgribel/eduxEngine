// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "Game.hpp"
#include "glmcommon.hpp"
#include "imgui.h"
#include "LogMacros.h"
#include <entt/entt.hpp>
#include <glm/gtc/type_ptr.hpp>

// FOR TESTS ->
#include "MainThreadQueue.hpp"
#include "AssimpImporter.hpp" // --> ENGINE API
#include "MockImporter.hpp" // --> ENGINE API

#include "ecs/ModelComponent.hpp"

#include "ecs/MockComponents.hpp"
#include "mock/MockTypes.hpp"
#include "mock/MockAssetTypes.hpp"
#include "mock/CopySignaller.hpp"

#include "ResourceManager.hpp" // Since we use the concrete type
#include "BatchRegistry.hpp" // Since we use the concrete type
#include "MetaSerialize.hpp"
#include <filesystem>
// <-

namespace {

#if 0
    // Log all registered resource types
    void logRegisteredResourceTypes(eeng::ResourceRegistry& registry)
    {
        EENG_LOG("Registered resource types");
        EENG_LOG("Meshes:");
        registry.for_all<eeng::Mesh>([](eeng::Mesh& m) {
            EENG_LOG("Mesh value %zu", m.x);
            });
    }
#endif
}

namespace eeng::dev
{

    void schedule_startup_import_and_scan(
        const std::filesystem::path& asset_root,
        const std::filesystem::path& batches_root,
        std::shared_ptr<eeng::EngineContext> ctx)
    {
        using ModelRef = eeng::AssetRef<eeng::mock::Model>;

        // 1) define task
        auto startup_task = [asset_root, batches_root, ctx]() {
            // If logging must be on main, do it here:
            // ctx->main_thread_queue->push([ctx]() {
            EENG_LOG(ctx.get(), "[startup] Begin startup worker on main...");
            // });

        // 2) run orchestrator on worker pool
            ctx->thread_pool->queue_task([asset_root, batches_root, ctx]() {
                try
                {
#if 1
                    // Just scan & load existing batch index

                    // Scan assets
                    EENG_LOG(ctx.get(), "[startup] Waiting for scan to complete before adding components...");
                    auto scan_fut = ctx->resource_manager->scan_assets_async(asset_root, *ctx);

                    // Create/load batch index
                    EENG_LOG(ctx.get(), "[startup] Loading/Creating batch index at: %s", batches_root.string().c_str());
                    auto& br = static_cast<eeng::BatchRegistry&>(*ctx->batch_registry);
                    br.load_or_create_index(batches_root / "index.json");

                    // Wait for scan to finish
                    scan_fut.get();
                    EENG_LOG(ctx.get(), "[startup] Scan done.");
#else
                    constexpr int num_tasks = 3;
                    std::vector<std::future<ModelRef>> futures;
                    futures.reserve(num_tasks);
                    EENG_LOG(ctx.get(), "[startup] Importing models on worker...");

                    // 2.1) Run import tasks
                    for (int i = 0; i < num_tasks; ++i)
                    {
                        futures.emplace_back(
                            ctx->thread_pool->queue_task([asset_root, ctx]()
                                {
                                    return eeng::mock::ModelImporter::import(asset_root, ctx);
                                })
                        );
                    }
                    // Wait for import tasks to complete
                    std::vector<ModelRef> refs;
                    refs.reserve(num_tasks);
                    for (auto& f : futures) refs.push_back(f.get());

                    // 2.2) Import a quads model (this thread)
                    EENG_LOG(ctx.get(), "[startup] Importing quads model on worker...");
                    auto quadmodel_ref = eeng::mock::ModelImporter::import_quads_modeldata(asset_root, ctx);

                    // 3) Run scan (don't wait)
                    auto nbr_assets = refs.size();
                    EENG_LOG(ctx.get(), "[startup] Imported %zu models. Scanning...", nbr_assets);
                    auto scan_fut = ctx->resource_manager->scan_assets_async(asset_root, *ctx);

                    // Create a few batches
                    // Add a few entities to each batch
                    // Let entities,
                    //  - reference other entities (EntityRef)
                    //  - reference assets (AssetRef<T>)
                    //
                    // Note: impport has finished at this point, but scanning may be ongoing
                    // Defer loading to GUI command etc
                    //
                    // auto& br = ctx->batch_registry;
                    auto& br = static_cast<eeng::BatchRegistry&>(*ctx->batch_registry);
                    // Save empty to reset index
                    EENG_LOG(ctx.get(), "[startup] Creating batch index at: %s", batches_root.string().c_str());
                    br.load_or_create_index(batches_root / "index.json");
                    /* + DURING APP INIT */ //br.load_index_from_json(batches_root / "index.json");
                    //
                    auto batch_id1 = br.create_batch("Startup Batch 1"); assert(batch_id1.valid());
                    //
                    // LOADING BATCHES
                    //
                    EENG_LOG(ctx.get(), "[startup] Loading batch 1...");
                    br.queue_load(batch_id1, *ctx).get();
                    EENG_LOG(ctx.get(), "[startup] Loading all batches...");
                    br.queue_load_all_async(*ctx).get();
                    //
                    // SAVING BATCHES
                    //
                    EENG_LOG(ctx.get(), "[startup] Saving batch 1...");
                    br.queue_save_batch(batch_id1, *ctx).get();
                    EENG_LOG(ctx.get(), "[startup] Saving all batches...");
                    br.queue_save_all_async(*ctx).get();
                    //
                    // ADD ENTITIES TO BATCH
                    // NOTE w.r.t. SCAN: We're adding entities to a loaded batch (as we must);
                    //      if load_and_bind_async is called when an entity is added,
                    //      then the assets must be available to RM (i.e. scan must have completed).
                    //      Otherwise, load will fail for entities unknown assets.
                    EENG_LOG(ctx.get(), "[startup] Creating entities...");
                    {
                        // 1) Create entities new entity in batch (HeaderComponent added automatically)
                        auto root_fut = br.queue_create_entity(batch_id1, "Root", eeng::ecs::EntityRef{}, *ctx);
                        ecs::EntityRef er_root = root_fut.get();
                        auto player_fut = br.queue_create_entity(batch_id1, "Player", er_root, *ctx);
                        auto camera_fut = br.queue_create_entity(batch_id1, "Camera", er_root, *ctx);
                        ecs::EntityRef er_player = player_fut.get();
                        ecs::EntityRef er_camera = camera_fut.get();

                        // Wait for scan to finish before assigning assets
                        EENG_LOG(ctx.get(), "[startup] Waiting for scan to complete before adding components...");
                        scan_fut.get();
                        EENG_LOG(ctx.get(), "[startup] Scan done.");

                        // 2) Add mock components on main thread
#if 1
                        ctx->main_thread_queue->push_and_wait([&]()
                            {
                                EENG_LOG(ctx.get(), "[startup] Adding components to entities on main...");

                                auto registry_sptr = ctx->entity_manager->registry_wptr().lock();
                                if (!registry_sptr || !er_player.is_bound() || !er_camera.is_bound())
                                    return;

                                auto& reg = *registry_sptr;

                                auto player_model_ref = refs[0]; // 
                                auto camera_model_ref = refs[1];
                                ecs::mock::MockPlayerComponent player_comp{ 1.0f, 2.0f, er_camera, player_model_ref };
                                ecs::mock::MockCameraComponent camera_comp{ 3.0f, 4.0f, er_player, camera_model_ref };
                                reg.emplace<ecs::mock::MockPlayerComponent>(er_player.entity, player_comp);
                                reg.emplace<ecs::mock::MockCameraComponent>(er_camera.entity, camera_comp);

                                reg.emplace<ecs::mock::MockMixComponent>(er_player.entity);
                                reg.emplace<ecs::mock::MockMixComponent>(er_camera.entity);

                                reg.emplace<eeng::CopySignaller>(er_root.entity);

                                reg.emplace<eeng::ecs::ModelComponent>(er_player.entity, "Model", quadmodel_ref);

                                // If Position has AssetRef<T> inside,
                                // either:
                                //  - call a BR helper to update closure, or
                                //  - rely on a later “recompute closure” pass.
                                EENG_LOG(ctx.get(), "[startup] Done adding components...");
                            });
#endif
                    }
                    //
                    EENG_LOG(ctx.get(), "[startup] Saving all batches...");
                    br.queue_save_all_async(*ctx).get();
                    //
                    EENG_LOG(ctx.get(), "[startup] Saving batch index...");
                    br.save_index(batches_root / "index.json");
#endif
                }
                catch (const std::exception& ex)
                {
                    // push error log to main (safe)
                    // ctx->main_thread_queue->push([ctx, msg = std::string(ex.what())]()
                    //     {
                    EENG_LOG(ctx.get(), "[startup] ERROR during import: %s", ex.what());
                    // });
                }
                });
            };

        // 3) schedule on main
        ctx->main_thread_queue->push(std::move(startup_task));
    }

    // void schedule_startup_batch_setup()
    // {


    //     // TODO

    //     auto task = [asset_root, ctx]() {
    //         EENG_LOG(ctx.get(), "[startup] Begin startup import+scan task on main...");
    //         };
    // }

} // namespace eeng::dev


bool Game::init()
{
    forwardRenderer = std::make_shared<eeng::ForwardRenderer>();
    forwardRenderer->init("shaders/phong_vert.glsl", "shaders/phong_frag.glsl");

    shapeRenderer = std::make_shared<ShapeRendering::ShapeRenderer>();
    shapeRenderer->init();

    renderSystem = std::make_unique<eeng::ecs::systems::RenderSystem>();
    renderSystem->init("shaders/phong_vert.glsl", "shaders/phong_frag.glsl");

    // LEVEL CYCLE API TESTS
    {
        /*
        Potential design:
        - One function does all of the below
        -

        1. Load a SET OF ENTITIES given by a batch / level file etc
            Use some dummy component that tests inspectble types & references another entity
        2. Bind component ENTITY references
        One function to load & bind all entities in the set
        This is probably serial; maybe file IO & parsing is done async

        3. Collect ASSET GUIDs from entities

        4. Load & bind ASSETS (ASYNC)

        5. Bind component ASSET references (SERIAL)

        6. Use entities & assets "in game"

        7. Detroy entities & assets
        */

        // === ECS PHASE ===

        std::cout << "ECS API TESTS..." << std::endl;

        // 1. Create entities
#if 0
        std::vector<eeng::ecs::Entity> entities;
        for (int i = 0; i < 5; ++i)
        {
            auto [guid, entity] = ctx->entity_manager->create_entity("", "Entity " + std::to_string(i), eeng::ecs::Entity::EntityNull, eeng::ecs::Entity{});
            // Add components. Leave GUIDs empty.

            EENG_LOG(ctx, "[Game::init()] Created entity: %i", entity.to_integral());
            entities.push_back(entity);
    }
#endif

        // 2. ...

        // 3.
        // We cannot bind asset references yet
        // Fetch asset GUIDs once they are loaded

        // 4.
        // === RESOURCE PHASE ===

        std::cout << "RESOURCE MANAGER API TESTS..." << std::endl;

        // TODO: Ugly cast to concrete type from interface
        // - Use interface for parts of ResourceManager not part of IResourceManager (load etc)
        auto& resource_manager = static_cast<eeng::ResourceManager&>(*ctx->resource_manager);
        std::filesystem::path asset_root = "/Users/ag1498/GitHub/eduxEngine/Module1/project1/imported_assets/";
        std::filesystem::path batches_root = "/Users/ag1498/GitHub/eduxEngine/Module1/project1/batches/";
        //std::filesystem::path asset_root = "C:/Users/Admin/source/repos/eduEngine/Module1/project1/imported_assets/";

        // 1.   IMPORT resources concurrently
        //
        // using ModelRef = eeng::AssetRef<eeng::mock::Model>;
        // std::vector<ModelRef> refs;
        // std::vector<std::future<ModelRef>> futures;
        // {
        //     // Requires ResourceManager to be TS
        //     std::cout << "Importing assets recursively..." << std::endl;
        //     const int numTasks = 10;
        //     for (int i = 0; i < numTasks; ++i)
        //     {
        //         futures.emplace_back(
        //             ctx->thread_pool->queue_task([this, &asset_root]()
        //                 {
        //                     return eeng::mock::ModelImporter::import(asset_root, ctx);
        //                 })
        //         );
        //     }
        //     // BLOCK and wait for imports
        //     EENG_LOG(ctx, "[Game::init()] Wait for imports...");
        //     for (auto& f : futures) refs.push_back(f.get());
        // }

#if 1
        // 3. SCAN assets from disk concurrently
        //

        // Scan on the main thread later
        // ctx->main_thread_queue->push([&]() {
        //     EENG_LOG(ctx, "[MainThread] Scanning assets...");
        //     resource_manager.scan_assets_async("/Users/ag1498/GitHub/eduEngine/Module1/project1/imported_assets/", *ctx);
        //     });
// eeng::dev::import_then_scan_async(asset_root, ctx);
        eeng::dev::schedule_startup_import_and_scan(asset_root, batches_root, ctx);
        {
#if 0
            EENG_LOG(ctx, "[Game::init()] Scanning assets...");
            resource_manager.scan_assets_async("/Users/ag1498/GitHub/eduEngine/Module1/project1/imported_assets/", *ctx);
#endif
            //resource_manager.start_async_scan("C:/Users/Admin/source/repos/eduEngine/Module1/project1/imported_assets/", *ctx);
            // Wait for scanning to finish
            // while (resource_manager.is_scanning())
#if 0
            while (resource_manager.is_busy())
            {
                // Wait for scanning to finish
                std::cout << "Scanning assets..." << std::endl;
                EENG_LOG(ctx, "[Game::init()] Waiting for asset scan to finish...");
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                // std::this_thread::yield(); // Yield to other threads
}
#endif
            // Get asset index snapshot and log it
            // auto asset_index = resource_manager.get_asset_entries_snapshot();
            // EENG_LOG(ctx, "[Game::init()] Found %zu assets:", asset_index.size());
            // for (const auto& entry : asset_index)
            // {
            //     EENG_LOG(ctx, "  - %s (%s) at %s",
            //         entry.meta.name.c_str(),
            //         entry.meta.type_name.c_str(),
            //         entry.relative_path.string().c_str());
            // }
        }
#endif

#if 0
        // Enqueue dummy tasks to main thread 
        ctx->main_thread_queue->push([&]() {
            EENG_LOG(ctx, "[MainThreadQueue] task executed on main thread.");
            });
        ctx->main_thread_queue->push([&]() {
            EENG_LOG(ctx, "[MainThreadQueue] another task executed on main thread.");
            });
        // Enqueue dummy tasks to main thread - BLOCKING
        ctx->main_thread_queue->push_and_wait([&]() -> void {
            EENG_LOG(ctx, "[MainThreadQueue] blocking task executed on main thread.");
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            EENG_LOG(ctx, "[MainThreadQueue] blocking task finished.");
            });
#endif

        /*
        TODO
        0 Introduce DUMMY COMPONENTS, e.g. one with a AssetRef<Model>
            Instead of the 'refs' vector above
        1 Track futures for loaded assets
        2 In update(), when all are loaded: bind to components ()
        */

        // 4. LOAD assets concurrently
        // -> "collected asset GUIDs" -> auto fut = ctx->resource_manager->load_and_bind_async();
#if 0
        std::deque<eeng::Guid> branch_guids;
        for (const auto& ref : refs) branch_guids.push_back(ref.get_guid());
        {
#if 0
            // CREATE DEQUE AND LOAD USING load_and_bind_async
            EENG_LOG(ctx, "[Game::init()] Loading assets asynchronously...");
            auto batch_id = eeng::Guid::generate();
            auto fut = resource_manager.load_and_bind_async(branch_guids, batch_id, *ctx);
            // Wait for all assets to load
            // try {
            //     fut.get(); // Blocks until all assets are loaded and bound
            // } catch (const std::exception& ex) {
            //     EENG_LOG(ctx, "[Game::init()] Error loading assets: %s", ex.what());
            //     //return false; // Handle error appropriately
            // }
            //auto result = fut.get(); // Blocks until all assets are loaded and bound
            // Log result
            // for (const auto& op : result.results)
            // {
            //     if (!op.success)
            //         EENG_LOG(ctx, "Load error: GUID %s: %s", op.guid.to_string().c_str(), op.message.c_str());
            // }
            // EENG_LOG(ctx, "[Game::init()] Assets loaded and bound.");
#endif
#if 0
        // UNLOAD (NOT HERE)
            {
                EENG_LOG(ctx, "[Game::init()] Unloading assets asynchronously...");
                auto fut = resource_manager.unbind_and_unload_async(branch_guids, *ctx);
                auto result = fut.get(); // Blocks until all assets are unbound & unloaded
                // Log result
                for (const auto& op : result.results)
                {
                    if (!op.success) {
                        EENG_LOG(ctx, "Unload error: GUID %s: %s", op.guid.to_string().c_str(), op.message.c_str());
                    }
                }
            }
#endif
#if 0
            EENG_LOG(ctx, "[Game::init()] Loading assets...");
            // for (auto& ref : refs) resource_manager.load(ref, *ctx);
            // CONCURRENTLY
            std::vector<std::future<bool>> load_futures;
            for (auto& ref : refs) {
                load_futures.emplace_back(
                    ctx->thread_pool->queue_task([this, &resource_manager, &ref]() {
                        resource_manager.load(ref, *ctx);
                        })
                );
            }
            // Wait for all loads to finish
            for (auto& future : load_futures) future.get();
            // Verify content
            EENG_LOG(ctx, "[Game::init()] Loaded %zu assets", refs.size());
            for (const auto& ref : refs)
            {
                assert(ref.is_loaded());
                EENG_LOG(ctx, "  - Model .. with guid %s",
                    // m.to_string().c_str(),
                    ref.get_guid().to_string().c_str());
                auto m = resource_manager.storage().get_ref<eeng::mock::Model>(ref.get_handle());
                for (const auto& mesh_ref : m.meshes)
                {
                    assert(mesh_ref.is_loaded());
                    auto mesh = resource_manager.storage().get_ref<eeng::mock::Mesh>(mesh_ref.get_handle());
                    EENG_LOG(ctx, "  - Mesh .. with guid %s",
                        // mesh.get()->to_string().c_str(),
                        mesh_ref.get_guid().to_string().c_str());
                    assert(mesh.vertices[0] == 1.0f && mesh.vertices[1] == 2.0f && mesh.vertices[2] == 3.0f);
                    for (const auto& v : mesh.vertices)
                        EENG_LOG(ctx, "    - Vertex: %f", v);
                }
            }
#endif
        }
#endif

        // (3. When loading future is done - set entity asset GUIDs)

        // 5. BIND entity asset references

        // 6. USE entities:
        //      - Use views and get() components
        //      - Fetch referenced entities
        //      - Fetch referenced assets


        // 7. Destroy entities
#if 0
        for (const auto& entity : entities)
        {
            assert(ctx->entity_manager->entity_valid(entity));
            ctx->entity_manager->queue_entity_for_destruction(entity);
        }
        auto nbr_destroyed = ctx->entity_manager->destroy_pending_entities();
        EENG_LOG(ctx, "[Game::init()] Destroyed %i entities", nbr_destroyed);
#endif

        // 7. UNLOAD assets (concurrently)
        //      CAN BE MADE TS // storage->get_ref - NOT TS
        //      
#if 0
        {
            std::cout << "Unloading assets..." << std::endl;
            // SERIALLY
            //for (auto& ref : refs) resource_manager.unload(ref);
            // CONCURRENTLY
            std::vector<std::future<void>> load_futures;
            for (auto& ref : refs) {
                load_futures.emplace_back(
                    ctx->thread_pool->queue_task([this, &resource_manager, &ref]() {
                        resource_manager.unload(ref, *ctx);
                        })
                );
            }
            // Wait for all loads to finish
            for (auto& future : load_futures) future.get();
        }
#endif

        // GUI: import. load, unload, unimport ...
    }

#if 0
    // Thread pool test 1
    {
        std::cout << "Thread pool test 1..." << std::endl;

        ThreadPool pool{ 4 };

        // Define your per-frame update/render as a lambda (runs on main thread)
        auto game_update_and_render = [] {
            // … physics, input, drawing, etc. …
            std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60 FPS
            };

        // 1) Prepare N tasks and queue them
        const int numTasks = 5;
        std::vector<std::future<int>> futures;
        futures.reserve(numTasks);
        for (int i = 0; i < numTasks; ++i) {
            futures.emplace_back(
                pool.queue_task([i] {
                    // simulate variable work time
                    std::this_thread::sleep_for(std::chrono::seconds(i + 1));
                    return i * i;
                    })
            );
        }

        // Track which results we've already processed
        std::vector<bool> done(numTasks, false);
        int remaining = numTasks;

        // 2) Main loop: run frames and poll futures with zero timeout
        while (remaining > 0) {
            // a) Do one frame’s worth of work
            game_update_and_render();

            // b) Check each future without blocking
            for (int i = 0; i < numTasks; ++i) {
                if (!done[i] &&
                    futures[i].wait_for(std::chrono::milliseconds(0))
                    == std::future_status::ready)
                {
                    // Task i finished—get its result and mark done
                    int value = futures[i].get();
                    std::cout << "Task " << i << " result: " << value << "\n";
                    done[i] = true;
                    --remaining;
                }
            }
        }

        // 3) All tasks are done; ThreadPool destructor will shut down workers

    }
#endif
#if 0
    // Thread test 2
    {
        std::cout << "Thread pool test 2..." << std::endl;

        // 1) Create a packaged_task that simulates work by sleeping
        std::packaged_task<int()> task([] {
            std::this_thread::sleep_for(std::chrono::seconds(3));  // simulate 3s of work
            return 42;
            });

        // 2) Get its future
        std::future<int> result = task.get_future();

        // 3) Run the task on a worker thread
        std::thread worker(std::move(task));

        // 4) Meanwhile, do other work in main thread
        while (result.wait_for(std::chrono::milliseconds(500))
            != std::future_status::ready)
        {
            std::cout << "Main thread is doing other work...\n";
        }

        // 5) Task is done—retrieve and print its result
        std::cout << "Task completed, result = " << result.get() << "\n";

        // 6) Clean up
        worker.join();
        // return 0;
    }
#endif
#if 0
    // Thread test 3
    {
        std::cout << "Thread pool test 3..." << std::endl;

        std::packaged_task<int()> task([] {
            // This is the “work” the task will do
            return 6 * 7;
            });

        // 2) Extract the future associated with the task’s result
        std::future<int> result = task.get_future();

        // 3) Launch the task on a separate thread
        std::thread worker(std::move(task));

        // (Optional) do other work here...

        // 4) Block until the task is done and get its return value
        std::cout << "The answer is: " << result.get() << "\n";

        // 5) Clean up
        worker.join();
    }
#endif

    // RESOURCE REGISTRY TEST
#if 0
    {
        EENG_LOG(ctx, "Storage test begins");
        eeng::Storage storage; // <- Engine API

        // RUNTIME

        // "Import new resource" (GUI)
        eeng::mock::MockResource1 mockResource1; // <- IMPORTER
        // resource -> AssetIndex -> Serialize

        // Add/Load resource - by NAME or GUID"
        // 1. AssetIndex -> meta_any resource -> ResourceRegistry/Storage
        // ...
        entt::meta_any resource1 = mockResource1;
        eeng::Guid guid1 = eeng::Guid::generate(); // Fake guid (from AssetIndex)
        eeng::Guid guid2 = eeng::Guid::generate(); // Fake guid (from AssetIndex)
        eeng::Guid guid3 = eeng::Guid::generate(); // Fake guid (from AssetIndex)
        // 2. meta_any resource -> ResourceRegistry/Storage:
        eeng::MetaHandle meta_handle1 = storage.add(resource1, guid1); // copy
        eeng::MetaHandle meta_handle2 = storage.add(resource1, guid2); // copy
        eeng::MetaHandle meta_handle3 = storage.add(std::move(resource1), guid3); // move

        // Get & "use" resource
        // Cast meta_any to resource type
        // ...
        EENG_LOG(ctx, "get resource 1");
        {
            // Non-const get and mutation
            entt::meta_any meta_any1 = storage.get(meta_handle1);
            assert(meta_any1.base().policy() == entt::any_policy::ref);
            auto& val = meta_any1.cast<eeng::mock::MockResource1&>();
            val.x = 5;
        }
        {
            // get const
            const auto& cstorage = storage;
            entt::meta_any meta_any1 = cstorage.get(meta_handle1);
            assert(meta_any1.base().policy() == entt::any_policy::cref);
            const auto& cval = meta_any1.cast<const eeng::mock::MockResource1&>();
            assert(cval.x == 5);
        }
        {
            // try_get for valid handle
            if (auto maybe_meta_any = storage.try_get(meta_handle1)) {
                // got a valid meta_any
                auto& meta_any1 = *maybe_meta_any;
                assert(meta_any1.base().policy() == entt::any_policy::ref);
                auto& val = meta_any1.cast<eeng::mock::MockResource1&>();
                val.x = 10;
                EENG_LOG(ctx, "Valid resource found");
            }
            else assert(0 && "Valid should exist");
        }
        {
            // try_get const for valid handle
            const auto& cstorage = storage;
            if (auto maybe_meta_any = cstorage.try_get(meta_handle1)) {
                // got a valid meta_any
                auto& meta_any1 = *maybe_meta_any;
                assert(meta_any1.base().policy() == entt::any_policy::cref);
                const auto& cval = meta_any1.cast<const eeng::mock::MockResource1&>();
                assert(cval.x == 10);
                EENG_LOG(ctx, "Valid resource found");
            }
            else assert(0 && "Valid should exist");
        }
        {
            // try_get for invalid handle
            eeng::MetaHandle meta_handle{};
            if (auto maybe_meta_any = storage.try_get(meta_handle)) {
                assert(0 && "Invalid handle found");
            }
            else { EENG_LOG(ctx, "Invalid resource not found"); };
        }

        // Remove resource from ResourceRegistry/Storage
        // WHO DOES THIS
        // ...

        /*
        Retaining & Releasing

        auto h = storage.add(...);
        // two independent owners grab it:
        storage.retain(h);
        storage.retain(h);

        // first release → leaves count==2
        assert(storage.release(h) == 2);

        // next two releases → will hit zero and automatically destroy
        assert(storage.release(h) == 1);
        assert(storage.release(h) == 0);
        // by now the pool has removed the entry altogether

        */

        EENG_LOG(ctx, "Storage test ends");
    }
#endif
#if 0
    {
        // "Import"
        // Importer assigns resource GUID
        // Could be concurrently
        eeng::Mesh mesh1, mesh2;
        mesh1.x = 0; mesh2.x = 1;
        eeng::Guid guid1 = eeng::Guid::generate();
        eeng::Guid guid2 = eeng::Guid::generate();
        // mesh1.guid = guid1; STORE GUID IN RESOURCE?
        // mesh2.guid = guid2;

        eeng::ResourceRegistry registry;
        auto h1 = registry.add<eeng::Mesh>(guid1, mesh1);
        auto h2 = registry.add<eeng::Mesh>(guid2, mesh2);

        {
            auto& mesh = registry.get(h1);
            mesh.x = 10;
            auto& mesh2 = registry.get(h2);
            mesh2.x = 20;
        }

        logRegisteredResourceTypes(registry);

        registry.release(h1); // decrease ref count; if zero, free resource
        registry.release(h2); // decrease ref count; if zero, free resource
        // registry.remove(h); // 

        logRegisteredResourceTypes(registry);
    }
#endif

    // Do some entt stuff
    // entity_registry = std::make_shared<entt::registry>();
    // entity_registry->storage<int>();
    // auto ent1 = entity_registry->create();
    // struct Tfm
    // {
    //     float x, y, z;
    // };
    // entity_registry->emplace<Tfm>(ent1, Tfm{});

    // Grass
    grassMesh = std::make_shared<eeng::RenderableMesh>();
    grassMesh->load("assets/grass/grass_trees_merged.fbx", false);

    // Horse
    horseMesh = std::make_shared<eeng::RenderableMesh>();
    horseMesh->load("assets/Animals/Horse.fbx", false);

    // Character
    characterMesh = std::make_shared<eeng::RenderableMesh>();
#if 0
    // Character
    characterMesh->load("assets/Ultimate Platformer Pack/Character/Character.fbx", false);
#endif
#if 0
    // Enemy
    characterMesh->load("assets/Ultimate Platformer Pack/Enemies/Bee.fbx", false);
#endif
#if 0
    // ExoRed 5.0.1 PACK FBX, 60fps, No keyframe reduction
    characterMesh->load("assets/ExoRed/exo_red.fbx");
    characterMesh->load("assets/ExoRed/idle (2).fbx", true);
    characterMesh->load("assets/ExoRed/walking.fbx", true);
    // Remove root motion
    characterMesh->removeTranslationKeys("mixamorig:Hips");
#endif
#if 1
    // Amy 5.0.1 PACK FBX
    characterMesh->load("assets/Amy/Ch46_nonPBR.fbx");
    characterMesh->load("assets/Amy/idle.fbx", true);
    characterMesh->load("assets/Amy/walking.fbx", true);
    // Remove root motion
    characterMesh->removeTranslationKeys("mixamorig:Hips");
#endif
#if 0
    // Eve 5.0.1 PACK FBX
    // Fix for assimp 5.0.1 (https://github.com/assimp/assimp/issues/4486)
    // FBXConverter.cpp, line 648: 
    //      const float zero_epsilon = 1e-6f; => const float zero_epsilon = Math::getEpsilon<float>();
    characterMesh->load("assets/Eve/Eve By J.Gonzales.fbx");
    characterMesh->load("assets/Eve/idle.fbx", true);
    characterMesh->load("assets/Eve/walking.fbx", true);
    // Remove root motion
    characterMesh->removeTranslationKeys("mixamorig:Hips");
#endif

#ifdef SPONZA_PATH
    sponzaMesh = std::make_shared<eeng::RenderableMesh>();
    sponzaMesh->load(SPONZA_PATH, false);
#endif
#ifdef CHARACTER_PATH
    qcharacterMesh = std::make_shared<eeng::RenderableMesh>();
    qcharacterMesh->load(CHARACTER_PATH);
#endif
#ifdef ENEMY_PATH
    enemyMesh = std::make_shared<eeng::RenderableMesh>();
    enemyMesh->load(ENEMY_PATH);
#endif
#ifdef EXORED_PATH
    exoredMesh = std::make_shared<eeng::RenderableMesh>();
    exoredMesh->load(EXORED_PATH);
    exoredMesh->load(EXORED_ANIM0_PATH, true);
    exoredMesh->removeTranslationKeys("mixamorig:Hips");
#endif
#ifdef EVE_PATH
    eveMesh = std::make_shared<eeng::RenderableMesh>();
    eveMesh->load(EVE_PATH);
    eveMesh->load(EVE_ANIM0_PATH, true);
    eveMesh->removeTranslationKeys("mixamorig:Hips");
#endif
#ifdef MANNEQUIN_PATH
    mannequinMesh = std::make_shared<eeng::RenderableMesh>();
    mannequinMesh->load(MANNEQUIN_PATH);
    mannequinMesh->load(MANNEQUIN_ANIM0_PATH, true);
#endif
#ifdef UE5QUINN_PATH
    ue5quinnMesh = std::make_shared<eeng::RenderableMesh>();
    ue5quinnMesh->load(UE5QUINN_PATH);
    ue5quinnMesh->load(UE5QUINN_ANIM0_PATH, true);
#endif

    grassWorldMatrix = glm_aux::TRS(
        { 0.0f, 0.0f, 0.0f },
        0.0f, { 0, 1, 0 },
        { 100.0f, 100.0f, 100.0f });

    horseWorldMatrix = glm_aux::TRS(
        { 30.0f, 0.0f, -35.0f },
        35.0f, { 0, 1, 0 },
        { 0.01f, 0.01f, 0.01f });

    return true;
}

void Game::update(
    float time,
    float deltaTime)
{
    updateCamera();

    updatePlayer(deltaTime);

    pointlight.pos = glm::vec3(
        glm_aux::R(time * 0.1f, { 0.0f, 1.0f, 0.0f }) *
        glm::vec4(100.0f, 100.0f, 100.0f, 1.0f));

    characterWorldMatrix1 = glm_aux::TRS(
        player.pos,
        0.0f, { 0, 1, 0 },
        { 0.03f, 0.03f, 0.03f });

    characterWorldMatrix2 = glm_aux::TRS(
        { -3, 0, 0 },
        time * glm::radians(50.0f), { 0, 1, 0 },
        { 0.03f, 0.03f, 0.03f });

    characterWorldMatrix3 = glm_aux::TRS(
        { 3, 0, 0 },
        time * glm::radians(50.0f) * 0, { 0, 1, 0 },
        { 0.03f, 0.03f, 0.03f });

    // Intersect player view ray with AABBs of other objects 
    glm_aux::intersect_ray_AABB(player.viewRay, character_aabb2.min, character_aabb2.max);
    glm_aux::intersect_ray_AABB(player.viewRay, character_aabb3.min, character_aabb3.max);
    glm_aux::intersect_ray_AABB(player.viewRay, horse_aabb.min, horse_aabb.max);

    // We can also compute a ray from the current mouse position,
    // to use for object picking and such ...
    if (ctx->input_manager->GetMouseState().rightButton)
    {
        glm::ivec2 windowPos(camera.mouse_xy_prev.x, matrices.windowSize.y - camera.mouse_xy_prev.y);
        auto ray = glm_aux::world_ray_from_window_coords(windowPos, matrices.V, matrices.P, matrices.VP);
        // Intersect with e.g. AABBs ...

        EENG_LOG(ctx, "Picking ray origin = %s, dir = %s",
            glm_aux::to_string(ray.origin).c_str(),
            glm_aux::to_string(ray.dir).c_str());
    }
}

void Game::render(
    float time,
    int windowWidth,
    int windowHeight)
{
    renderUI();

    matrices.windowSize = glm::ivec2(windowWidth, windowHeight);

    // Projection matrix
    const float aspectRatio = float(windowWidth) / windowHeight;
    matrices.P = glm::perspective(glm::radians(60.0f), aspectRatio, camera.nearPlane, camera.farPlane);

    // View matrix
    matrices.V = glm::lookAt(camera.pos, camera.lookAt, camera.up);

    matrices.VP = glm_aux::create_viewport_matrix(0.0f, 0.0f, windowWidth, windowHeight, 0.0f, 1.0f);

    // Begin rendering pass
    forwardRenderer->beginPass(matrices.P, matrices.V, pointlight.pos, pointlight.color, camera.pos);

    // Grass
    forwardRenderer->renderMesh(grassMesh, grassWorldMatrix);
    grass_aabb = grassMesh->m_model_aabb.post_transform(grassWorldMatrix);

    // Horse
    horseMesh->animate(3, time);
    forwardRenderer->renderMesh(horseMesh, horseWorldMatrix);
    horse_aabb = horseMesh->m_model_aabb.post_transform(horseWorldMatrix);

    // Character, instance 1
    characterMesh->animate(characterAnimIndex, time * characterAnimSpeed);
    forwardRenderer->renderMesh(characterMesh, characterWorldMatrix1);
    character_aabb1 = characterMesh->m_model_aabb.post_transform(characterWorldMatrix1);

    // Character, instance 2
    characterMesh->animate(1, time * characterAnimSpeed);
    forwardRenderer->renderMesh(characterMesh, characterWorldMatrix2);
    character_aabb2 = characterMesh->m_model_aabb.post_transform(characterWorldMatrix2);

    // Character, instance 3
    // characterMesh->animate(2, time * characterAnimSpeed);
    characterMesh->animateBlend(1, 2, time, time, characterAnimBlend);
    forwardRenderer->renderMesh(characterMesh, characterWorldMatrix3);
    character_aabb3 = characterMesh->m_model_aabb.post_transform(characterWorldMatrix3);

#ifdef SPONZA_PATH
    forwardRenderer->renderMesh(sponzaMesh, glm::mat4{ 1.0f });
#endif
#ifdef CHARACTER_PATH
    qcharacterMesh->animate(CHARACTER_ANIM, time);
    forwardRenderer->renderMesh(qcharacterMesh, glm_aux::TS({ 0.0f, 0.0f, -5.0f }, { 0.02f, 0.02f, 0.02f }));
#endif
#ifdef ENEMY_PATH
    enemyMesh->animate(ENEMY_ANIM, time);
    forwardRenderer->renderMesh(enemyMesh, glm_aux::TS({ 0.0f, 0.0f, -10.0f }, { 0.02f, 0.02f, 0.02f }));
#endif
#ifdef EXORED_PATH
    exoredMesh->animate(EXORED_ANIM, time);
    forwardRenderer->renderMesh(exoredMesh, glm_aux::TS({ 0.0f, 0.0f, -15.0f }, { 0.05f, 0.05f, 0.05f }));
#endif
#ifdef EVE_PATH
    eveMesh->animate(EVE_ANIM, time);
    forwardRenderer->renderMesh(eveMesh, glm_aux::TS({ 0.0f, 0.0f, -20.0f }, { 0.05f, 0.05f, 0.05f }));
#endif
#ifdef MANNEQUIN_PATH
    mannequinMesh->animate(MANNEQUIN_ANIM, time);
    forwardRenderer->renderMesh(mannequinMesh, glm_aux::TS({ 0.0f, 0.0f, -25.0f }, { 0.05f, 0.05f, 0.05f }));
#endif
#ifdef UE5QUINN_PATH
    ue5quinnMesh->animate(UE5QUINN_ANIM, time);
    forwardRenderer->renderMesh(ue5quinnMesh, glm_aux::TS({ -5.0f, 0.0f, -25.0f }, { 0.05f, 0.05f, 0.05f }));
#endif

    // End rendering pass
    drawcallCount = forwardRenderer->endPass();

    // "New" render system
    if (renderSystem && renderSystem->initialized())
    {
        auto& registry = ctx->entity_manager->registry();
        const auto proj_view = matrices.P * matrices.V;

        renderSystem->render(
            registry,
            *ctx,
            [&](GLuint program)
            {
                glUniformMatrix4fv(
                    glGetUniformLocation(program, "ProjViewMatrix"),
                    1,
                    0,
                    glm::value_ptr(proj_view));
                glUniform3fv(glGetUniformLocation(program, "lightpos"), 1, glm::value_ptr(pointlight.pos));
                glUniform3fv(glGetUniformLocation(program, "lightColor"), 1, glm::value_ptr(pointlight.color));
                glUniform3fv(glGetUniformLocation(program, "eyepos"), 1, glm::value_ptr(camera.pos));
            },
            [&](GLuint program, entt::entity, const eeng::ecs::ModelComponent&)
            {
                const glm::mat4 world = glm::mat4(1.0f);
                glUniformMatrix4fv(
                    glGetUniformLocation(program, "WorldMatrix"),
                    1,
                    0,
                    glm::value_ptr(world));
            });
    }

    // Draw player view ray
    if (player.viewRay)
    {
        shapeRenderer->push_states(ShapeRendering::Color4u{ 0xff00ff00 });
        shapeRenderer->push_line(player.viewRay.origin, player.viewRay.point_of_contact());
    }
    else
    {
        shapeRenderer->push_states(ShapeRendering::Color4u{ 0xffffffff });
        shapeRenderer->push_line(player.viewRay.origin, player.viewRay.origin + player.viewRay.dir * 100.0f);
    }
    shapeRenderer->pop_states<ShapeRendering::Color4u>();

    // Draw object bases
    {
        shapeRenderer->push_basis_basic(characterWorldMatrix1, 1.0f);
        shapeRenderer->push_basis_basic(characterWorldMatrix2, 1.0f);
        shapeRenderer->push_basis_basic(characterWorldMatrix3, 1.0f);
        shapeRenderer->push_basis_basic(grassWorldMatrix, 1.0f);
        shapeRenderer->push_basis_basic(horseWorldMatrix, 1.0f);
    }

    // Draw AABBs
    {
        shapeRenderer->push_states(ShapeRendering::Color4u{ 0xFFE61A80 });
        shapeRenderer->push_AABB(character_aabb1.min, character_aabb1.max);
        shapeRenderer->push_AABB(character_aabb2.min, character_aabb2.max);
        shapeRenderer->push_AABB(character_aabb3.min, character_aabb3.max);
        shapeRenderer->push_AABB(horse_aabb.min, horse_aabb.max);
        shapeRenderer->push_AABB(grass_aabb.min, grass_aabb.max);
        shapeRenderer->pop_states<ShapeRendering::Color4u>();
    }

#if 0
    // Demo draw other shapes
    {
        shapeRenderer->push_states(glm_aux::T(glm::vec3(0.0f, 0.0f, -5.0f)));
        ShapeRendering::DemoDraw(shapeRenderer);
        shapeRenderer->pop_states<glm::mat4>();
}
#endif

    // Draw shape batches
    shapeRenderer->render(matrices.P * matrices.V);
    shapeRenderer->post_render();
    }

void Game::renderUI()
{
    ImGui::Begin("Game Info");

    ImGui::Text("Drawcall count %i", drawcallCount);

    if (ImGui::ColorEdit3("Light color",
        glm::value_ptr(pointlight.color),
        ImGuiColorEditFlags_NoInputs))
    {
    }

    if (characterMesh)
    {
        // Combo (drop-down) for animation clip
        int curAnimIndex = characterAnimIndex;
        std::string label = (curAnimIndex == -1 ? "Bind pose" : characterMesh->getAnimationName(curAnimIndex));
        if (ImGui::BeginCombo("Character animation##animclip", label.c_str()))
        {
            // Bind pose item
            const bool isSelected = (curAnimIndex == -1);
            if (ImGui::Selectable("Bind pose", isSelected))
                curAnimIndex = -1;
            if (isSelected)
                ImGui::SetItemDefaultFocus();

            // Clip items
            for (int i = 0; i < characterMesh->getNbrAnimations(); i++)
            {
                const bool isSelected = (curAnimIndex == i);
                const auto label = characterMesh->getAnimationName(i) + "##" + std::to_string(i);
                if (ImGui::Selectable(label.c_str(), isSelected))
                    curAnimIndex = i;
                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
            characterAnimIndex = curAnimIndex;
        }

        // In-world position label
        const auto VP_P_V = matrices.VP * matrices.P * matrices.V;
        auto world_pos = glm::vec3(horseWorldMatrix[3]);
        glm::ivec2 window_coords;
        if (glm_aux::window_coords_from_world_pos(world_pos, VP_P_V, window_coords))
        {
            ImGui::SetNextWindowPos(
                ImVec2{ float(window_coords.x), float(matrices.windowSize.y - window_coords.y) },
                ImGuiCond_Always,
                ImVec2{ 0.0f, 0.0f });
            ImGui::PushStyleColor(ImGuiCol_WindowBg, 0x80000000);
            ImGui::PushStyleColor(ImGuiCol_Text, 0xffffffff);

            ImGuiWindowFlags flags =
                ImGuiWindowFlags_NoDecoration |
                ImGuiWindowFlags_NoInputs |
                // ImGuiWindowFlags_NoBackground |
                ImGuiWindowFlags_AlwaysAutoResize;

            if (ImGui::Begin("window_name", nullptr, flags))
            {
                ImGui::Text("In-world GUI element");
                ImGui::Text("Window pos (%i, %i)", window_coords.x, window_coords.x);
                ImGui::Text("World pos (%1.1f, %1.1f, %1.1f)", world_pos.x, world_pos.y, world_pos.z);
                ImGui::End();
            }
            ImGui::PopStyleColor(2);
        }
    }

    ImGui::SliderFloat("Animation speed", &characterAnimSpeed, 0.1f, 5.0f);

    ImGui::SliderFloat("Animation mix", &characterAnimBlend, 0.0f, 1.0f);

    ImGui::End(); // end info window
}

void Game::destroy()
{
    if (renderSystem)
        renderSystem->shutdown();
}

void Game::updateCamera()
{
    // Fetch mouse and compute movement since last frame
    auto mouse = ctx->input_manager->GetMouseState();
    glm::ivec2 mouse_xy{ mouse.x, mouse.y };
    glm::ivec2 mouse_xy_diff{ 0, 0 };
    if (mouse.leftButton && camera.mouse_xy_prev.x >= 0)
        mouse_xy_diff = camera.mouse_xy_prev - mouse_xy;
    camera.mouse_xy_prev = mouse_xy;

    // Update camera rotation from mouse movement
    camera.yaw += mouse_xy_diff.x * camera.sensitivity;
    camera.pitch += mouse_xy_diff.y * camera.sensitivity;
    camera.pitch = glm::clamp(camera.pitch, -glm::radians(89.0f), 0.0f);

    // Update camera position
    const glm::vec4 rotatedPos = glm_aux::R(camera.yaw, camera.pitch) * glm::vec4(0.0f, 0.0f, camera.distance, 1.0f);
    camera.pos = camera.lookAt + glm::vec3(rotatedPos);
}

void Game::updatePlayer(float deltaTime)
{
    // Fetch keys relevant for player movement
    using Key = eeng::IInputManager::Key;
    auto& input = ctx->input_manager;
    bool W = input->IsKeyPressed(Key::W);
    bool A = input->IsKeyPressed(Key::A);
    bool S = input->IsKeyPressed(Key::S);
    bool D = input->IsKeyPressed(Key::D);

    // Compute vectors in the local space of the player
    player.fwd = glm::vec3(glm_aux::R(camera.yaw, glm_aux::vec3_010) * glm::vec4(0.0f, 0.0f, -1.0f, 0.0f));
    player.right = glm::cross(player.fwd, glm_aux::vec3_010);

    // Compute the total movement as a 3D vector
    auto movement =
        player.fwd * player.velocity * deltaTime * ((W ? 1.0f : 0.0f) + (S ? -1.0f : 0.0f)) +
        player.right * player.velocity * deltaTime * ((A ? -1.0f : 0.0f) + (D ? 1.0f : 0.0f));

    // Update player position and forward view ray
    player.pos += movement;
    player.viewRay = glm_aux::Ray{ player.pos + glm::vec3(0.0f, 2.0f, 0.0f), player.fwd };

    // Update camera to track the player
    camera.lookAt += movement;
    camera.pos += movement;

}
