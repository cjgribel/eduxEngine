// Created by Carl Johan Gribel.
// Licensed under the MIT License. See LICENSE file for details.

// InputManager.hpp
#pragma once

#include "IInputManager.hpp"
#include <memory>

namespace eeng {

    class InputManager final : public IInputManager
    {
    public:
        InputManager();
        ~InputManager() override;

        // From IInputManager
        bool IsKeyPressed(Key key) const override;
        bool IsMouseButtonDown(int button) const override;
        const MouseState& GetMouseState() const override;
        const ControllerState& GetControllerState(int controllerIndex) const override;
        int GetConnectedControllerCount() const override;
        const ControllerMap& GetControllers() const override;

        // Not part of IInputManager
        void HandleEvent(const void* event);

    private:
        struct Impl;
        std::unique_ptr<Impl> pImpl;
    };

    using InputManagerPtr = std::shared_ptr<InputManager>;
}
