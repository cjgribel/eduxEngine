// Created by Carl Johan Gribel 2026.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include "EngineContext.hpp"
#include "editor/AssetRefInspect.hpp"
#include "editor/GLMInspect.hpp"
#include "editor/InspectorState.hpp"
#include "editor/TypeInspect.hpp"
#include "engineapi/EngineContextHelpers.hpp"
#include "meta/MetaInspect.hpp"
#include "meta/MetaInfo.h"
#include "ecs/ModelComponent.hpp"
#include "ecs/PhysicsComponents.hpp"
#include "physics/PhysicsGeometry.hpp"
#include "assets/types/ModelAssets.hpp"
#include "assets/types/TerrainAssets.hpp"

#include "imgui.h"
#include <algorithm>
#include <string>

// Custom inspectors for physics components and collider helpers.
namespace eeng::editor
{
    // Inspector-specific helpers for physics component UI.
    namespace detail
    {
        inline void show_meta_tooltip(const entt::meta_type& meta_type,
            const char* field_name,
            ImGuiHoveredFlags flags = ImGuiHoveredFlags_DelayNormal)
        {
            if (!ImGui::IsItemHovered(flags))
                return;
            if (!meta_type)
                return;
            entt::meta_data meta_data = meta_type.data(entt::hashed_string{ field_name }.value());
            if (!meta_data)
                return;
            eeng::DataMetaInfo* info = meta_data.custom();
            if (!info || info->tooltip.empty())
                return;
            ImGui::SetTooltip("%s", info->tooltip.c_str());
        }

        template<typename T>
        inline void show_meta_tooltip(
            const char* field_name,
            ImGuiHoveredFlags flags = ImGuiHoveredFlags_DelayNormal)
        {
            show_meta_tooltip(entt::resolve<T>(), field_name, flags);
        }

        inline const eeng::DataMetaInfo* get_data_meta_info(
            const entt::meta_type& meta_type,
            const char* field_name)
        {
            if (!meta_type)
                return nullptr;
            entt::meta_data meta_data = meta_type.data(entt::hashed_string{ field_name }.value());
            if (!meta_data)
                return nullptr;
            return meta_data.custom();
        }

        struct ScopedFieldMetaInfo
        {
            InspectorState& inspector;
            const eeng::DataMetaInfo* prev = nullptr;

            ScopedFieldMetaInfo(
                InspectorState& inspector_in,
                const entt::meta_type& meta_type,
                const char* field_name)
                : inspector(inspector_in)
                , prev(inspector.current_data_meta_info)
            {
                // Temporarily override the active DataMetaInfo so nested widgets
                // (for example quaternion editors) can pick up per-field hints
                // such as "edit as Euler degrees" without hard-coding field names.
                inspector.current_data_meta_info = get_data_meta_info(meta_type, field_name);
            }

            ~ScopedFieldMetaInfo()
            {
                // Restore previous context to avoid leaking hints across fields.
                inspector.current_data_meta_info = prev;
            }
        };

        // Human-readable labels for collider types.
        inline const char* collider_type_label(ecs::ColliderType type)
        {
            switch (type)
            {
            case ecs::ColliderType::Box: return "Box";
            case ecs::ColliderType::Sphere: return "Sphere";
            case ecs::ColliderType::Capsule: return "Capsule";
            case ecs::ColliderType::ConvexHull: return "Convex Hull";
            case ecs::ColliderType::TriangleMesh: return "Triangle Mesh";
            case ecs::ColliderType::Heightfield: return "Heightfield";
            case ecs::ColliderType::AABB: return "AABB";
            default: return "Unknown";
            }
        }

        // Allocates a stable id above the current max.
        inline ecs::ColliderId next_collider_id(const ecs::ColliderComponent& comp)
        {
            ecs::ColliderId next = 1;
            for (const auto& desc : comp.colliders)
            {
                if (desc.id >= next)
                    next = desc.id + 1;
            }
            if (next == 0)
                ++next;
            return next;
        }

        // Computes an AABB for one submesh using index ranges.
        inline AABB submesh_aabb(const assets::ModelDataAsset& model, const assets::SubMesh& submesh)
        {
            AABB aabb;
            aabb.reset();

            const auto& positions = model.positions;
            const auto& indices = model.indices;
            const std::size_t base_vertex = static_cast<std::size_t>(submesh.base_vertex);
            const std::size_t base_index = static_cast<std::size_t>(submesh.base_index);
            const std::size_t nbr_indices = static_cast<std::size_t>(submesh.nbr_indices);
            const std::size_t nbr_vertices = static_cast<std::size_t>(submesh.nbr_vertices);

            if (!indices.empty() && nbr_indices > 0)
            {
                const std::size_t end_index = std::min(indices.size(), base_index + nbr_indices);
                for (std::size_t i = base_index; i < end_index; ++i)
                {
                    const std::size_t local_index = static_cast<std::size_t>(indices[i]);
                    const std::size_t pos_index = base_vertex + local_index;
                    if (pos_index < positions.size())
                        aabb.grow(positions[pos_index]);
                }
                return aabb;
            }

            if (nbr_vertices > 0 && base_vertex < positions.size())
            {
                const std::size_t end_vertex = std::min(positions.size(), base_vertex + nbr_vertices);
                for (std::size_t i = base_vertex; i < end_vertex; ++i)
                    aabb.grow(positions[i]);
            }
            return aabb;
        }

        // Appends one AABB collider per submesh from the entity's ModelComponent.
        inline bool append_aabb_colliders_from_model(
            ecs::ColliderComponent& comp,
            const ecs::ModelComponent& model_comp,
            EngineContext& ctx,
            bool clear_existing)
        {
            if (clear_existing)
                comp.colliders.clear();

            auto rm = eeng::try_get_resource_manager(ctx, "ColliderComponentInspect");
            if (!rm)
                return false;

            bool appended = false;
            eeng::try_read_asset_ref(
                *rm,
                model_comp.model_ref,
                ctx,
                "ColliderComponentInspect",
                "Missing GpuModelAsset for ModelComponent:",
                [&](const assets::GpuModelAsset& gpu_model)
                {
                    eeng::try_read_asset_ref(
                        *rm,
                        gpu_model.model_ref,
                        ctx,
                        "ColliderComponentInspect",
                        "Missing ModelDataAsset for ModelComponent:",
                        [&](const assets::ModelDataAsset& model_data)
                        {
                            for (std::size_t i = 0; i < model_data.submeshes.size(); ++i)
                            {
                                const auto& submesh = model_data.submeshes[i];
                                const AABB aabb = submesh_aabb(model_data, submesh);
                                if (!physics::is_valid_aabb(aabb))
                                    continue;

                                ecs::ColliderDesc desc{};
                                desc.id = next_collider_id(comp);
                                desc.type = ecs::ColliderType::AABB;
                                desc.local_position = physics::aabb_center(aabb);
                                desc.half_extents = physics::aabb_half_extents(aabb);
                                desc.submesh_index = static_cast<int>(i);
                                comp.colliders.push_back(desc);
                                appended = true;
                            }
                        });
                });
            return appended;
        }
    } // namespace detail

    // Inspector for a single collider descriptor.
    inline bool inspect_ColliderDesc(
        entt::meta_any& any,
        InspectorState& inspector,
        EngineContext& ctx)
    {
        auto* desc = any.try_cast<ecs::ColliderDesc>();
        if (!desc)
            return false;

        bool modified = false;

        inspector.begin_leaf("id");
        inspector.begin_disabled();
        modified |= inspect_type(desc->id, inspector);
        detail::show_meta_tooltip<ecs::ColliderDesc>(
            "id",
            ImGuiHoveredFlags_DelayNormal | ImGuiHoveredFlags_AllowWhenDisabled);
        inspector.end_disabled();
        inspector.end_leaf();

        inspector.begin_leaf("type");
        const char* current_label = detail::collider_type_label(desc->type);
        if (ImGui::BeginCombo("##collider_type", current_label))
        {
            const ecs::ColliderType options[] = {
                ecs::ColliderType::Box,
                ecs::ColliderType::Sphere,
                ecs::ColliderType::Capsule,
                ecs::ColliderType::ConvexHull,
                ecs::ColliderType::TriangleMesh,
                ecs::ColliderType::Heightfield,
                ecs::ColliderType::AABB
            };
            for (ecs::ColliderType option : options)
            {
                const bool is_selected = (desc->type == option);
                if (ImGui::Selectable(detail::collider_type_label(option), is_selected))
                {
                    desc->type = option;
                    modified = true;
                }
                if (is_selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        detail::show_meta_tooltip<ecs::ColliderDesc>("type");
        inspector.end_leaf();

        inspector.begin_leaf("local_position");
        auto pos_any = entt::forward_as_meta(desc->local_position);
        modified |= inspect_glmvec3(pos_any, inspector, ctx);
        detail::show_meta_tooltip<ecs::ColliderDesc>("local_position");
        inspector.end_leaf();

        inspector.begin_leaf("local_rotation");
        {
            detail::ScopedFieldMetaInfo field_scope(
                inspector,
                entt::resolve<ecs::ColliderDesc>(),
                "local_rotation");
            auto rot_any = entt::forward_as_meta(desc->local_rotation);
            modified |= inspect_glmquat(rot_any, inspector, ctx);
        }
        detail::show_meta_tooltip<ecs::ColliderDesc>("local_rotation");
        inspector.end_leaf();

        const bool is_box = desc->type == ecs::ColliderType::Box || desc->type == ecs::ColliderType::AABB;
        const bool is_sphere = desc->type == ecs::ColliderType::Sphere;
        const bool is_capsule = desc->type == ecs::ColliderType::Capsule;
        const bool is_mesh = desc->type == ecs::ColliderType::ConvexHull || desc->type == ecs::ColliderType::TriangleMesh;
        const bool is_heightfield = desc->type == ecs::ColliderType::Heightfield;

        if (is_box)
        {
            inspector.begin_leaf("half_extents");
            auto ext_any = entt::forward_as_meta(desc->half_extents);
            modified |= inspect_glmvec3(ext_any, inspector, ctx);
            detail::show_meta_tooltip<ecs::ColliderDesc>("half_extents");
            inspector.end_leaf();
        }

        if (is_sphere || is_capsule)
        {
            inspector.begin_leaf("radius");
            modified |= inspect_type(desc->radius, inspector);
            detail::show_meta_tooltip<ecs::ColliderDesc>("radius");
            inspector.end_leaf();
        }

        if (is_capsule)
        {
            inspector.begin_leaf("height");
            modified |= inspect_type(desc->height, inspector);
            detail::show_meta_tooltip<ecs::ColliderDesc>("height");
            inspector.end_leaf();
        }

        if (is_mesh)
        {
            inspector.begin_leaf("mesh_ref");
            auto ref_any = entt::forward_as_meta(desc->mesh_ref);
            modified |= inspect_AssetRef<assets::ModelDataAsset>(ref_any, inspector, ctx);
            detail::show_meta_tooltip<ecs::ColliderDesc>("mesh_ref");
            inspector.end_leaf();

            inspector.begin_leaf("submesh_index");
            modified |= inspect_type(desc->submesh_index, inspector);
            detail::show_meta_tooltip<ecs::ColliderDesc>("submesh_index");
            inspector.end_leaf();
        }

        if (is_heightfield)
        {
            inspector.begin_leaf("terrain_chunk_ref");
            auto ref_any = entt::forward_as_meta(desc->terrain_chunk_ref);
            modified |= inspect_AssetRef<assets::TerrainChunkAsset>(ref_any, inspector, ctx);
            detail::show_meta_tooltip<ecs::ColliderDesc>("terrain_chunk_ref");
            inspector.end_leaf();
        }

        inspector.begin_leaf("is_trigger");
        modified |= inspect_type(desc->is_trigger, inspector);
        detail::show_meta_tooltip<ecs::ColliderDesc>("is_trigger");
        inspector.end_leaf();

        return modified;
    }

    inline bool inspect_RigidBodyComponent(
        entt::meta_any& any,
        InspectorState& inspector,
        EngineContext& ctx)
    {
        auto* rb = any.try_cast<ecs::RigidBodyComponent>();
        if (!rb)
            return false;

        bool modified = false;

        inspector.begin_leaf("motion");
        {
            auto motion_any = entt::forward_as_meta(rb->motion);
            modified |= eeng::meta::inspect_enum_any(motion_any, inspector);
        }
        detail::show_meta_tooltip<ecs::RigidBodyComponent>("motion");
        inspector.end_leaf();

        inspector.begin_leaf("auto_mass");
        modified |= inspect_type(rb->auto_mass, inspector);
        detail::show_meta_tooltip<ecs::RigidBodyComponent>("auto_mass");
        inspector.end_leaf();

        inspector.begin_leaf("mass");
        if (rb->auto_mass)
            inspector.begin_disabled();
        modified |= inspect_type(rb->mass, inspector);
        if (rb->auto_mass)
        {
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                ImGui::SetTooltip("Computed from density and collider volume.");
            inspector.end_disabled();
        }
        else
        {
            detail::show_meta_tooltip<ecs::RigidBodyComponent>("mass");
        }
        inspector.end_leaf();

        inspector.begin_leaf("density");
        modified |= inspect_type(rb->density, inspector);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Used when Auto Mass is enabled.");
        inspector.end_leaf();

        inspector.begin_leaf("auto_inertia");
        modified |= inspect_type(rb->auto_inertia, inspector);
        detail::show_meta_tooltip<ecs::RigidBodyComponent>("auto_inertia");
        inspector.end_leaf();

        inspector.begin_leaf("inertia");
        if (rb->auto_inertia)
            inspector.begin_disabled();
        {
            auto inertia_any = entt::forward_as_meta(rb->inertia);
            modified |= inspect_glmvec3(inertia_any, inspector, ctx);
        }
        if (rb->auto_inertia)
        {
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                ImGui::SetTooltip("Computed diagonal inertia in body axes.");
            inspector.end_disabled();
        }
        else
        {
            detail::show_meta_tooltip<ecs::RigidBodyComponent>("inertia");
        }
        inspector.end_leaf();

        inspector.begin_leaf("com_local_position");
        inspector.begin_disabled();
        {
            auto com_pos_any = entt::forward_as_meta(rb->com_local_position);
            (void)inspect_glmvec3(com_pos_any, inspector, ctx);
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetTooltip("Computed center of mass offset (pivot -> COM).");
        inspector.end_disabled();
        inspector.end_leaf();

        inspector.begin_leaf("com_local_rotation");
        inspector.begin_disabled();
        {
            detail::ScopedFieldMetaInfo field_scope(
                inspector,
                entt::resolve<ecs::RigidBodyComponent>(),
                "com_local_rotation");
            auto rot_any = entt::forward_as_meta(rb->com_local_rotation);
            (void)inspect_glmquat(rot_any, inspector, ctx);
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetTooltip("Computed principal-axes rotation (body -> pivot).");
        inspector.end_disabled();
        inspector.end_leaf();

        inspector.begin_leaf("linear_damping");
        modified |= inspect_type(rb->linear_damping, inspector);
        detail::show_meta_tooltip<ecs::RigidBodyComponent>("linear_damping");
        inspector.end_leaf();

        inspector.begin_leaf("angular_damping");
        modified |= inspect_type(rb->angular_damping, inspector);
        detail::show_meta_tooltip<ecs::RigidBodyComponent>("angular_damping");
        inspector.end_leaf();

        inspector.begin_leaf("gravity_scale");
        modified |= inspect_type(rb->gravity_scale, inspector);
        detail::show_meta_tooltip<ecs::RigidBodyComponent>("gravity_scale");
        inspector.end_leaf();

        inspector.begin_leaf("allow_sleep");
        modified |= inspect_type(rb->allow_sleep, inspector);
        detail::show_meta_tooltip<ecs::RigidBodyComponent>("allow_sleep");
        inspector.end_leaf();

        inspector.begin_leaf("enable_ccd");
        modified |= inspect_type(rb->enable_ccd, inspector);
        detail::show_meta_tooltip<ecs::RigidBodyComponent>("enable_ccd");
        inspector.end_leaf();

        inspector.begin_leaf("ccd_swept_sphere_radius");
        modified |= inspect_type(rb->ccd_swept_sphere_radius, inspector);
        detail::show_meta_tooltip<ecs::RigidBodyComponent>("ccd_swept_sphere_radius");
        inspector.end_leaf();

        inspector.begin_leaf("ccd_motion_threshold");
        modified |= inspect_type(rb->ccd_motion_threshold, inspector);
        detail::show_meta_tooltip<ecs::RigidBodyComponent>("ccd_motion_threshold");
        inspector.end_leaf();

        return modified;
    }


    // Inspector for the collider list (adds/removes + bulk generation).
    inline bool inspect_ColliderComponent(
        entt::meta_any& any,
        InspectorState& inspector,
        EngineContext& ctx)
    {
        auto* comp = any.try_cast<ecs::ColliderComponent>();
        if (!comp)
            return false;

        bool modified = false;

        // Manual collider entry.
        inspector.row();
        ImGui::TextDisabled("Collider Tools");
        inspector.next_column();

        static ecs::ColliderType add_type = ecs::ColliderType::Box;
        if (ImGui::BeginCombo("##collider_add_type", detail::collider_type_label(add_type)))
        {
            const ecs::ColliderType options[] = {
                ecs::ColliderType::Box,
                ecs::ColliderType::Sphere,
                ecs::ColliderType::Capsule,
                ecs::ColliderType::ConvexHull,
                ecs::ColliderType::TriangleMesh,
                ecs::ColliderType::Heightfield,
                ecs::ColliderType::AABB
            };
            for (ecs::ColliderType option : options)
            {
                const bool is_selected = (add_type == option);
                if (ImGui::Selectable(detail::collider_type_label(option), is_selected))
                    add_type = option;
                if (is_selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        if (ImGui::Button("Add Collider"))
        {
            ecs::ColliderDesc desc{};
            desc.id = detail::next_collider_id(*comp);
            desc.type = add_type;
            comp->colliders.push_back(desc);
            modified = true;
        }
        // Hover hint for collider creation.
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Add a collider entry with defaults.");

        ecs::Entity selected_entity{};
        if (ctx.entity_selection && !ctx.entity_selection->empty())
            selected_entity = ctx.entity_selection->first();

        bool can_append_bounds = false;
        ecs::ModelComponent* model_comp = nullptr;
        auto registry_sp = eeng::try_get_registry(ctx, "ColliderComponentInspect");
        if (registry_sp && selected_entity.has_id() && registry_sp->valid(selected_entity))
        {
            model_comp = registry_sp->try_get<ecs::ModelComponent>(selected_entity);
            if (model_comp && model_comp->model_ref.is_bound())
                can_append_bounds = true;
        }

        // Mesh-based AABB generation from the entity's ModelComponent.
        inspector.row();
        ImGui::TextDisabled("Mesh Bounds");
        inspector.next_column();

        static bool clear_then_append = false;
        ImGui::BeginDisabled(!can_append_bounds);
        if (ImGui::Button("Append model bounds"))
        {
            if (model_comp)
                modified |= detail::append_aabb_colliders_from_model(*comp, *model_comp, ctx, clear_then_append);
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::Checkbox("Clear then append", &clear_then_append);
        // Hover hint for the clear/append behavior.
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Clear existing colliders before appending bounds.");

        for (std::size_t i = 0; i < comp->colliders.size();)
        {
            bool removed = false;
            std::string label = "Collider ";
            label += std::to_string(i);

            if (inspector.begin_node(label.c_str()))
            {
                if (ImGui::Button(("Remove##collider_" + std::to_string(i)).c_str()))
                {
                    comp->colliders.erase(comp->colliders.begin() + static_cast<std::ptrdiff_t>(i));
                    modified = true;
                    removed = true;
                }

                if (!removed)
                {
                    auto desc_any = entt::forward_as_meta(comp->colliders[i]);
                    modified |= inspect_ColliderDesc(desc_any, inspector, ctx);
                }

                inspector.end_node();
            }

            if (!removed)
                ++i;
        }

        return modified;
    }
} // namespace eeng::editor
