#pragma once
#include <memory>

namespace eeng
{
    class Engine;
    
    std::unique_ptr<Engine> make_default_engine();
}