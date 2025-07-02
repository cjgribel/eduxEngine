// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once
#include "IGuiManager.hpp"

namespace eeng
{
    class GuiManager : public IGuiManager
    {
        void init() override;
        void release() override;
    };
    
} // namespace eeng