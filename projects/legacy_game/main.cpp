#include "Engine.hpp"
#include "EngineFactory.hpp"
#include "Game.hpp"
#include <filesystem>

int main(int argc, char* argv[])
{
    std::cout << "Starting eduxEngine..." << std::endl;

    auto engine = eeng::make_default_engine();

    if (!engine->init("eduxEngine", 1920, 1080))
    {
        std::cerr << "Engine failed to initialize." << std::endl;
        return -1;
    }

    const std::filesystem::path project_config =
        std::filesystem::path(EENG_SOURCE_DIR) / "projects/legacy_game/project.json";
    engine->run_editor<Game>(project_config);

    std::cout << "Exiting eduxEngine." << std::endl;
    return 0;
}
