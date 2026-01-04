#include "Engine.hpp"
#include "EngineFactory.hpp"
#include "Game.hpp"

int main(int argc, char* argv[])
{
    std::cout << "Starting eduxEngine..." << std::endl;

    auto engine = eeng::make_default_engine();

    if (!engine->init("eduxEngine", 1600, 900))
    {
        std::cerr << "Engine failed to initialize." << std::endl;
        return -1;
    }

    engine->run<Game>();

    std::cout << "Exiting eduxEngine." << std::endl;
    return 0;
}