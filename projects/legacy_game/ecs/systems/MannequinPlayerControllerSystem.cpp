// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "ecs/systems/MannequinPlayerControllerSystem.hpp"

#include <algorithm>
#include <cmath>
#include <string_view>

#include <SDL2/SDL.h>

#include "EngineContextHelpers.hpp"
#include "ecs/AnimationGraphComponent.hpp"
#include "ecs/PhysicsComponents.hpp"
#include "ecs/MannequinPlayerControllerComponent.hpp"
#include "ecs/TransformComponent.hpp"
#include "ecs/systems/PhysicsSystem.hpp"
#include "engineapi/IInputManager.hpp"
#include "assets/types/AnimationGraphAsset.hpp"

namespace
{
    float apply_deadzone(float value, float deadzone)
    {
        if (std::fabs(value) <= deadzone)
            return 0.0f;
        return value;
    }

    float clamp_axis(float value)
    {
        return std::min(std::max(value, -1.0f), 1.0f);
    }

    bool controller_button_down(const eeng::IInputManager::ControllerState& state, int button)
    {
        auto it = state.buttonStates.find(button);
        return it != state.buttonStates.end() && it->second;
    }

    bool rising_edge(bool down, bool& was_down)
    {
        const bool pressed = down && !was_down;
        was_down = down;
        return pressed;
    }

    void update_mouse_aim(
        eeng::ecs::MannequinPlayerControllerComponent& controller,
        const eeng::IInputManager::MouseState& mouse)
    {
        if (!controller.has_mouse_origin)
        {
            controller.mouse_x = mouse.x;
            controller.mouse_y = mouse.y;
            controller.has_mouse_origin = true;
            return;
        }

        const int dx = mouse.x - controller.mouse_x;
        const int dy = mouse.y - controller.mouse_y;
        controller.mouse_x = mouse.x;
        controller.mouse_y = mouse.y;

        controller.aim_x = clamp_axis(controller.aim_x + static_cast<float>(dx) * controller.mouse_sensitivity);
        controller.aim_y = clamp_axis(controller.aim_y - static_cast<float>(dy) * controller.mouse_sensitivity);
    }

    bool resolve_param_slot(
        const eeng::assets::AnimationGraphAsset& graph,
        std::string_view name,
        eeng::assets::AnimGraphParamType& type_out,
        std::size_t& index_out)
    {
        if (name.empty())
            return false;

        if (graph.runtime.built)
        {
            auto it = graph.runtime.param_index.find(std::string(name));
            if (it == graph.runtime.param_index.end())
                return false;
            if (it->second >= graph.runtime.param_slots.size())
                return false;

            const auto& slot = graph.runtime.param_slots[it->second];
            type_out = slot.type;
            index_out = slot.index;
            return slot.type != eeng::assets::AnimGraphParamType::Invalid;
        }

        std::size_t float_index = 0;
        std::size_t int_index = 0;
        std::size_t bool_index = 0;
        std::size_t trigger_index = 0;
        for (const auto& param : graph.params)
        {
            if (param.name == name)
            {
                type_out = param.type;
                switch (param.type)
                {
                case eeng::assets::AnimGraphParamType::Float: index_out = float_index; break;
                case eeng::assets::AnimGraphParamType::Int: index_out = int_index; break;
                case eeng::assets::AnimGraphParamType::Bool: index_out = bool_index; break;
                case eeng::assets::AnimGraphParamType::Trigger: index_out = trigger_index; break;
                default: index_out = 0; break;
                }
                return param.type != eeng::assets::AnimGraphParamType::Invalid;
            }

            switch (param.type)
            {
            case eeng::assets::AnimGraphParamType::Float: ++float_index; break;
            case eeng::assets::AnimGraphParamType::Int: ++int_index; break;
            case eeng::assets::AnimGraphParamType::Bool: ++bool_index; break;
            case eeng::assets::AnimGraphParamType::Trigger: ++trigger_index; break;
            default: break;
            }
        }
        return false;
    }

    bool set_param_float(
        const eeng::assets::AnimationGraphAsset& graph,
        eeng::ecs::AnimGraphInstance& instance,
        std::string_view name,
        float value)
    {
        eeng::assets::AnimGraphParamType type = eeng::assets::AnimGraphParamType::Invalid;
        std::size_t index = 0;
        if (!resolve_param_slot(graph, name, type, index))
            return false;

        switch (type)
        {
        case eeng::assets::AnimGraphParamType::Float:
            if (index < instance.float_params.size())
            {
                instance.float_params[index] = value;
                return true;
            }
            break;
        case eeng::assets::AnimGraphParamType::Int:
            if (index < instance.int_params.size())
            {
                instance.int_params[index] = static_cast<int>(std::round(value));
                return true;
            }
            break;
        case eeng::assets::AnimGraphParamType::Bool:
            if (index < instance.bool_params.size())
            {
                instance.bool_params[index] = value != 0.0f ? 1u : 0u;
                return true;
            }
            break;
        case eeng::assets::AnimGraphParamType::Trigger:
            if (index < instance.trigger_params.size())
            {
                instance.trigger_params[index] = value != 0.0f ? 1u : 0u;
                return true;
            }
            break;
        default:
            break;
        }
        return false;
    }

    bool set_param_bool(
        const eeng::assets::AnimationGraphAsset& graph,
        eeng::ecs::AnimGraphInstance& instance,
        std::string_view name,
        bool value)
    {
        eeng::assets::AnimGraphParamType type = eeng::assets::AnimGraphParamType::Invalid;
        std::size_t index = 0;
        if (!resolve_param_slot(graph, name, type, index))
            return false;

        switch (type)
        {
        case eeng::assets::AnimGraphParamType::Bool:
            if (index < instance.bool_params.size())
            {
                instance.bool_params[index] = value ? 1u : 0u;
                return true;
            }
            break;
        case eeng::assets::AnimGraphParamType::Trigger:
            if (index < instance.trigger_params.size())
            {
                instance.trigger_params[index] = value ? 1u : 0u;
                return true;
            }
            break;
        case eeng::assets::AnimGraphParamType::Float:
            if (index < instance.float_params.size())
            {
                instance.float_params[index] = value ? 1.0f : 0.0f;
                return true;
            }
            break;
        case eeng::assets::AnimGraphParamType::Int:
            if (index < instance.int_params.size())
            {
                instance.int_params[index] = value ? 1 : 0;
                return true;
            }
            break;
        default:
            break;
        }
        return false;
    }
}

namespace eeng::ecs::systems
{
    void MannequinPlayerControllerSystem::update(entt::registry& registry, EngineContext& ctx, float)
    {
        auto* input = ctx.input_manager.get();
        if (!input)
            return;

        auto rm = eeng::try_get_resource_manager(ctx, "MannequinPlayerControllerSystem");
        if (!rm)
            return;

        // Policy: Use same-entity component access so animation control stays local to the entity.
        auto view = registry.view<ecs::MannequinPlayerControllerComponent, ecs::AnimationGraphComponent>();
        for (auto entity : view)
        {
            auto& controller = view.get<ecs::MannequinPlayerControllerComponent>(entity);
            auto& graph_comp = view.get<ecs::AnimationGraphComponent>(entity);

            if (!controller.enabled || !graph_comp.enabled)
                continue;
            if (!graph_comp.graph_ref.is_bound() || !graph_comp.instance.initialized)
                continue;

            const eeng::IInputManager::ControllerState* controller_state = nullptr;
            const auto& controllers = input->GetControllers();
            if (!controllers.empty())
            {
                if (controller.controller_id >= 0)
                {
                    auto it = controllers.find(controller.controller_id);
                    if (it != controllers.end())
                        controller_state = &it->second;
                }
                else
                {
                    controller_state = &controllers.begin()->second;
                }
            }

            bool use_controller = controller_state != nullptr;
            bool use_keyboard = !use_controller && controller.use_keyboard_fallback;

            float move_x = 0.0f;
            float move_y = 0.0f;
            float aim_x = controller.aim_x;
            float aim_y = controller.aim_y;

            bool jump_down = false;
            bool fire_down = false;
            bool reload_down = false;
            bool hit0_down = false;
            bool hit1_down = false;
            bool hit2_down = false;
            bool hit3_down = false;

            if (use_controller)
            {
                move_x = apply_deadzone(controller_state->axisLeftX, controller.stick_deadzone);
                move_y = apply_deadzone(-controller_state->axisLeftY, controller.stick_deadzone);
                aim_x = apply_deadzone(controller_state->axisRightX, controller.stick_deadzone);
                aim_y = apply_deadzone(-controller_state->axisRightY, controller.stick_deadzone);

                jump_down = controller_button_down(*controller_state, SDL_CONTROLLER_BUTTON_A);
                fire_down = controller_state->triggerRight > controller.trigger_deadzone
                    || controller_button_down(*controller_state, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);
                reload_down = controller_state->triggerLeft > controller.trigger_deadzone
                    || controller_button_down(*controller_state, SDL_CONTROLLER_BUTTON_LEFTSHOULDER);

                hit0_down = controller_button_down(*controller_state, SDL_CONTROLLER_BUTTON_Y);
                hit1_down = controller_button_down(*controller_state, SDL_CONTROLLER_BUTTON_X);
                hit2_down = controller_button_down(*controller_state, SDL_CONTROLLER_BUTTON_B);
                hit3_down = controller_button_down(*controller_state, SDL_CONTROLLER_BUTTON_BACK);
            }
            else if (use_keyboard)
            {
                move_x = (input->IsKeyPressed(eeng::IInputManager::Key::D) ? 1.0f : 0.0f)
                    - (input->IsKeyPressed(eeng::IInputManager::Key::A) ? 1.0f : 0.0f);
                move_y = (input->IsKeyPressed(eeng::IInputManager::Key::W) ? 1.0f : 0.0f)
                    - (input->IsKeyPressed(eeng::IInputManager::Key::S) ? 1.0f : 0.0f);

                if (controller.use_mouse_aim)
                {
                    update_mouse_aim(controller, input->GetMouseState());
                    aim_x = controller.aim_x;
                    aim_y = controller.aim_y;
                }
                else
                {
                    controller.has_mouse_origin = false;
                }

                const float key_aim_x = (input->IsKeyPressed(eeng::IInputManager::Key::Right) ? 1.0f : 0.0f)
                    - (input->IsKeyPressed(eeng::IInputManager::Key::Left) ? 1.0f : 0.0f);
                const float key_aim_y = (input->IsKeyPressed(eeng::IInputManager::Key::Up) ? 1.0f : 0.0f)
                    - (input->IsKeyPressed(eeng::IInputManager::Key::Down) ? 1.0f : 0.0f);
                if (key_aim_x != 0.0f || key_aim_y != 0.0f)
                {
                    controller.aim_x = key_aim_x;
                    controller.aim_y = key_aim_y;
                    aim_x = controller.aim_x;
                    aim_y = controller.aim_y;
                }

                jump_down = input->IsKeyPressed(eeng::IInputManager::Key::Space);
                fire_down = input->IsMouseButtonDown(SDL_BUTTON_LEFT);
                reload_down = input->IsKeyPressed(eeng::IInputManager::Key::R);
                hit0_down = input->IsKeyPressed(eeng::IInputManager::Key::Num1);
                hit1_down = input->IsKeyPressed(eeng::IInputManager::Key::Num2);
                hit2_down = input->IsKeyPressed(eeng::IInputManager::Key::Num3);
                hit3_down = input->IsKeyPressed(eeng::IInputManager::Key::Num4);
            }
            else
            {
                controller.has_mouse_origin = false;
                controller.aim_x = 0.0f;
                controller.aim_y = 0.0f;
                aim_x = controller.aim_x;
                aim_y = controller.aim_y;
            }

            move_x = clamp_axis(move_x);
            move_y = clamp_axis(move_y);
            aim_x = clamp_axis(aim_x);
            aim_y = clamp_axis(aim_y);

            const bool jump_press = rising_edge(jump_down, controller.last_jump);
            const bool fire_press = rising_edge(fire_down, controller.last_fire);
            const bool reload_press = rising_edge(reload_down, controller.last_reload);
            const bool hit0_press = rising_edge(hit0_down, controller.last_hit0);
            const bool hit1_press = rising_edge(hit1_down, controller.last_hit1);
            const bool hit2_press = rising_edge(hit2_down, controller.last_hit2);
            const bool hit3_press = rising_edge(hit3_down, controller.last_hit3);

            eeng::try_read_asset_ref(
                *rm,
                graph_comp.graph_ref,
                ctx,
                "MannequinPlayerControllerSystem",
                "Missing AnimationGraphAsset for MannequinPlayerControllerSystem:",
                [&](const assets::AnimationGraphAsset& graph)
                {
                    set_param_float(graph, graph_comp.instance, controller.move_x_param, move_x);
                    set_param_float(graph, graph_comp.instance, controller.move_y_param, move_y);
                    set_param_float(graph, graph_comp.instance, controller.aim_x_param, aim_x);
                    set_param_float(graph, graph_comp.instance, controller.aim_y_param, aim_y);

                    // Policy: Treat action inputs as single-frame triggers to avoid looping transitions.
                    set_param_bool(graph, graph_comp.instance, controller.jump_param, jump_press);
                    set_param_bool(graph, graph_comp.instance, controller.fire_param, fire_press);
                    set_param_bool(graph, graph_comp.instance, controller.reload_param, reload_press);
                    set_param_bool(graph, graph_comp.instance, controller.hit0_param, hit0_press);
                    set_param_bool(graph, graph_comp.instance, controller.hit1_param, hit1_press);
                    set_param_bool(graph, graph_comp.instance, controller.hit2_param, hit2_press);
                    set_param_bool(graph, graph_comp.instance, controller.hit3_param, hit3_press);
                });

            // Debug raycast: cast a short ray downward from the player.
            if (physics_system_)
            {
                auto* tfm = registry.try_get<ecs::TransformComponent>(entity);
                if (tfm)
                {
                    auto& debug = registry.get_or_emplace<ecs::PhysicsRaycastDebugComponent>(entity);

                    // Use local position for now (player entities are expected to be root-level).
                    const glm::vec3 origin = tfm->position;
                    const glm::vec3 direction = glm::vec3(0.0f, -1.0f, 0.0f);
                    const float length = 2.0f;

                    ecs::systems::PhysicsSystem::RaycastFilter filter{};
                    filter.include_triggers = false;

                    ecs::systems::PhysicsSystem::RaycastHit hit{};
                    const bool hit_any = physics_system_->raycast(origin, direction, length, hit, filter);

                    ecs::PhysicsRaycastDebugRay ray{};
                    ray.origin = origin;
                    ray.direction = direction;
                    ray.length = length;
                    ray.hit = hit_any;
                    ray.hit_point = hit.point;
                    ray.hit_normal = hit.normal;
                    ray.hit_entity = hit.entity;
                    ray.hit_collider = hit.collider_id;
                    ray.hit_is_trigger = hit.is_trigger;
                    debug.rays.push_back(ray);
                }
            }
        }
    }
}
