#include "Engine.hpp"
#include "EngineFactory.hpp"
#include "Module1/project2/ReferenceGame.hpp"

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    auto engine = eeng::make_default_engine();
    if (!engine->init("eduxEngine - project2", 1280, 720))
        return -1;

    // With editor
    engine->run_editor<eeng::project2::ReferenceGame>();

    // Without editor
    // engine->run_game<eeng::project2::ReferenceGame>();

    return 0;
}
