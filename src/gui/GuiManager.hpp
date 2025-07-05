// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once
#include "IGuiManager.hpp"
#include "EngineContext.hpp"
#include <unordered_map>

namespace eeng
{
    class GuiManager : public IGuiManager
    {
    public:

        void init() override;
        void release() override;

        void set_flag(GuiFlags flag, bool enabled) override;
        bool is_flag_enabled(GuiFlags flag) const override;

        void draw(EngineContext& ctx) const override;

    private:
        void draw_log(EngineContext& ctx) const;
        void draw_storage(EngineContext& ctx) const;
        void draw_engine_info(EngineContext& ctx) const;

        std::unordered_map<GuiFlags, bool> flags;
    };

} // namespace eeng