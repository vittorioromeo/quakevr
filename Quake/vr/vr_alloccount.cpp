// vr_alloccount.cpp -- C++ allocations counted: the global operator new replaced by one that counts, per thread, then
// calls malloc. The profiler reads the main thread's count at each frame's end (vr_profile_report's "allocations": new
// calls a frame; the code a frame runs is meant to reuse its buffers, see docs/vr-port/CODE_STYLE.md, "Scratch buffers
// and caches"). The count is a thread_local increment: no lock, no shared cache line. The engine's C code (malloc, the
// zone, the hunk) is not counted.
//
// vr_alloc_sites (vr_allocsites.cpp) traces them by call stack for a few frames: traceBegin marks the calling thread,
// whose allocations then record their stack (RtlCaptureStackBackTrace: no allocation) in a fixed table, until traceEnd;
// traceSites resolves them (dbghelp, loaded then, as vr_crash's report does) to the first frame outside the allocators.

#include "vr_alloccount.hpp"

#include "Zancle/Algorithm/Sort.hpp"
#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/String/String.hpp"
#include "Zancle/String/StringView.hpp"

#include <new>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <dbghelp.h>
#endif

namespace
{

thread_local za::U64 allocations = 0;

// The trace (vr_alloc_sites): each distinct call stack once, with its count (open addressing on the stack's hash).
constexpr int traceDepth = 16;
constexpr za::SizeT traceSlots = 4096; // (a power of two)
struct TracedStack
{
    void* pc[traceDepth];
    za::U32 depth;
    za::U32 hash;
    za::U64 count; // 0: a free slot
};
TracedStack traceTable[traceSlots];
za::U64 traceTotal = 0, traceDropped = 0;
thread_local bool tracing = false; // this thread's allocations are traced

[[gnu::noinline]] void record()
{
#ifdef _WIN32
    void* pc[traceDepth];
    ULONG hash = 0;
    const za::U32 depth = RtlCaptureStackBackTrace(1, traceDepth, pc, &hash);
    traceTotal++;
    for(za::SizeT i = hash & (traceSlots - 1), probes = 0; probes < traceSlots; i = (i + 1) & (traceSlots - 1), probes++)
    {
        TracedStack& t = traceTable[i];
        if(t.count == 0)
        {
            memcpy(t.pc, pc, sizeof(void*) * depth);
            t.depth = depth;
            t.hash = hash;
            t.count = 1;
            return;
        }
        if(t.hash == hash && t.depth == depth && memcmp(t.pc, pc, sizeof(void*) * depth) == 0)
        {
            t.count++;
            return;
        }
    }
    traceDropped++;
#endif
}

[[nodiscard]] void* allocate(za::SizeT size)
{
    ++allocations;
    if(tracing)
    {
        record();
    }
    if(void* p = malloc(size ? size : 1))
    {
        return p;
    }
    // Out of memory: the game ends here, with the crash report (abort: vr_crash's SIGABRT handler). The engine is built
    // without exceptions, so there is no std::bad_alloc to throw (the C code's Sys_Error is not for a thread's new).
    fprintf(stderr, "operator new: out of memory (%zu bytes)\n", static_cast<size_t>(size));
    abort();
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

void traceBegin()
{
    memset(traceTable, 0, sizeof(traceTable));
    traceTotal = 0;
    traceDropped = 0;
    tracing = true;
}

void traceEnd()
{
    tracing = false;
}

namespace
{

// A frame of the allocators themselves (this file, Zancle's containers, the standard library, the scratch sets), not
// the site that asked.
[[nodiscard]] bool allocatorFrame(za::StringView name)
{
    constexpr const char* prefixes[] = {"operator new", "`anonymous namespace'::allocate", "`anonymous namespace'::record",
        "za::", "std::", "ankerl::", "malloc", "qvr::mem::"};
    for(const char* p : prefixes)
    {
        const za::SizeT n = strlen(p);
        if(name.size() >= n && memcmp(name.data(), p, n) == 0)
        {
            return true;
        }
    }
    return false;
}

} // namespace

za::Vector<Site> traceSites(za::U64& total, za::U64& dropped)
{
    total = traceTotal;
    dropped = traceDropped;
    za::Vector<Site> sites;
#ifdef _WIN32
    using SymInitialize_t = BOOL(WINAPI*)(HANDLE, PCSTR, BOOL);
    using SymCleanup_t = BOOL(WINAPI*)(HANDLE);
    using SymSetOptions_t = DWORD(WINAPI*)(DWORD);
    using SymFromAddr_t = BOOL(WINAPI*)(HANDLE, DWORD64, PDWORD64, PSYMBOL_INFO);
    using SymGetLineFromAddr64_t = BOOL(WINAPI*)(HANDLE, DWORD64, PDWORD, PIMAGEHLP_LINE64);
    HMODULE dbg = LoadLibraryA("dbghelp.dll");
    if(!dbg)
    {
        return sites;
    }
    const auto proc0 = [&](const char* name) { return reinterpret_cast<void*>(GetProcAddress(dbg, name)); };
    const auto symInitialize = reinterpret_cast<SymInitialize_t>(proc0("SymInitialize"));
    const auto symCleanup = reinterpret_cast<SymCleanup_t>(proc0("SymCleanup"));
    const auto symSetOptions = reinterpret_cast<SymSetOptions_t>(proc0("SymSetOptions"));
    const auto symFromAddr = reinterpret_cast<SymFromAddr_t>(proc0("SymFromAddr"));
    const auto symGetLine = reinterpret_cast<SymGetLineFromAddr64_t>(proc0("SymGetLineFromAddr64"));
    if(!symInitialize || !symFromAddr || !symSetOptions)
    {
        FreeLibrary(dbg);
        return sites;
    }
    const HANDLE proc = GetCurrentProcess();
    symSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES);
    symInitialize(proc, nullptr, TRUE);

    // A frame's function, and the function with its file (without the folders) and line.
    const auto describe = [&](void* pc, za::String& function, za::String& where) {
        alignas(SYMBOL_INFO) char buf[sizeof(SYMBOL_INFO) + 512] = {};
        auto* sym = reinterpret_cast<SYMBOL_INFO*>(buf);
        sym->SizeOfStruct = sizeof(SYMBOL_INFO);
        sym->MaxNameLen = 511;
        DWORD64 disp = 0;
        const auto addr = reinterpret_cast<DWORD64>(pc);
        function = symFromAddr(proc, addr, &disp, sym) ? za::String{sym->Name} : za::String{"?"};
        IMAGEHLP_LINE64 line{};
        line.SizeOfStruct = sizeof(line);
        DWORD ldisp = 0;
        where = function;
        if(symGetLine && symGetLine(proc, addr, &ldisp, &line))
        {
            const char* file = line.FileName;
            for(const char* c = line.FileName; *c; c++)
            {
                if(*c == '\\' || *c == '/')
                {
                    file = c + 1;
                }
            }
            char text[300];
            snprintf(text, sizeof(text), " (%s:%lu)", file, static_cast<unsigned long>(line.LineNumber));
            where += text;
        }
    };

    for(const TracedStack& t : traceTable)
    {
        if(t.count == 0)
        {
            continue;
        }
        za::String where = "(allocators only)", via;
        for(za::U32 f = 0; f < t.depth; f++)
        {
            za::String function, at;
            describe(t.pc[f], function, at);
            if(allocatorFrame(za::StringView{function}))
            {
                continue;
            }
            where = at;
            if(f + 1 < t.depth)
            {
                za::String caller;
                describe(t.pc[f + 1], caller, via);
            }
            break;
        }
        Site* found = nullptr;
        for(Site& s : sites)
        {
            if(s.where == where)
            {
                found = &s;
                break;
            }
        }
        if(!found)
        {
            sites.pushBack(Site{where, za::String{}, 0, 0});
            found = &sites.back();
        }
        found->count += t.count;
        if(t.count > found->viaCount) // (the caller of its commonest stack)
        {
            found->via = via;
            found->viaCount = t.count;
        }
    }
    if(symCleanup)
    {
        symCleanup(proc);
    }
    FreeLibrary(dbg);
    za::quickSort(sites.begin(), sites.end(), [](const Site& a, const Site& b) { return a.count > b.count; });
#endif
    return sites;
}

} // namespace qvr::alloccount
