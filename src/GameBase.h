// Created by Carl Johan Gribel.
// Licensed under the MIT License. See LICENSE file for details.

#ifndef GameBase_h
#define GameBase_h
#pragma once

#include "engineapi/IApp.hpp"

namespace eeng
{
    // Legacy alias kept for transition; prefer IApp for new code.
    using GameBase = IApp;
} // namespace eeng

#endif
