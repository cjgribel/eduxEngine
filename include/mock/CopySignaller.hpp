#pragma once
#include <iostream>

namespace eeng
{
    class CopySignaller
    {
    public:
        int data = 0;

        CopySignaller()
        {
            std::cout << "CopySignaller::CopySignaller() [default]" << std::endl;
        }

        CopySignaller(const CopySignaller& other)
            : data(other.data)
        {
            std::cout << "CopySignaller::CopySignaller(const CopySignaller&) [copy]" << std::endl;
        }

        CopySignaller(CopySignaller&& other) noexcept
            : data(other.data)
        {
            std::cout << "CopySignaller::CopySignaller(CopySignaller&&) [move]" << std::endl;
            other.data = 0;
        }

        CopySignaller& operator=(const CopySignaller& other)
        {
            if (this != &other)
            {
                data = other.data;
            }

            std::cout << "CopySignaller::operator=(const CopySignaller&) [copy assign]" << std::endl;
            return *this;
        }

        CopySignaller& operator=(CopySignaller&& other) noexcept
        {
            if (this != &other)
            {
                data = other.data;
                other.data = 0;
            }

            std::cout << "CopySignaller::operator=(CopySignaller&&) [move assign]" << std::endl;
            return *this;
        }

        ~CopySignaller()
        {
            std::cout << "CopySignaller::~CopySignaller() [destructor]" << std::endl;
        }
    };
}
