// vr_alloccount.cpp -- C++ allocations counted: the global operator new replaced by one that counts, per thread, then
// calls malloc. The profiler reads the main thread's count at each frame's end (vr_profile_report's "allocations": new
// calls a frame; the code a frame runs is meant to reuse its buffers, see docs/vr-port/CODE_STYLE.md, "Scratch buffers
// and caches"). The count is a thread_local increment: no lock, no shared cache line. The engine's C code (malloc, the
// zone, the hunk) is not counted.

#include "vr_alloccount.hpp"

#include <cstdlib>
#include <new>

namespace
{

thread_local std::uint64_t allocations = 0;

[[nodiscard]] void* allocate(std::size_t size)
{
    ++allocations;
    if(void* p = std::malloc(size ? size : 1))
    {
        return p;
    }
    throw std::bad_alloc{};
}

} // namespace

// The other forms (the arrays', nothrow, sized delete) forward to these in the standard library's defaults (MSVC and
// libstdc++); the aligned forms keep their own allocator and are not counted (the VR code uses none per frame).
void* operator new(std::size_t size)
{
    return allocate(size);
}

void* operator new[](std::size_t size)
{
    return allocate(size);
}

void operator delete(void* p) noexcept
{
    std::free(p);
}

void operator delete[](void* p) noexcept
{
    std::free(p);
}

void operator delete(void* p, std::size_t) noexcept
{
    std::free(p);
}

void operator delete[](void* p, std::size_t) noexcept
{
    std::free(p);
}

namespace qvr::alloccount
{

std::uint64_t thisThread()
{
    return allocations;
}

} // namespace qvr::alloccount
