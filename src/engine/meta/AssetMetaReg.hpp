// Licensed under the MIT License. See LICENSE file for details.

#pragma once
#ifndef AssetMetaReg_hpp
#define AssetMetaReg_hpp

#include <iostream>

namespace eeng
{
    struct EngineContext;
    void register_asset_meta_types(EngineContext& ctx);

} // namespace eeng

#endif // AssetMetaReg_hpp