// vr_alloccount.cpp -- C++ allocations counted: the global operator new replaced by one that counts, per thread, then
// calls malloc. The profiler reads the main thread's count at each frame's end (vr_profile_report's "allocations": new
// calls a frame; the code a frame runs is meant to reuse its buffers, see docs/vr-port/CODE_STYLE.md, "Scratch buffers
// and caches"). The count is a thread_local increment: no lock, no shared cache line. The engine's C code (malloc, the
// zone, the hunk) is not counted.

#include "vr_alloccount.hpp"

#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Base/SizeT.hpp"

#include <new>

namespace
{

thread_local za::U64 allocations = 0;

[[nodiscard]] void* allocate(za::SizeT size)
{
    ++allocations;
    if(void* p = malloc(size ? size : 1))
    {
        return p;
    }
    throw std::bad_alloc{}; // ZANCLE-TODO (none possible: the language's contract for a replaced operator new)
}

} // namespace

// The other forms (the arrays', nothrow, sized delete) forward to these in the standard library's defaults (MSVC and
// libstdc++); the aligned forms keep their own allocator and are not counted (the VR code uses none per frame).
void* operator new(za::SizeT size)
{
    return allocate(size);
}

void* operator new[](za::SizeT size)
{
    return allocate(size);
}

void operator delete(void* p) noexcept
{
    free(p);
}

void operator delete[](void* p) noexcept
{
    free(p);
}

void operator delete(void* p, za::SizeT) noexcept
{
    free(p);
}

void operator delete[](void* p, za::SizeT) noexcept
{
    free(p);
}

namespace qvr::alloccount
{

za::U64 thisThread()
{
    return allocations;
}

} // namespace qvr::alloccount
