#pragma once
#include <future>
#include <functional>

struct IExecutor 
{
    virtual ~IExecutor() = default;
    virtual void post(std::function<void()> fn) = 0;
};