// Created by Carl Johan Gribel.
// Licensed under the MIT License. See LICENSE file for details.

#ifndef hash_combine_h
#define hash_combine_h

#include <functional>

namespace detail {

template<typename T>
void hash_combine(size_t& seed, const T& val)
{
    seed ^= std::hash<T>()(val) + 0x9e3779b9 + (seed<<6) + (seed>>2);
}

}

/// hash_combine
/// http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2017/p0814r0.pdf
template<typename... Types>
size_t hash_combine(const Types&... args)
{
    size_t seed = 0;
    (detail::hash_combine(seed,args) , ... );
    return seed;
}

#endif /* hash_combine_h */
