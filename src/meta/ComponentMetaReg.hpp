// Licensed under the MIT License. See LICENSE file for details.

#pragma once
#ifndef ComponentMetaReg_hpp
#define ComponentMetaReg_hpp

#include <iostream>

namespace eeng
{
    struct EngineContext;

    void register_component_meta_types(EngineContext& ctx);

} // namespace eeng

#endif // ComponentMetaReg_hpp