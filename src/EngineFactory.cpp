#include "EngineFactory.hpp"
#include "Engine.hpp"
#include "ecs/EntityManager.hpp"
#include "ResourceManager.hpp"
#include "GuiManager.hpp"
#include "InputManager.hpp"
#include "LogManager.hpp"

namespace eeng
{
    std::unique_ptr<Engine> make_default_engine()
    {
        std::shared_ptr<EngineContext> ctx = std::make_shared<EngineContext>(
            std::make_unique<EntityManager>(),
            std::make_unique<ResourceManager>(),
            std::make_unique<GuiManager>(),
            std::make_unique<InputManager>(),
            std::make_shared<LogManager>()
        );
        return std::make_unique<Engine>(ctx);
    }
}