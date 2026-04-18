#include "BlazterGame.hpp"
#include "Engine.hpp"
#include "EngineFactory.hpp"
#include <filesystem>

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    auto engine = eeng::make_default_engine();
    if (!engine->init("eduxEngine - blazter", 1600, 900))
        return -1;

    const std::filesystem::path project_config =
        std::filesystem::path(EENG_SOURCE_DIR) / "projects/blazter/project.json";
    engine->run_editor<eeng::blazter::BlazterGame>(project_config);

    return 0;
}
