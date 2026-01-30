#include "Engine.hpp"
#include "EngineFactory.hpp"
#include "ReferenceGame.hpp"
#include <filesystem>

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    auto engine = eeng::make_default_engine();
    if (!engine->init("eduxEngine - reference_game", 1280, 720))
        return -1;

    // With editor
    const std::filesystem::path project_config =
        std::filesystem::path(EENG_SOURCE_DIR) / "projects/reference_game/project.json";
    engine->run_editor<eeng::reference_game::ReferenceGame>(project_config);

    // Without editor
    // engine->run_game<eeng::reference_game::ReferenceGame>();

    return 0;
}
