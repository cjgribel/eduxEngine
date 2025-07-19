// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "Game.hpp"
#include "glmcommon.hpp"
#include "imgui.h"
#include "LogMacros.h"
#include <entt/entt.hpp>

// FOR TESTS ->
#include "AssimpImporter.hpp" // --> ENGINE API
#include "MockImporter.hpp" // --> ENGINE API
#include "ResourceTypes.h"
#include "ResourceManager.hpp" // eeng::ResourceManager
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

bool Game::init()
{
    forwardRenderer = std::make_shared<eeng::ForwardRenderer>();
    forwardRenderer->init("shaders/phong_vert.glsl", "shaders/phong_frag.glsl");

    shapeRenderer = std::make_shared<ShapeRendering::ShapeRenderer>();
    shapeRenderer->init();

    // RESOURCE MANAGER API TESTS
    {
        std::cout << "RESOURCE MANAGER API TESTS..." << std::endl;

        // TODO: Ugly cast to concrete type from interface
        // https://chatgpt.com/s/t_68630dc8597881918ffd0e6db8a8c57e
        // - Use helper free function for loading etc?
        auto& resource_manager = static_cast<eeng::ResourceManager&>(*ctx->resource_manager);
        std::filesystem::path asset_root = "/Users/ag1498/GitHub/eduEngine/Module1/project1/imported_assets/";

        // 1.   IMPORT resources concurrently
        //
        using ModelRef = eeng::AssetRef<eeng::mock::Model>;
        // Requires ResourceManager to be TS
        std::cout << "Importing assets recursively..." << std::endl;
        const int numTasks = 10;
        std::vector<std::future<ModelRef>> futures;
        for (int i = 0; i < numTasks; ++i)
        {
            futures.emplace_back(
                ctx->thread_pool->queue_task([this, &asset_root]()
                    {
                        return eeng::mock::ModelImporter::import(asset_root, ctx);
                    })
            );
        }

        // 1b. Fetch asset references from futures
        //
        // - get() blocks until the task is done
        // - wait_for() checks & waits for a period of time without blocking
        EENG_LOG(ctx, "[Game::init()] Wait for imports...");
        std::vector<ModelRef> refs;
        for (auto& f : futures) refs.push_back(f.get());

        // 3. SCAN assets from disk concurrently
        //
        {
            EENG_LOG(ctx, "[Game::init()] Scanning assets...");
            resource_manager.start_async_scan("/Users/ag1498/GitHub/eduEngine/Module1/project1/imported_assets/", *ctx);
            // Wait for scanning to finish
            while (resource_manager.is_scanning())
            {
                // Wait for scanning to finish
                EENG_LOG(ctx, "[Game::init()] Waiting for asset scan to finish...");
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                // std::this_thread::yield(); // Yield to other threads
            }
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

        // 4. LOAD assets concurrently
#if 0
//
        {
            EENG_LOG(ctx, "[Game::init()] Loading assets...");
            // for (auto& ref : refs) resource_manager.load(ref, *ctx);
            // CONCURRENTLY
            std::vector<std::future<void>> load_futures;
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
        }
#endif

        // 5. UNLOAD assets (concurrently)
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
            // (Optionally, you could skip polling every few frames to reduce overhead)
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
    entity_registry = std::make_shared<entt::registry>();
    entity_registry->storage<int>();
    auto ent1 = entity_registry->create();
    struct Tfm
    {
        float x, y, z;
    };
    entity_registry->emplace<Tfm>(ent1, Tfm{});

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