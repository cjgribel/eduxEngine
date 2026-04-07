// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "EngineFactory.hpp"
#include "Engine.hpp"
#include "ecs/EntityManager.hpp"
#include "ResourceManager.hpp"
#include "BatchRegistry.hpp"
#include "GuiManager.hpp"
#include "InputManager.hpp"
#include "LogManager.hpp"

namespace eeng
{
    std::unique_ptr<Engine> make_default_engine()
    {
        auto services = std::make_shared<EngineServices>(
            std::make_shared<ResourceManager>(),
            std::make_unique<GuiManager>(),
            std::make_unique<InputManager>(),
            std::make_shared<LogManager>());

        auto world = std::make_shared<WorldState>(
            std::make_unique<EntityManager>(),
            std::make_unique<BatchRegistry>());

        std::shared_ptr<EngineContext> ctx = std::make_shared<EngineContext>(
            std::move(services),
            std::move(world));
        return std::make_unique<Engine>(ctx);
    }
}
