// Created by Carl Johan Gribel.
// Licensed under the MIT License. See LICENSE file for details.

#include "InputManager.hpp"
#include <SDL2/SDL.h>
#include <iostream>

struct eeng::InputManager::Impl 
{
    std::unordered_map<SDL_Keycode, Key> sdlToKeyMap;
    std::unordered_map<Key, bool> keyStates;

    MouseState mouseState;
    ControllerMap controllers;

    Impl() 
    {
        // Letters
        sdlToKeyMap[SDLK_a] = Key::A; sdlToKeyMap[SDLK_b] = Key::B; sdlToKeyMap[SDLK_c] = Key::C;
        sdlToKeyMap[SDLK_d] = Key::D; sdlToKeyMap[SDLK_e] = Key::E; sdlToKeyMap[SDLK_f] = Key::F;
        sdlToKeyMap[SDLK_g] = Key::G; sdlToKeyMap[SDLK_h] = Key::H; sdlToKeyMap[SDLK_i] = Key::I;
        sdlToKeyMap[SDLK_j] = Key::J; sdlToKeyMap[SDLK_k] = Key::K; sdlToKeyMap[SDLK_l] = Key::L;
        sdlToKeyMap[SDLK_m] = Key::M; sdlToKeyMap[SDLK_n] = Key::N; sdlToKeyMap[SDLK_o] = Key::O;
        sdlToKeyMap[SDLK_p] = Key::P; sdlToKeyMap[SDLK_q] = Key::Q; sdlToKeyMap[SDLK_r] = Key::R;
        sdlToKeyMap[SDLK_s] = Key::S; sdlToKeyMap[SDLK_t] = Key::T; sdlToKeyMap[SDLK_u] = Key::U;
        sdlToKeyMap[SDLK_v] = Key::V; sdlToKeyMap[SDLK_w] = Key::W; sdlToKeyMap[SDLK_x] = Key::X;
        sdlToKeyMap[SDLK_y] = Key::Y; sdlToKeyMap[SDLK_z] = Key::Z;

        // Numbers
        sdlToKeyMap[SDLK_0] = Key::Num0; sdlToKeyMap[SDLK_1] = Key::Num1; sdlToKeyMap[SDLK_2] = Key::Num2;
        sdlToKeyMap[SDLK_3] = Key::Num3; sdlToKeyMap[SDLK_4] = Key::Num4; sdlToKeyMap[SDLK_5] = Key::Num5;
        sdlToKeyMap[SDLK_6] = Key::Num6; sdlToKeyMap[SDLK_7] = Key::Num7; sdlToKeyMap[SDLK_8] = Key::Num8;
        sdlToKeyMap[SDLK_9] = Key::Num9;

        // Modifiers
        sdlToKeyMap[SDLK_LSHIFT] = Key::LeftShift; sdlToKeyMap[SDLK_RSHIFT] = Key::RightShift;
        sdlToKeyMap[SDLK_LCTRL] = Key::LeftCtrl;   sdlToKeyMap[SDLK_RCTRL] = Key::RightCtrl;
        sdlToKeyMap[SDLK_LALT] = Key::LeftAlt;     sdlToKeyMap[SDLK_RALT] = Key::RightAlt;

        // Navigation
        sdlToKeyMap[SDLK_UP] = Key::Up; sdlToKeyMap[SDLK_DOWN] = Key::Down;
        sdlToKeyMap[SDLK_LEFT] = Key::Left; sdlToKeyMap[SDLK_RIGHT] = Key::Right;

        // Special Keys
        sdlToKeyMap[SDLK_SPACE] = Key::Space;
        sdlToKeyMap[SDLK_RETURN] = Key::Enter;
        sdlToKeyMap[SDLK_BACKSPACE] = Key::Backspace;
        sdlToKeyMap[SDLK_TAB] = Key::Tab;
        sdlToKeyMap[SDLK_ESCAPE] = Key::Escape;

        // Function Keys
        sdlToKeyMap[SDLK_F1] = Key::F1; sdlToKeyMap[SDLK_F2] = Key::F2; sdlToKeyMap[SDLK_F3] = Key::F3;
        sdlToKeyMap[SDLK_F4] = Key::F4; sdlToKeyMap[SDLK_F5] = Key::F5; sdlToKeyMap[SDLK_F6] = Key::F6;
        sdlToKeyMap[SDLK_F7] = Key::F7; sdlToKeyMap[SDLK_F8] = Key::F8; sdlToKeyMap[SDLK_F9] = Key::F9;
        sdlToKeyMap[SDLK_F10] = Key::F10; sdlToKeyMap[SDLK_F11] = Key::F11; sdlToKeyMap[SDLK_F12] = Key::F12;
    }

    ~Impl() 
    {
        for (auto& [id, state] : controllers) 
        {
            SDL_GameControllerClose(SDL_GameControllerFromInstanceID(id));
        }
    }

    void HandleEvent(const SDL_Event& event) 
    {
        switch (event.type) 
        {
        case SDL_MOUSEMOTION:
            mouseState.x = event.motion.x;
            mouseState.y = event.motion.y;
            break;

        case SDL_MOUSEBUTTONDOWN:
            if (event.button.button == SDL_BUTTON_LEFT) mouseState.leftButton = true;
            if (event.button.button == SDL_BUTTON_RIGHT) mouseState.rightButton = true;
            break;

        case SDL_MOUSEBUTTONUP:
            if (event.button.button == SDL_BUTTON_LEFT) mouseState.leftButton = false;
            if (event.button.button == SDL_BUTTON_RIGHT) mouseState.rightButton = false;
            break;

        case SDL_KEYDOWN:
        case SDL_KEYUP: {
            auto it = sdlToKeyMap.find(event.key.keysym.sym);
            if (it != sdlToKeyMap.end()) {
                keyStates[it->second] = (event.type == SDL_KEYDOWN);
            }
            break;
        }
        case SDL_CONTROLLERAXISMOTION:
            if (auto controller = controllers.find(event.caxis.which); controller != controllers.end()) 
            {
                auto& state = controller->second;
                if (event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTX)
                    state.axisLeftX = event.caxis.value / 32767.0f;
                else if (event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTY)
                    state.axisLeftY = event.caxis.value / 32767.0f;
                else if (event.caxis.axis == SDL_CONTROLLER_AXIS_RIGHTX)
                    state.axisRightX = event.caxis.value / 32767.0f;
                else if (event.caxis.axis == SDL_CONTROLLER_AXIS_RIGHTY)
                    state.axisRightY = event.caxis.value / 32767.0f;
                else if (event.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT)
                    state.triggerLeft = event.caxis.value / 32767.0f;
                else if (event.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT)
                    state.triggerRight = event.caxis.value / 32767.0f;
            }
            break;
        case SDL_CONTROLLERBUTTONDOWN:
            controllers[event.cbutton.which].buttonStates[event.cbutton.button] = true;
            break;

        case SDL_CONTROLLERBUTTONUP:
            controllers[event.cbutton.which].buttonStates[event.cbutton.button] = false;
            break;

        case SDL_CONTROLLERDEVICEADDED: {
            SDL_GameController* controller = SDL_GameControllerOpen(event.cdevice.which);
            if (controller) {
                int id = SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller));
                controllers[id] = ControllerState{
                    .name = SDL_GameControllerName(controller) ? SDL_GameControllerName(controller) : "Unknown"
                };
                std::cout << "Controller connected: " << id << ", " << controllers[id].name << std::endl;
            }
            break;
        }

        case SDL_CONTROLLERDEVICEREMOVED: {
            int id = event.cdevice.which;
            if (controllers.find(id) != controllers.end()) {
                std::cout << "Controller disconnected: " << id << ", " << controllers[id].name << std::endl;
                SDL_GameControllerClose(SDL_GameControllerFromInstanceID(id));
                controllers.erase(id);
            }
            break;
        }
        }
    }

    int GetConnectedControllerCount() const 
    {
        return static_cast<int>(controllers.size());
    }
};

// InputManager implementation

eeng::InputManager::InputManager() : pImpl(new Impl()) {}
eeng::InputManager::~InputManager() = default; // { delete pImpl; }

void eeng::InputManager::HandleEvent(const void* event) 
{
    pImpl->HandleEvent(*reinterpret_cast<const SDL_Event*>(event));
}

bool eeng::InputManager::IsKeyPressed(Key key) const 
{
    auto it = pImpl->keyStates.find(key);
    return it != pImpl->keyStates.end() && it->second;
}

bool eeng::InputManager::IsMouseButtonDown(int button) const 
{
    if (button == SDL_BUTTON_LEFT) return pImpl->mouseState.leftButton;
    if (button == SDL_BUTTON_RIGHT) return pImpl->mouseState.rightButton;
    return false;
}

const eeng::InputManager::MouseState& eeng::InputManager::GetMouseState() const 
{
    return pImpl->mouseState;
}

const eeng::InputManager::ControllerState& eeng::InputManager::GetControllerState(int controllerIndex) const 
{
    static ControllerState emptyController;
    auto it = pImpl->controllers.find(controllerIndex);
    return it != pImpl->controllers.end() ? it->second : emptyController;
}

int eeng::InputManager::GetConnectedControllerCount() const 
{
    return pImpl->GetConnectedControllerCount();
}

const eeng::InputManager::ControllerMap& eeng::InputManager::GetControllers() const
{
    return pImpl->controllers;
}