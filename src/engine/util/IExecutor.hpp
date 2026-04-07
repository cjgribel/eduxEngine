// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once
#include <future>
#include <functional>

struct IExecutor 
{
    virtual ~IExecutor() = default;
    virtual void post(std::function<void()> fn) = 0;
};