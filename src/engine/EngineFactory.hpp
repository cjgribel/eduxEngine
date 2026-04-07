// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once
#include <memory>

namespace eeng
{
    class Engine;
    
    std::unique_ptr<Engine> make_default_engine();
}