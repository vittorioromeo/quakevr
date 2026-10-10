// vr_diskcache.hpp -- work kept on disk between sessions (vr_texcache.cpp): `<gamedir>/cache/<kind>/<build>/<key>.<ext>`,
// <key> a hash of everything the work reads, <build> the working code's own build (a changed maker never reads an old
// file). A file is 4 bytes of magic, the version, two numbers of the caller's, then the data; it is written on the pool
// (beside its place, then renamed: never read half written).

#pragma once

#include "Zancle/Container/Vector.hpp"

#include <stddef.h>

namespace qvr::diskcache
{

// The file of `key`: its two numbers and its data, or false (none, another version, short).
[[nodiscard]] bool read(const char* kind, const char* build, unsigned long long key, const char* ext, const char (&magic)[5],
    unsigned& a, unsigned& b, za::Vector<char>& data);

// The file of `key` written on the pool (its folder made, the kind's other builds' folders removed, here).
void write(const char* kind, const char* build, unsigned long long key, const char* ext, const char (&magic)[5], unsigned a,
    unsigned b, const void* data, size_t length);

// FNV-1a, 64 bits (the keys' and the builds' hash): `h` 14695981039346656037 to start.
[[nodiscard]] inline unsigned long long fnv(unsigned long long h, const void* p, size_t n)
{
    const unsigned char* bytes = static_cast<const unsigned char*>(p);
    for(size_t i = 0; i < n; i++)
    {
        h = (h ^ bytes[i]) * 1099511628211ull;
    }
    return h;
}

constexpr unsigned long long fnvStart = 14695981039346656037ull;

} // namespace qvr::diskcache
