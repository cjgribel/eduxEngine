// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once
#include <vector>
#include "EngineContext.hpp"
#include "assets/types/ModelAssets.hpp"
// #include "Guid.h"

namespace eeng::gl
{
    struct GpuModelInitResult
    {
        bool ok = false;
        std::string error;
    };

    GpuModelInitResult
        init_gpu_model(
            const eeng::Handle<eeng::assets::GpuModelAsset>& gpu_handle,
            EngineContext& ctx);

    void
        destroy_gpu_model(
            const eeng::Handle<eeng::assets::GpuModelAsset>& gpu_handle,
            EngineContext& ctx);
}
