// Per-thread C++ and engine-owned C heap counters; stack capture is opt-in and allocation-free.
// Requested bytes measure request traffic, not live heap size. External DLL heaps are outside coverage.

#include "vr_alloccount.hpp"
#include "vr_alloccount.h"
#include "vr_crtheap.h"

#include "Zancle/Algorithm/Sort.hpp"
#include "Zancle/Base/FwdStdNew.hpp" // operator new/delete's std::align_val_t, std::nothrow_t (not <new>)
#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Base/Memcmp.hpp"
#include "Zancle/Base/Memcpy.hpp"
#include "Zancle/Base/Memset.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Base/Strlen.hpp"
#include "Zancle/Concurrency/AtomicMutex.hpp"
#include "Zancle/Concurrency/LockGuard.hpp"
#include "Zancle/Container/AnkerlUnorderedDense.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/String/String.hpp"
#include "Zancle/String/StringView.hpp"

#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
#include <malloc.h>
#endif

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <dbghelp.h>
#endif

namespace
{

thread_local qvr::alloccount::Stats counters{};

// The trace (vr_alloc_sites): each distinct call stack once, with its count (open addressing on the stack's hash).
constexpr int traceDepth = 16;
constexpr za::SizeT traceSlots = 4096; // (a power of two)
struct TracedStack
{
    void* pc[traceDepth];
    za::U32 depth;
    za::U32 hash;
    za::U64 count; // 0: a free slot
    qvr::alloccount::Kind kind;
    za::U64 bytes;
    za::U64 frameCount, frameBytes, peakCount, peakBytes;
};
TracedStack traceTable[traceSlots];
za::U64 traceTotal = 0, traceDropped = 0;
thread_local bool tracing = false; // this thread's allocations are traced

[[gnu::noinline]] void record(qvr::alloccount::Kind kind, za::SizeT bytes)
{
    traceTotal++;
#ifdef _WIN32
    void* pc[traceDepth];
    ULONG hash = 0;
    const za::U32 depth = RtlCaptureStackBackTrace(1, traceDepth, pc, &hash);
    hash ^= static_cast<ULONG>(kind) * 0x9e3779b9u;
    for(za::SizeT i = hash & (traceSlots - 1), probes = 0; probes < traceSlots; i = (i + 1) & (traceSlots - 1), probes++)
    {
        TracedStack& t = traceTable[i];
        if(t.count == 0)
        {
            ZA_MEMCPY(t.pc, pc, sizeof(void*) * depth);
            t.depth = depth;
            t.hash = hash;
            t.count = 1;
            t.kind = kind;
            t.bytes = bytes;
            t.frameCount = 1;
            t.frameBytes = bytes;
            return;
        }
        if(t.kind == kind && t.hash == hash && t.depth == depth && ZA_MEMCMP(t.pc, pc, sizeof(void*) * depth) == 0)
        {
            t.count++;
            t.bytes += bytes;
            ++t.frameCount;
            t.frameBytes += bytes;
            return;
        }
    }
    traceDropped++;
#else
    (void)kind;
    (void)bytes;
#endif
}

void account(qvr::alloccount::Kind kind, za::SizeT size)
{
    ++counters.calls[static_cast<int>(kind)];
    counters.requestedBytes += size;
    if(tracing) record(kind, size);
}

void deallocate(void* p)
{
    if(p) account(qvr::alloccount::Kind::Delete, 0);
    free(p);
}

[[nodiscard]] void* allocate(za::SizeT size)
{
    account(qvr::alloccount::Kind::New, size);
    if(void* p = malloc(size ? size : 1))
    {
        return p;
    }
    // Out of memory: the game ends here, with the crash report (abort: vr_crash's SIGABRT handler). The engine is built
    // without exceptions, so there is no std::bad_alloc to throw (the C code's Sys_Error is not for a thread's new).
    fprintf(stderr, "operator new: out of memory (%zu bytes)\n", static_cast<za::SizeT>(size));
    abort();
}

} // namespace

// C++ allocation uses raw C heap storage (malloc/free: mimalloc, vr_crtheap.c) to avoid counting the same request twice.
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
    deallocate(p);
}

void operator delete[](void* p) noexcept
{
    deallocate(p);
}

void operator delete(void* p, za::SizeT) noexcept
{
    deallocate(p);
}

void operator delete[](void* p, za::SizeT) noexcept
{
    deallocate(p);
}

namespace
{
void* alignedAllocate(za::SizeT size, za::SizeT alignment)
{
#ifdef _WIN32
    return _aligned_malloc(size ? size : 1, alignment);
#else
    // posix_memalign refuses an alignment under a pointer's (EINVAL): libraries ask for 1, 2 or 4 (Mesa's LLVM does,
    // compiling the shaders), which aligned operator new must accept, as libstdc++'s own does.
    void* p = nullptr;
    alignment = alignment < sizeof(void*) ? sizeof(void*) : alignment;
    return posix_memalign(&p, alignment, size ? size : 1) == 0 ? p : nullptr;
#endif
}
void alignedRelease(void* p)
{
#ifdef _WIN32
    _aligned_free(p);
#else
    free(p);
#endif
}
}
void* operator new(za::SizeT size, za::AlignValT alignment)
{
    account(qvr::alloccount::Kind::New, size);
    if(void* p = alignedAllocate(size, static_cast<za::SizeT>(alignment))) return p;
    fprintf(stderr, "aligned operator new: out of memory\n");
    abort();
}
void* operator new[](za::SizeT size, za::AlignValT a) { return ::operator new(size, a); }
// The nothrow forms return null when out of memory (the library's would call the throwing ones above, which abort: there
// are no exceptions to catch).
void* operator new(za::SizeT size, const za::NothrowT&) noexcept
{
    account(qvr::alloccount::Kind::New, size);
    return malloc(size ? size : 1);
}
void* operator new[](za::SizeT size, const za::NothrowT& n) noexcept { return ::operator new(size, n); }
void* operator new(za::SizeT size, za::AlignValT alignment, const za::NothrowT&) noexcept
{
    account(qvr::alloccount::Kind::New, size);
    return alignedAllocate(size, static_cast<za::SizeT>(alignment));
}
void* operator new[](za::SizeT size, za::AlignValT a, const za::NothrowT& n) noexcept
{
    return ::operator new(size, a, n);
}
void operator delete(void* p, const za::NothrowT&) noexcept { ::operator delete(p); }
void operator delete[](void* p, const za::NothrowT&) noexcept { ::operator delete(p); }
void operator delete(void* p, za::AlignValT a, const za::NothrowT&) noexcept { ::operator delete(p, a); }
void operator delete[](void* p, za::AlignValT a, const za::NothrowT&) noexcept { ::operator delete(p, a); }
void operator delete(void* p, za::AlignValT) noexcept
{
    if(p) account(qvr::alloccount::Kind::Delete, 0);
    alignedRelease(p);
}
void operator delete[](void* p, za::AlignValT a) noexcept { ::operator delete(p, a); }
void operator delete(void* p, za::SizeT, za::AlignValT a) noexcept { ::operator delete(p, a); }
void operator delete[](void* p, za::SizeT, za::AlignValT a) noexcept { ::operator delete(p, a); }
extern "C" void* VR_HeapAlignedAlloc(za::SizeT size, za::SizeT alignment)
{
    account(qvr::alloccount::Kind::Malloc, size);
    return alignedAllocate(size, alignment);
}
extern "C" void VR_HeapAlignedFree(void* p)
{
    if(p) account(qvr::alloccount::Kind::Free, 0);
    alignedRelease(p);
}

extern "C" void* VR_HeapMalloc(za::SizeT size)
{
    account(qvr::alloccount::Kind::Malloc, size);
    return malloc(size);
}
extern "C" void* VR_HeapCalloc(za::SizeT count, za::SizeT size)
{
    // Do not overflow the accounting multiplication or change CRT failure semantics.
    const za::SizeT bytes = size && count > static_cast<za::SizeT>(-1) / size ? 0 : count * size;
    account(qvr::alloccount::Kind::Calloc, bytes);
    return calloc(count, size);
}
extern "C" void* VR_HeapRealloc(void* p, za::SizeT size)
{
    // realloc(NULL, n) allocates and realloc(p, 0) frees: counted as such, so that requests and frees balance.
    account(!p ? qvr::alloccount::Kind::Malloc : size == 0 ? qvr::alloccount::Kind::Free : qvr::alloccount::Kind::Realloc, size);
    return realloc(p, size);
}
extern "C" char* VR_HeapStrdup(const char* s)
{
    const za::SizeT n = ZA_STRLEN(s) + 1;
    account(qvr::alloccount::Kind::Malloc, n);
    char* copy = static_cast<char*>(malloc(n));
    if(copy) ZA_MEMCPY(copy, s, n);
    return copy;
}
extern "C" void VR_HeapFree(void* p)
{
    if(p) account(qvr::alloccount::Kind::Free, 0);
    free(p);
}

namespace qvr::alloccount
{

za::U64 thisThread()
{
    return counters.calls[static_cast<int>(Kind::New)];
}

Stats statsThisThread() { return counters; }
const char* kindName(Kind kind)
{
    constexpr const char* names[] = {"new", "delete", "malloc", "calloc", "realloc", "free"};
    return names[static_cast<int>(kind)];
}

void traceBegin()
{
    ZA_MEMSET(traceTable, 0, sizeof(traceTable));
    traceTotal = 0;
    traceDropped = 0;
    tracing = true;
}

void traceEnd()
{
    tracing = false;
}

void traceFrameEnd(bool retainPeak)
{
    for(TracedStack& t : traceTable)
    {
        if(retainPeak)
        {
            // The peak frame is chosen by its allocations (vr_allocsites frameEnd): its sites are the allocating ones,
            // not the frees and deletes traced in it too.
            const bool frees = t.kind == Kind::Free || t.kind == Kind::Delete;
            t.peakCount = frees ? 0 : t.frameCount;
            t.peakBytes = frees ? 0 : t.frameBytes;
        }
        t.frameCount = t.frameBytes = 0;
    }
}

namespace
{

// A frame of the allocators themselves (this file, Zancle's containers, the standard library, the scratch sets), not
// the site that asked.
[[nodiscard]] bool allocatorFrame(za::StringView name)
{
    constexpr const char* prefixes[] = {"operator new", "operator delete", "VR_Heap",
        "`anonymous namespace'::heapAllocate", "`anonymous namespace'::heapFree", "b3Alloc", "b3Free",
        "`anonymous namespace'::account", "`anonymous namespace'::deallocate", "`anonymous namespace'::allocate",
        "`anonymous namespace'::record", "`anonymous namespace'::live", "qvrTraced",
        "za::", "std::", "ankerl::", "malloc", "qvr::mem::"};
    for(const char* p : prefixes)
    {
        const za::SizeT n = ZA_STRLEN(p);
        if(name.size() >= n && ZA_MEMCMP(name.data(), p, n) == 0)
        {
            return true;
        }
    }
    return false;
}

} // namespace

// ---- The live-block trace (vr_heap_sites) ----------------------------------------------------------------------------
// Every thread's blocks of a size range, from liveBegin to liveEnd, each kept by its address with its stack's place in
// traceTable until it is freed: traceTable's count and bytes are each stack's blocks still live (peakCount marks a slot
// taken; the table is vr_alloc_sites', the two never run together). Allocation-free (it runs inside malloc): the
// address table is VirtualAlloc's, made once and kept.
namespace
{

struct LiveBlock
{
    void* p; // null: free (linear probing, deletion by backward shift)
    za::U32 stack;
    za::U64 size;
};
constexpr za::SizeT liveSlots = za::SizeT{1} << 21; // (a power of two)
LiveBlock* liveTable = nullptr;
za::SizeT liveUsed = 0;
za::U64 liveMin = 0, liveMax = 0, liveDropped = 0, liveAllocs = 0;
bool liveOn = false;
za::AtomicMutex liveLock;

[[nodiscard]] za::SizeT liveHash(const void* p)
{
    return static_cast<za::SizeT>((reinterpret_cast<za::U64>(p) >> 4) * 0x9e3779b97f4a7c15ull >> 40) & (liveSlots - 1);
}

[[nodiscard]] za::I32 liveStack()
{
#ifdef _WIN32
    void* pc[traceDepth];
    ULONG hash = 0;
    const za::U32 depth = RtlCaptureStackBackTrace(2, traceDepth, pc, &hash); // (past liveStack and liveHook)
    for(za::SizeT i = hash & (traceSlots - 1), probes = 0; probes < traceSlots; i = (i + 1) & (traceSlots - 1), probes++)
    {
        TracedStack& t = traceTable[i];
        if(t.peakCount == 0)
        {
            ZA_MEMCPY(t.pc, pc, sizeof(void*) * depth);
            t.depth = depth;
            t.hash = hash;
            t.kind = qvr::alloccount::Kind::Malloc;
            t.peakCount = 1;
            return static_cast<za::I32>(i);
        }
        if(t.hash == hash && t.depth == depth && ZA_MEMCMP(t.pc, pc, sizeof(void*) * depth) == 0)
        {
            return static_cast<za::I32>(i);
        }
    }
#endif
    return -1;
}

void liveForget(void* p)
{
    za::SizeT i = liveHash(p);
    while(liveTable[i].p && liveTable[i].p != p)
    {
        i = (i + 1) & (liveSlots - 1);
    }
    if(!liveTable[i].p)
    {
        return; // (not traced)
    }
    TracedStack& t = traceTable[liveTable[i].stack];
    t.count--;
    t.bytes -= liveTable[i].size;
    --liveUsed;
    // backward shift: the entries after it that probed past it move up
    for(za::SizeT j = (i + 1) & (liveSlots - 1); liveTable[j].p; j = (j + 1) & (liveSlots - 1))
    {
        const za::SizeT home = liveHash(liveTable[j].p);
        if(((j - home) & (liveSlots - 1)) >= ((j - i) & (liveSlots - 1)))
        {
            liveTable[i] = liveTable[j];
            i = j;
        }
    }
    liveTable[i].p = nullptr;
}

void liveHook(void* p, za::SizeT size)
{
    za::LockGuard guard{liveLock};
    if(!liveOn)
    {
        return;
    }
    if(size == 0)
    {
        liveForget(p);
        return;
    }
    if(size < liveMin || size > liveMax)
    {
        return; // (a new block: never in the table, its address's last block was forgotten as it was freed)
    }
    ++liveAllocs;
    const za::I32 s = liveStack();
    if(s < 0 || liveUsed * 4 >= liveSlots * 3)
    {
        ++liveDropped;
        return;
    }
    za::SizeT i = liveHash(p);
    while(liveTable[i].p && liveTable[i].p != p)
    {
        i = (i + 1) & (liveSlots - 1);
    }
    if(!liveTable[i].p)
    {
        ++liveUsed;
    }
    liveTable[i] = LiveBlock{p, static_cast<za::U32>(s), size};
    traceTable[s].count++;
    traceTable[s].bytes += size;
}

} // namespace

bool liveBegin(za::U64 minSize, za::U64 maxSize)
{
#ifdef _WIN32
    if(!liveTable)
    {
        liveTable = static_cast<LiveBlock*>(VirtualAlloc(nullptr, liveSlots * sizeof(LiveBlock), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        if(!liveTable) return false;
    }
    {
        za::LockGuard guard{liveLock};
        ZA_MEMSET(traceTable, 0, sizeof(traceTable));
        ZA_MEMSET(liveTable, 0, liveSlots * sizeof(LiveBlock));
        traceTotal = traceDropped = 0;
        liveUsed = 0;
        liveDropped = liveAllocs = 0;
        liveMin = minSize;
        liveMax = maxSize;
        liveOn = true;
    }
    VR_CrtHeapSetTrace(&liveHook);
    return true;
#else
    (void)minSize;
    (void)maxSize;
    return false;
#endif
}

void liveEnd(za::U64& allocs, za::U64& live, za::U64& dropped)
{
    VR_CrtHeapSetTrace(nullptr);
    za::LockGuard guard{liveLock};
    liveOn = false;
    allocs = liveAllocs;
    live = liveUsed;
    dropped = liveDropped;
}

bool liveTracing()
{
    return liveOn;
}

za::Vector<Site> traceSites(za::U64& total, za::U64& dropped, bool peakFrame)
{
    total = traceTotal;
    if(peakFrame)
    {
        total = 0;
        for(const auto& t : traceTable) total += t.peakCount;
    }
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
        const za::U64 count = peakFrame ? t.peakCount : t.count;
        const za::U64 bytes = peakFrame ? t.peakBytes : t.bytes;
        if(count == 0)
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
            if(s.kind == t.kind && s.where == where)
            {
                found = &s;
                break;
            }
        }
        if(!found)
        {
            sites.pushBack(Site{where, za::String{}, 0, 0, t.kind, 0});
            found = &sites.back();
        }
        found->count += count;
        found->bytes += bytes;
        if(count > found->viaCount) // (the caller of its commonest stack)
        {
            found->via = via;
            found->viaCount = count;
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

// The page guard (zone.c's -memguard): a block on pages of its own, ending at its last committed page's end (the next
// page reserved, never committed: an overrun faults); its free decommits the pages and keeps them reserved, so the
// address is never handed out again and any later read or write through a kept pointer faults (a crash report with the
// stack of the use). Elsewhere than Windows: plain calloc/free (no guard).
extern "C" void* VR_PageGuardAlloc(za::SizeT size)
{
#ifdef _WIN32
    const za::SizeT page = 4096;
    const za::SizeT span = (size + page - 1) / page * page;
    const za::SizeT lead = span - ((size + 15) & ~static_cast<za::SizeT>(15)); // 16-byte aligned, ending at most 15 bytes short
    unsigned char* base = static_cast<unsigned char*>(VirtualAlloc(nullptr, span + page, MEM_RESERVE, PAGE_NOACCESS));
    if(!base || !VirtualAlloc(base, span, MEM_COMMIT, PAGE_READWRITE)) return nullptr;
    return base + lead; // committed pages are zeroed
#else
    return calloc(1, size ? size : 1);
#endif
}

extern "C" void VR_PageGuardFree(void* pointer, za::SizeT size)
{
    if(!pointer) return;
#ifdef _WIN32
    const za::SizeT page = 4096;
    const za::SizeT span = (size + page - 1) / page * page;
    const za::SizeT lead = span - ((size + 15) & ~static_cast<za::SizeT>(15)); // 16-byte aligned, ending at most 15 bytes short
    unsigned char* base = static_cast<unsigned char*>(pointer) - lead;
    VirtualFree(base, span, MEM_DECOMMIT);
#else
    (void)size;
    free(pointer);
#endif
}
