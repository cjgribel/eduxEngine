#include "Engine.hpp"
#include "EngineFactory.hpp"
#include "FluidSandboxGame.hpp"
#include <filesystem>

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    auto engine = eeng::make_default_engine();
    if (!engine->init("eduxEngine - fluid_sandbox", 1280, 720))
        return -1;

    // With editor
    const std::filesystem::path project_config =
        std::filesystem::path(EENG_SOURCE_DIR) / "projects/fluid_sandbox/project.json";
    engine->run_editor<eeng::fluid_sandbox::FluidSandboxGame>(project_config);

    // Without editor
    // engine->run_game<eeng::fluid_sandbox::FluidSandboxGame>();

    return 0;
}
