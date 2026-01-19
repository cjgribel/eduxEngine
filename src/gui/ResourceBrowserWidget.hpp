// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include "EngineContext.hpp"
#include "ResourceManager.hpp"
#include "AssetTreeViews.hpp"
#include "editor/InspectorState.hpp"
#include "editor/AssignFieldCommand.hpp"
#include "editor/EditorActions.hpp"
#include "meta/MetaInspect.hpp"
#include "MetaAux.h"
#include "AssimpImporter.hpp"

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"
#include "ImGuiFileDialog.h"

#include <atomic>
#include <algorithm>
#include <cctype>
#include <deque>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

namespace eeng::gui
{
    namespace detail
    {
        inline constexpr const char* kModelImportDialog = "ResourceBrowserImportDialog";
        inline constexpr const char* kModelImportFilters =
            "Model files{.fbx,.obj,.dae,.gltf,.glb,.3ds,.ply,.blend},All files{.*}";

        inline void update_asset_selection(GuidSelection& selection, const Guid& guid)
        {
            const bool ctrl = ImGui::GetIO().KeyCtrl;
            const bool is_selected = selection.contains(guid);

            if (ctrl)
            {
                is_selected ? selection.remove(guid) : selection.add(guid);
                return;
            }

            selection.clear();
            selection.add(guid);
        }

        inline const char* load_state_label(LoadState state)
        {
            switch (state)
            {
            case LoadState::Unloaded:  return "Unloaded";
            case LoadState::Unloading: return "Unloading";
            case LoadState::Loading:   return "Loading";
            case LoadState::Loaded:    return "Loaded";
            case LoadState::Failed:    return "Failed";
            }
            return "Unknown";
        }

        inline ImVec4 load_state_color(LoadState state)
        {
            switch (state)
            {
            case LoadState::Unloaded:  return ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
            case LoadState::Unloading: return ImVec4(1.0f, 0.7f, 0.2f, 1.0f);
            case LoadState::Loading:   return ImVec4(0.6f, 0.8f, 1.0f, 1.0f);
            case LoadState::Loaded:    return ImVec4(0.4f, 1.0f, 0.4f, 1.0f);
            case LoadState::Failed:    return ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
            }
            return ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
        }

        using AssetBatch = std::deque<Guid>;

        inline AssetBatch get_branch_bottomup(const Guid& guid, const EngineContext& ctx)
        {
            AssetBatch stack;
            auto index_data = ctx.resource_manager->get_index_data();
            if (!index_data || !index_data->trees)
                return stack;

            auto& tree = index_data->trees->content_tree;
            tree.traverse_breadthfirst(guid, [&](const Guid& guid, size_t) { stack.push_front(guid); });
            return stack;
        }

        inline std::deque<Guid> compute_selected_bottomup_closure(const EngineContext& ctx)
        {
            std::deque<Guid> merged;
            std::unordered_set<Guid> seen;

            for (const Guid& guid : ctx.asset_selection->get_all())
            {
                auto branch = get_branch_bottomup(guid, ctx);

                for (const Guid& g : branch)
                {
                    if (seen.insert(g).second)
                        merged.push_back(g);
                }
            }

            return merged;
        }

        inline bool is_assimp_extension(std::filesystem::path path)
        {
            auto ext = path.extension().string();
            if (ext.empty())
                return false;
            for (char& c : ext)
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

            static constexpr const char* kExts[] = {
                ".fbx", ".obj", ".dae", ".gltf", ".glb", ".3ds", ".ply", ".blend"
            };

            for (const char* known : kExts)
            {
                if (ext == known)
                    return true;
            }
            return false;
        }

        inline assets::ImportFlags default_assimp_flags()
        {
            using assets::ImportFlags;
            return static_cast<ImportFlags>(
                static_cast<unsigned>(ImportFlags::GenerateTangents) |
                static_cast<unsigned>(ImportFlags::GenerateNormals) |
                static_cast<unsigned>(ImportFlags::GenerateUVs) |
                static_cast<unsigned>(ImportFlags::SortByPType) |
                static_cast<unsigned>(ImportFlags::FlipUVs) |
                static_cast<unsigned>(ImportFlags::OptimizeGraph));
        }
    }

    struct VerticalSplitterWidget
    {
        float bottom_height = 120.0f;
        float min_height = 50.0f;
        float thickness = 6.0f;

        float calc_top_height(float total_height)
        {
            float max_height = std::max(min_height, total_height - min_height);
            bottom_height = std::min(std::max(bottom_height, min_height), max_height);
            return std::max(0.0f, total_height - bottom_height - thickness);
        }

        void draw_handle(float total_height)
        {
            const float max_height = std::max(min_height, total_height - min_height);

            ImGui::PushID("ResourceBrowserSplitter");
            ImGui::InvisibleButton("##SplitDrag", ImVec2(-1, thickness));

            const bool hovered = ImGui::IsItemHovered();
            const bool active = ImGui::IsItemActive();
            if (hovered || active)
                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);

            if (active)
                bottom_height -= ImGui::GetIO().MouseDelta.y;

            bottom_height = std::min(std::max(bottom_height, min_height), max_height);
            ImGui::PopID();

            ImVec2 min = ImGui::GetItemRectMin();
            ImVec2 max = ImGui::GetItemRectMax();
            ImU32 bg_col = ImGui::GetColorU32(active ? ImGuiCol_SeparatorActive
                : hovered ? ImGuiCol_SeparatorHovered
                : ImGuiCol_Separator);
            ImGui::GetWindowDrawList()->AddRectFilled(min, max, bg_col);
            const float y = (min.y + max.y) * 0.5f;
            ImGui::GetWindowDrawList()
                ->AddLine({ min.x, y }, { max.x, y }, ImGui::GetColorU32(ImGuiCol_SeparatorActive));
        }
    };

    struct AssetFlatListWidget
    {
        EngineContext& ctx;
        ResourceManager& resource_manager;

        explicit AssetFlatListWidget(EngineContext& ctx)
            : ctx(ctx)
            , resource_manager(static_cast<ResourceManager&>(*ctx.resource_manager))
        {
        }

        void draw()
        {
            auto index_data = resource_manager.asset_index().get_index_data();
            if (!index_data)
            {
                ImGui::TextUnformatted("No asset index data available.");
                return;
            }

            const auto& entries = index_data->entries;
            const auto& by_guid = index_data->by_guid;
            auto& selection = *ctx.asset_selection;

            ImGui::Text("Assets found: %zu", entries.size());
            ImGui::Separator();

            for (const auto& entry : entries)
            {
                ImGui::PushID(entry.meta.guid.raw());

                const bool is_selected = selection.contains(entry.meta.guid);
                ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanFullWidth |
                    ImGuiTreeNodeFlags_OpenOnArrow |
                    ImGuiTreeNodeFlags_OpenOnDoubleClick;
                if (is_selected)
                    flags |= ImGuiTreeNodeFlags_Selected;

                auto guid_status = resource_manager.get_status(entry.meta.guid);
                const auto state_color = detail::load_state_color(guid_status.state);
                ImGui::PushStyleColor(ImGuiCol_Text, state_color);
                bool opened = ImGui::TreeNodeEx(entry.meta.name.c_str(), flags);
                ImGui::PopStyleColor();
                if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
                    detail::update_asset_selection(selection, entry.meta.guid);

                if (opened)
                {
                    ImGui::Text("Type: %s", entry.meta.type_id.c_str());
                    ImGui::Text("GUID: %s", entry.meta.guid.to_string().c_str());
                    ImGui::Text("File: %s", entry.relative_path.string().c_str());

                    const auto& children = entry.meta.contained_assets;
                    if (!children.empty())
                    {
                        if (ImGui::TreeNode("Contained Assets"))
                        {
                            for (const auto& child_guid : children)
                            {
                                auto it = by_guid.find(child_guid);
                                if (it != by_guid.end())
                                {
                                    const AssetEntry* child = it->second;
                                    ImGui::BulletText("%s [%s]",
                                        child->meta.name.c_str(),
                                        child->meta.type_id.c_str());
                                }
                                else
                                {
                                    ImGui::BulletText("Unknown GUID: %s", child_guid.to_string().c_str());
                                }
                            }
                            ImGui::TreePop();
                        }
                    }

                    ImGui::TreePop();
                }

                ImGui::PopID();
            }
        }
    };

    struct AssetDependencyTreeWidget
    {
        EngineContext& ctx;
        ResourceManager& resource_manager;

        explicit AssetDependencyTreeWidget(EngineContext& ctx)
            : ctx(ctx)
            , resource_manager(static_cast<ResourceManager&>(*ctx.resource_manager))
        {
        }

        void draw()
        {
            auto index_data = resource_manager.asset_index().get_index_data();
            if (!index_data || !index_data->trees)
            {
                ImGui::TextUnformatted("No asset index data available.");
                return;
            }

            const auto& tree = index_data->trees->content_tree;
            auto& selection = *ctx.asset_selection;

            std::function<void(size_t)> draw_node_recursive;
            draw_node_recursive = [&](size_t node_idx)
                {
                    const Guid& guid = tree.get_payload_at(node_idx);
                    auto guid_status = ctx.resource_manager->get_status(guid);

                    auto it = index_data->by_guid.find(guid);
                    if (it == index_data->by_guid.end())
                        return;
                    const AssetEntry& entry = *it->second;

                    const bool is_leaf = tree.get_nbr_children(guid) == 0;
                    const bool is_selected = selection.contains(guid);

                    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanFullWidth |
                        ImGuiTreeNodeFlags_OpenOnArrow |
                        ImGuiTreeNodeFlags_OpenOnDoubleClick;
                    if (is_leaf)
                        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet;
                    if (is_selected)
                        flags |= ImGuiTreeNodeFlags_Selected;

                    const auto state_color = detail::load_state_color(guid_status.state);
                    ImGui::PushStyleColor(ImGuiCol_Text, state_color);

                    const uint32_t leases = resource_manager.total_leases(entry.meta.guid);
                    const std::string label = entry.meta.name + " [L:" + std::to_string(leases) + "]";

                    intptr_t id_int = static_cast<intptr_t>(guid.raw());
                    void* id_ptr = reinterpret_cast<void*>(id_int);

                    bool opened = ImGui::TreeNodeEx(id_ptr, flags, "%s", label.c_str());
                    ImGui::PopStyleColor();

                    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
                        detail::update_asset_selection(selection, guid);

                    if (opened)
                    {
                        tree.traverse_children(node_idx, [&](const Guid&, size_t child_idx, size_t)
                            {
                                draw_node_recursive(child_idx);
                            });
                        ImGui::TreePop();
                    }
                };

            for (size_t root_idx : tree.get_roots())
                draw_node_recursive(root_idx);
        }
    };

    struct ResourceBrowserActionsWidget
    {
        EngineContext& ctx;
        ResourceManager& resource_manager;

        explicit ResourceBrowserActionsWidget(EngineContext& ctx)
            : ctx(ctx)
            , resource_manager(static_cast<ResourceManager&>(*ctx.resource_manager))
        {
        }

        void draw()
        {
            static std::shared_ptr<std::atomic<bool>> import_in_flight =
                std::make_shared<std::atomic<bool>>(false);
            bool busy = resource_manager.is_busy() ||
                import_in_flight->load(std::memory_order_relaxed);
            auto index_data = resource_manager.asset_index().get_index_data();
            auto& selection = *ctx.asset_selection;

            bool unimport_has_selection = !selection.empty();
            bool unimport_missing = false;
            bool unimport_has_non_root = false;
            if (unimport_has_selection)
            {
                if (!index_data || !index_data->trees)
                {
                    unimport_missing = true;
                }
                else
                {
                    const auto& tree = index_data->trees->content_tree;
                    for (const Guid& guid : selection.get_all())
                    {
                        if (!tree.contains(guid))
                        {
                            unimport_missing = true;
                            break;
                        }
                        if (!tree.is_root(guid))
                        {
                            unimport_has_non_root = true;
                            break;
                        }
                    }
                }
            }

            const bool unimport_enabled = unimport_has_selection &&
                !unimport_missing &&
                !unimport_has_non_root;
            if (busy) ImGui::BeginDisabled();

            if (ImGui::Button("Import..."))
            {
                IGFD::FileDialogConfig config;
                const auto& assets_root = resource_manager.assets_root();
                config.path = assets_root.empty() ? "." : assets_root.string();
                ImGuiFileDialog::Instance()->OpenDialog(
                    detail::kModelImportDialog,
                    "Import Model (Assimp)",
                    detail::kModelImportFilters,
                    config);
            }
            ImGui::SameLine();
            if (ImGui::Button("Import Graph (Mock)"))
            {
                editor::AssetActions::import_animation_graph_mock(ctx);
            }
            ImGui::SameLine();
            if (!unimport_enabled) ImGui::BeginDisabled();
            if (ImGui::Button("Unimport"))
            {
                if (!unimport_enabled)
                {
                    EENG_LOG_WARN(&ctx, "Unimport skipped: selection not eligible.");
                }
                else
                {
                    std::vector<Guid> roots;
                    roots.insert(roots.end(),
                        selection.get_all().begin(),
                        selection.get_all().end());

                    editor::AssetActions::unimport_assets(ctx, std::move(roots));
                }
            }
            if (!unimport_enabled)
            {
                ImGui::EndDisabled();
                if (ImGui::IsItemHovered())
                {
                    if (!unimport_has_selection)
                        ImGui::SetTooltip("Select a dependency-tree root to unimport.");
                    else if (unimport_missing)
                        ImGui::SetTooltip("Asset tree not available or selection not found.");
                    else if (unimport_has_non_root)
                        ImGui::SetTooltip("Only dependency-tree roots can be unimported.");
                }
            }
            ImGui::SameLine();

            static auto batch_id1 = Guid::generate();
            static auto batch_id2 = Guid::generate();

            if (ImGui::Button("Load (Batch 1)"))
            {
                EENG_LOG(&ctx, "GUI load batch (1) %s", batch_id1.to_string().c_str());
                auto to_reload = detail::compute_selected_bottomup_closure(ctx);
                resource_manager.load_and_bind_async(to_reload, batch_id1, ctx);
            }
            ImGui::SameLine();
            if (ImGui::Button("Unload (Batch 1)"))
            {
                auto to_reload = detail::compute_selected_bottomup_closure(ctx);
                resource_manager.unbind_and_unload_async(to_reload, batch_id1, ctx);
            }

            if (ImGui::Button("Load (Batch 2)"))
            {
                auto to_reload = detail::compute_selected_bottomup_closure(ctx);
                resource_manager.load_and_bind_async(to_reload, batch_id2, ctx);
            }
            ImGui::SameLine();
            if (ImGui::Button("Unload (Batch 2)"))
            {
                auto to_reload = detail::compute_selected_bottomup_closure(ctx);
                resource_manager.unbind_and_unload_async(to_reload, batch_id2, ctx);
            }

            if (busy) ImGui::EndDisabled();

            if (ImGuiFileDialog::Instance()->Display(detail::kModelImportDialog))
            {
                if (ImGuiFileDialog::Instance()->IsOk())
                {
                    const auto file_path = std::filesystem::path(
                        ImGuiFileDialog::Instance()->GetFilePathName());

                    if (!detail::is_assimp_extension(file_path))
                    {
                        EENG_LOG_WARN(&ctx, "Import skipped: unsupported extension %s",
                            file_path.extension().string().c_str());
                    }
                    else if (busy)
                    {
                        EENG_LOG_WARN(&ctx, "Import skipped: resource manager busy.");
                    }
                    else
                    {
                        editor::AssetActions::import_model(
                            ctx,
                            file_path,
                            detail::default_assimp_flags(),
                            file_path.stem().string(),
                            import_in_flight);
                    }
                }

                ImGuiFileDialog::Instance()->Close();
            }
        }
    };

    struct AssetInspectorWidget
    {
        EngineContext& ctx;
        ResourceManager& resource_manager;

        explicit AssetInspectorWidget(EngineContext& ctx)
            : ctx(ctx)
            , resource_manager(static_cast<ResourceManager&>(*ctx.resource_manager))
        {
        }

        void draw()
        {
            auto index_data = resource_manager.asset_index().get_index_data();
            if (!index_data)
            {
                ImGui::TextUnformatted("No asset index data available.");
                return;
            }

            auto& selection = *ctx.asset_selection;
            ImGui::TextDisabled("Asset inspector");

            if (selection.empty())
            {
                ImGui::TextDisabled("Select an asset above to see details here.");
                return;
            }

            const Guid guid = selection.first();
            auto it = index_data->by_guid.find(guid);
            if (it == index_data->by_guid.end())
            {
                ImGui::TextUnformatted("Selected asset not found in index.");
                return;
            }

            const AssetEntry& entry = *it->second;
            const AssetStatus guid_status = ctx.resource_manager->get_status(guid);

            draw_info_box(entry, guid_status);
            ImGui::Spacing();

            if (ImGui::BeginChild("AssetInspectorPanel", ImVec2(0.0f, 0.0f), true))
                draw_inspector(entry);
            ImGui::EndChild();
        }

    private:
        void draw_info_box(const AssetEntry& entry, const AssetStatus& guid_status)
        {
            const float line_h = ImGui::GetTextLineHeightWithSpacing();
            const float info_height = line_h * 6.0f;

            if (ImGui::BeginChild("AssetInfoBox", ImVec2(0.0f, info_height), true))
            {
                bool valid = resource_manager.validate_asset(entry.meta.guid, ctx);
                bool valid_rec = resource_manager.validate_asset_recursive(entry.meta.guid, ctx);

                ImGui::TextColored(valid ? ImVec4(0.3f, 1.0f, 0.3f, 1.0f) : ImVec4(1.0f, 0.3f, 0.3f, 1.0f),
                    "%s", valid ? "Valid" : "Invalid");
                ImGui::SameLine();
                ImGui::TextColored(valid_rec ? ImVec4(0.3f, 1.0f, 0.3f, 1.0f) : ImVec4(1.0f, 0.5f, 0.2f, 1.0f),
                    "%s", "Recursive");
                ImGui::SameLine();
                ImGui::TextColored(detail::load_state_color(guid_status.state),
                    "%s", detail::load_state_label(guid_status.state));

                if (!guid_status.error_message.empty())
                {
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
                        "[%s]", guid_status.error_message.c_str());
                }

                ImGui::Text("Type: %s", entry.meta.type_id.c_str());
                ImGui::SameLine();
                ImGui::Text("Leases: %u", resource_manager.total_leases(entry.meta.guid));
                ImGui::Text("GUID: %s", entry.meta.guid.to_string().c_str());
                ImGui::Text("Path: %s", entry.relative_path.string().c_str());

                const bool can_save = guid_status.state == LoadState::Loaded;
                if (!can_save) ImGui::BeginDisabled();
                if (ImGui::Button("Save Asset"))
                {
                    try
                    {
                        resource_manager.save_asset(entry.meta.guid, ctx);
                    }
                    catch (const std::exception& ex)
                    {
                        EENG_LOG_WARN(&ctx, "Save asset failed: %s", ex.what());
                    }
                }
                if (!can_save)
                {
                    ImGui::EndDisabled();
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Asset must be loaded to save.");
                }
            }
            ImGui::EndChild();
        }

        void draw_inspector(const AssetEntry& entry)
        {
            editor::InspectorState insp;
            const ImGuiTableFlags flags =
                ImGuiTableFlags_BordersV |
                ImGuiTableFlags_BordersOuterH |
                ImGuiTableFlags_Resizable |
                ImGuiTableFlags_RowBg |
                ImGuiTableFlags_NoBordersInBody;

            auto metah_opt = resource_manager.storage().handle_for_guid(entry.meta.guid);
            if (metah_opt.has_value())
            {
                if (auto h_opt = metah_opt->cast<mock::Mesh>(); h_opt.has_value())
                {
                    resource_manager.storage().read(*h_opt, [&](const mock::Mesh& mesh) {
                        ImGui::TextDisabled("%f, %f, %f", mesh.vertices[0], mesh.vertices[1], mesh.vertices[2]);
                        ImGui::Separator();
                    });
                }
            }

            if (ImGui::BeginTable("AssetInspectorTable", 2, flags))
            {
                if (insp.begin_node("Metadata"))
                {
                    editor::AssignFieldCommandBuilder cmd_builder;
                    auto any = entt::forward_as_meta(entry.meta);
                    meta::inspect_any(any, insp, cmd_builder, ctx);
                    insp.end_node();
                }

                if (metah_opt.has_value())
                {
                    editor::AssignFieldCommandBuilder cmd_builder;
                    resource_manager.storage().modify(*metah_opt, [&](entt::meta_any& any) {
                        auto type_name = meta::get_meta_type_display_name(any.type());

                        if (insp.begin_node(type_name.c_str()))
                        {
                            cmd_builder.target_asset(ctx, ctx.resource_manager, entry.meta.guid, entry.meta.type_id);
                            meta::inspect_any(any, insp, cmd_builder, ctx);
                            insp.end_node();
                        }
                    });
                }

                ImGui::EndTable();
            }
        }
    };

    struct ResourceBrowserWidget
    {
        EngineContext& ctx;

        explicit ResourceBrowserWidget(EngineContext& ctx)
            : ctx(ctx)
        {
        }

        void draw()
        {
            static VerticalSplitterWidget splitter{};
            ImVec2 avail = ImGui::GetContentRegionAvail();
            float top_pane_height = splitter.calc_top_height(avail.y);

            if (ImGui::BeginChild("##ResourceBrowserTopPane", ImVec2(0, top_pane_height), true))
            {
                if (ImGui::BeginTabBar("ResourceViews"))
                {
                    if (ImGui::BeginTabItem("Dependency Tree"))
                    {
                        auto& selection = *ctx.asset_selection;
                        ImGui::TextDisabled("Selected:");
                        if (selection.empty())
                        {
                            ImGui::SameLine();
                            ImGui::TextDisabled("(none)");
                        }
                        else
                        {
                            for (auto& guid : selection.get_all())
                            {
                                ImGui::SameLine();
                                ImGui::TextDisabled("%s%s",
                                    guid.to_string().c_str(),
                                    (guid == selection.last()) ? "" : ", ");
                            }
                        }

                        AssetDependencyTreeWidget dep_view{ ctx };
                        dep_view.draw();
                        ImGui::EndTabItem();
                    }

                    if (ImGui::BeginTabItem("Flat List"))
                    {
                        AssetFlatListWidget flat_list{ ctx };
                        flat_list.draw();
                        ImGui::EndTabItem();
                    }

                    /*
                    ImGui::BeginDisabled();

                    if (ImGui::BeginTabItem("By Type"))
                    {
                        ImGui::TextUnformatted("(Unimplemented) Grouped by resource type");
                        ImGui::EndTabItem();
                    }

                    if (ImGui::BeginTabItem("File View"))
                    {
                        ImGui::TextUnformatted("(Unimplemented) Hierarchical file/folder view");
                        ImGui::EndTabItem();
                    }

                    if (ImGui::BeginTabItem("Reference View"))
                    {
                        ImGui::TextUnformatted("(Unimplemented) Dependencies between assets");
                        ImGui::EndTabItem();
                    }

                    ImGui::EndDisabled();
                    */

                    ImGui::EndTabBar();
                }
            }
            ImGui::EndChild();

            splitter.draw_handle(avail.y);

            if (ImGui::BeginChild("##ResourceBrowserBottomPane", ImVec2(0, splitter.bottom_height), true))
            {
                ResourceBrowserActionsWidget actions{ ctx };
                actions.draw();

                ImGui::Separator();

                AssetInspectorWidget inspector{ ctx };
                inspector.draw();
            }
            ImGui::EndChild();
        }
    };
} // namespace eeng::gui
