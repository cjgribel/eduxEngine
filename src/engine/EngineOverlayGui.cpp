// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "EngineOverlayGui.hpp"

#include "engineapi/EngineContext.hpp"
#include "engineapi/IGuiManager.hpp"
#include "engineapi/SelectionManager.hpp"
#include "ecs/EntityManager.hpp"
#include "BatchRegistry.hpp"
#include "editor/CommandQueue.hpp"
#include "editor/ProjectConfig.hpp"
#include "LogManager.hpp"
#include "MainThreadQueue.hpp"
#include "ThreadPool.hpp"
#include "EventQueue.h"
#include "VecTree.h"
#include "imgui.h"
#include <entt/entt.hpp>

namespace eeng
{
    namespace
    {
        void draw_log(EngineContext& ctx)
        {
            if (!ctx.log_manager)
                return;
            static_cast<LogManager&>(*ctx.log_manager).draw_gui_widget("Log");
        }

        void draw_engine_info(EngineContext& ctx)
        {
            ImGui::Begin("Engine Info");

            if (ImGui::CollapsingHeader("General", ImGuiTreeNodeFlags_DefaultOpen))
            {
                if (ctx.input_manager)
                {
                    auto mouse = ctx.input_manager->GetMouseState();
                    ImGui::Text("Mouse pos (%i, %i) %s%s",
                        mouse.x,
                        mouse.y,
                        mouse.leftButton ? "L" : "",
                        mouse.rightButton ? "R" : "");
                }
                else
                {
                    ImGui::TextDisabled("Input manager unavailable.");
                }

                // Framerate
                ImGui::Text("%.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

                if (ctx.engine_config)
                {
                    // Combo (drop-down) for fps settings
                    static const char* items[] = { "10", "30", "60", "120", "Uncapped" };
                    static int currentItem = 2;
                    if (ImGui::BeginCombo("FPS cap##targetfps", items[currentItem]))
                    {
                        for (int i = 0; i < IM_ARRAYSIZE(items); i++)
                        {
                            const bool isSelected = (currentItem == i);
                            if (ImGui::Selectable(items[i], isSelected))
                                currentItem = i;

                            if (isSelected)
                                ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }
                    float min_frametime_ms = 0.0f;
                    if (currentItem == 0)
                        min_frametime_ms = 1000.0f / 10;
                    else if (currentItem == 1)
                        min_frametime_ms = 1000.0f / 30;
                    else if (currentItem == 2)
                        min_frametime_ms = 1000.0f / 60;
                    else if (currentItem == 3)
                        min_frametime_ms = 1000.0f / 120;
                    else {}
                    ctx.engine_config->set_value(EngineValue::MinFrameTime, min_frametime_ms);

                    // V-sync
                    bool vsync = ctx.engine_config->get_flag(EngineFlag::VSync);
                    if (ImGui::Checkbox("V-Sync", &vsync))
                    {
                        ctx.engine_config->set_flag(EngineFlag::VSync, vsync);
                    }

                    // Wireframe rendering
                    ImGui::SameLine();
                    bool wf = ctx.engine_config->get_flag(EngineFlag::WireframeRendering);
                    if (ImGui::Checkbox("Wireframe", &wf))
                    {
                        ctx.engine_config->set_flag(EngineFlag::WireframeRendering, wf);
                    }

                    // Debug logging
                    ImGui::SameLine();
                    bool debug_logging = ctx.engine_config->get_flag(EngineFlag::DebugLogging);
                    if (ImGui::Checkbox("Debug logging", &debug_logging))
                    {
                        ctx.engine_config->set_flag(EngineFlag::DebugLogging, debug_logging);
                    }
                }
                else
                {
                    ImGui::TextDisabled("Engine config unavailable.");
                }

                if (ctx.services)
                {
                    const bool play_mode = ctx.services->play_mode_active.load(std::memory_order_relaxed);
                    ImGui::Text("Play mode: %s", play_mode ? "Active" : "Stopped");
                }
            }

            if (ImGui::CollapsingHeader("World", ImGuiTreeNodeFlags_DefaultOpen))
            {
                if (!ctx.entity_manager)
                {
                    ImGui::TextDisabled("Entity manager unavailable.");
                }
                else
                {
                    auto& registry = ctx.entity_manager->registry();
                    const size_t entity_count = registry.storage<entt::entity>().size();
                    ImGui::Text("Entities: %zu", entity_count);

                    if (ctx.entity_selection)
                        ImGui::Text("Selected entities: %zu", ctx.entity_selection->size());

                    if (auto* em = dynamic_cast<EntityManager*>(ctx.entity_manager.get()))
                    {
                        auto& graph = em->scene_graph();
                        const size_t graph_nodes = graph.size();
                        ImGui::Text("Scene graph nodes: %zu", graph_nodes);

                        size_t root_count = 0;
                        const auto& tree = graph.get_tree();
                        for (size_t i = 0; i < tree.size(); ++i)
                        {
                            const auto [payload, nbr_children, branch_stride, parent_ofs] = tree.get_node_info_at(i);
                            (void)payload;
                            (void)nbr_children;
                            (void)branch_stride;
                            if (parent_ofs == 0)
                                ++root_count;
                        }
                        ImGui::Text("Scene roots: %zu", root_count);
                    }
                    else
                    {
                        ImGui::TextDisabled("Scene graph stats unavailable.");
                    }

                    size_t storage_count = 0;
                    size_t component_instances = 0;
                    for (auto&& [id, storage] : registry.storage())
                    {
                        (void)id;
                        ++storage_count;
                        component_instances += storage.size();
                    }
                    ImGui::Text("Component storages: %zu", storage_count);
                    ImGui::Text("Component instances: %zu", component_instances);
                }
            }

            if (ImGui::CollapsingHeader("Resources", ImGuiTreeNodeFlags_DefaultOpen))
            {
                if (!ctx.resource_manager)
                {
                    ImGui::TextDisabled("Resource manager unavailable.");
                }
                else
                {
                    auto& rm = *ctx.resource_manager;
                    ImGui::Text("Busy: %s", rm.is_busy() ? "yes" : "no");
                    ImGui::Text("Queued tasks: %d", rm.queued_tasks());

                    if (ctx.asset_selection)
                        ImGui::Text("Selected assets: %zu", ctx.asset_selection->size());

                    if (auto index = rm.get_index_data())
                    {
                        const size_t total = index->entries.size();
                        ImGui::Text("Assets indexed: %zu", total);
                        ImGui::Text("Asset types: %zu", index->by_type.size());

                        size_t loaded = 0;
                        size_t loading = 0;
                        size_t unloading = 0;
                        size_t unloaded = 0;
                        size_t failed = 0;
                        for (const auto& entry : index->entries)
                        {
                            const auto status = rm.get_status(entry.meta.guid);
                            switch (status.state)
                            {
                            case LoadState::Loaded:    ++loaded; break;
                            case LoadState::Loading:   ++loading; break;
                            case LoadState::Unloading: ++unloading; break;
                            case LoadState::Unloaded:  ++unloaded; break;
                            case LoadState::Failed:    ++failed; break;
                            }
                        }

                        ImGui::Text("Loaded: %zu", loaded);
                        ImGui::SameLine();
                        ImGui::Text("Loading: %zu", loading);
                        ImGui::SameLine();
                        ImGui::Text("Unloading: %zu", unloading);
                        ImGui::Text("Unloaded: %zu", unloaded);
                        ImGui::SameLine();
                        ImGui::Text("Failed: %zu", failed);
                    }
                    else
                    {
                        ImGui::TextDisabled("No asset index data available.");
                    }
                }
            }

            if (ImGui::CollapsingHeader("Batches"))
            {
                if (!ctx.batch_registry)
                {
                    ImGui::TextDisabled("Batch registry unavailable.");
                }
                else if (auto* br = dynamic_cast<BatchRegistry*>(ctx.batch_registry.get()))
                {
                    const auto batches = br->list();
                    size_t loaded = 0;
                    size_t loading = 0;
                    size_t queued = 0;
                    size_t unloading = 0;
                    size_t error = 0;
                    size_t live_entities = 0;

                    for (const auto* info : batches)
                    {
                        if (!info)
                            continue;
                        switch (info->state)
                        {
                        case BatchInfo::State::Loaded:    ++loaded; break;
                        case BatchInfo::State::Loading:   ++loading; break;
                        case BatchInfo::State::Queued:    ++queued; break;
                        case BatchInfo::State::Unloading: ++unloading; break;
                        case BatchInfo::State::Error:     ++error; break;
                        case BatchInfo::State::Unloaded:  break;
                        }
                        live_entities += info->live.size();
                    }

                    ImGui::Text("Batches: %zu", batches.size());
                    ImGui::Text("Loaded: %zu", loaded);
                    ImGui::SameLine();
                    ImGui::Text("Loading: %zu", loading);
                    ImGui::SameLine();
                    ImGui::Text("Queued: %zu", queued);
                    ImGui::Text("Unloading: %zu", unloading);
                    ImGui::SameLine();
                    ImGui::Text("Error: %zu", error);
                    ImGui::Text("Live entities: %zu", live_entities);

                    if (ctx.batch_selection)
                        ImGui::Text("Selected batches: %zu", ctx.batch_selection->size());
                }
                else
                {
                    ImGui::TextDisabled("Batch registry stats unavailable.");
                }
            }

            if (ImGui::CollapsingHeader("Queues"))
            {
                if (ctx.thread_pool)
                {
                    auto* pool = ctx.thread_pool.get();
                    ImGui::Text("Thread pool: %zu threads (%zu working, %zu idle)",
                        pool->nbr_threads(),
                        pool->nbr_working_threads(),
                        pool->nbr_idle_threads());
                    ImGui::Text("Queued tasks: %zu", pool->task_queue_size());
                }
                else
                {
                    ImGui::TextDisabled("Thread pool unavailable.");
                }

                if (ctx.main_thread_queue)
                {
                    const bool empty = ctx.main_thread_queue->empty();
                    ImGui::Text("Main thread queue: %s", empty ? "empty" : "pending");
                }
                else
                {
                    ImGui::TextDisabled("Main thread queue unavailable.");
                }

                if (ctx.event_queue)
                {
                    ImGui::Text("Event queue: %s",
                        ctx.event_queue->has_pending_events() ? "pending" : "empty");
                }
                else
                {
                    ImGui::TextDisabled("Event queue unavailable.");
                }

                if (ctx.command_queue)
                {
                    auto& cq = *ctx.command_queue;
                    ImGui::Text("Command queue: %zu", cq.size());
                    ImGui::Text("In flight: %s", cq.has_in_flight() ? "yes" : "no");
                    ImGui::Text("Ready commands: %s", cq.has_ready_commands() ? "yes" : "no");
                    ImGui::Text("Enqueued commands: %d", cq.enqueued_count());
                }
                else
                {
                    ImGui::TextDisabled("Command queue unavailable.");
                }
            }

            if (ImGui::CollapsingHeader("Project"))
            {
                if (ctx.project_config)
                {
                    ImGui::Text("Project root: %s", ctx.project_config->project_root.string().c_str());
                    ImGui::Text("Assets root: %s", ctx.project_config->assets_root.string().c_str());
                    ImGui::Text("Imported assets: %s", ctx.project_config->imported_assets_root.string().c_str());
                    ImGui::Text("Batches root: %s", ctx.project_config->batches_root.string().c_str());
                }
                else
                {
                    ImGui::TextDisabled("No project config loaded.");
                }
            }

            if (ImGui::CollapsingHeader("View"))
            {
                if (ctx.overlay_view_state && ctx.overlay_view_state->valid)
                {
                    const auto& view = *ctx.overlay_view_state;
                    ImGui::Text("Window size: %d x %d", view.window_size.x, view.window_size.y);
                }
                else
                {
                    ImGui::TextDisabled("Overlay view state unavailable.");
                }
            }

            if (ImGui::CollapsingHeader("Physics", ImGuiTreeNodeFlags_DefaultOpen))
            {
                // Read-only snapshot populated by RuntimePipeline each frame.
                if (!ctx.physics_monitor_stats || !ctx.physics_monitor_stats->valid)
                {
                    ImGui::TextDisabled("No physics stats available.");
                }
                else
                {
                    const auto& stats = *ctx.physics_monitor_stats;
                    ImGui::Text("Bodies: %zu", stats.body_count);
                    ImGui::Text("Collision objects: %d", stats.collision_objects);
                    ImGui::Text("Manifolds: %d", stats.manifolds);
                    ImGui::Text("Contact points: %d", stats.contact_points);
                    ImGui::Separator();
                    ImGui::Text("Dirty entities: %zu", stats.dirty_entities);
                    ImGui::Text("Event entities: %zu", stats.event_entities);
                    ImGui::Text("Tracked contacts: %zu", stats.tracked_contacts);
                }
            }

            if (ImGui::CollapsingHeader("Controllers", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::Text("Controllers connected: %i", ctx.input_manager->GetConnectedControllerCount());

                for (auto& [id, state] : ctx.input_manager->GetControllers())
                {
                    ImGui::PushID(id);
                    ImGui::BeginChild("Controller", ImVec2(0, ImGui::GetTextLineHeightWithSpacing() * 6), true);

                    ImGui::Text("Controller %i: '%s'", id, state.name.c_str());
                    ImGui::Text("Left stick:  X: %.2f  Y: %.2f", state.axisLeftX, state.axisLeftY);
                    ImGui::Text("Right stick: X: %.2f  Y: %.2f", state.axisRightX, state.axisRightY);
                    ImGui::Text("Triggers:    L: %.2f  R: %.2f", state.triggerLeft, state.triggerRight);
                    std::string buttons;
                    for (const auto& [buttonId, isPressed] : state.buttonStates)
                        buttons += "#" + std::to_string(buttonId) + "(" + (isPressed ? "1) " : "0) ");
                    ImGui::Text("Buttons: %s", buttons.c_str());

                    ImGui::EndChild();
                    ImGui::PopID();
                }
            }

            ImGui::End();
        }
    }

    void EngineOverlayGui::draw(EngineContext& ctx) const
    {
        if (!ctx.gui_manager)
            return;

        if (ctx.gui_manager->is_flag_enabled(eeng::GuiFlags::ShowLogWindow))
            draw_log(ctx);

        if (ctx.gui_manager->is_flag_enabled(eeng::GuiFlags::ShowEngineInfo))
            draw_engine_info(ctx);
    }
}
