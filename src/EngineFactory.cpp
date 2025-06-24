#include "EngineFactory.hpp"
#include "Engine.hpp"
#include "EntityManager.hpp"
#include "ResourceManager.hpp"

namespace eeng
{
    std::unique_ptr<Engine> make_default_engine()
    {
        std::shared_ptr<EngineContext> ctx = std::make_shared<EngineContext>(
            std::make_unique<EntityManager>(),
            std::make_unique<ResourceManager>()
        );
        return std::make_unique<Engine>(ctx);
    }
}