#pragma once
#include "LogGlobals.hpp"

namespace eeng
{
    class CopySignaller
    {
    public:
        int data = 0;

        CopySignaller()
        {
            eeng::LogGlobals::log("CopySignaller::CopySignaller() [default]");
        }

        CopySignaller(const CopySignaller& other)
            : data(other.data)
        {
            eeng::LogGlobals::log("CopySignaller::CopySignaller(const CopySignaller&) [copy]");
        }

        CopySignaller(CopySignaller&& other) noexcept
            : data(other.data)
        {
            eeng::LogGlobals::log("CopySignaller::CopySignaller(CopySignaller&&) [move]");
            other.data = 0;
        }

        CopySignaller& operator=(const CopySignaller& other)
        {
            if (this != &other)
            {
                data = other.data;
            }

            eeng::LogGlobals::log("CopySignaller::operator=(const CopySignaller&) [copy assign]");
            return *this;
        }

        CopySignaller& operator=(CopySignaller&& other) noexcept
        {
            if (this != &other)
            {
                data = other.data;
                other.data = 0;
            }

            eeng::LogGlobals::log("CopySignaller::operator=(CopySignaller&&) [move assign]");
            return *this;
        }

        ~CopySignaller()
        {
            eeng::LogGlobals::log("CopySignaller::~CopySignaller() [destructor]");
        }
    };
}
