// The process's C heap: mimalloc (external/mimalloc/README.md) in place of the C runtime's, for the engine and every
// library linked into the executable. malloc, calloc, realloc, free, _msize, _expand, _recalloc, the _aligned_* family,
// strdup and wcsdup are defined here; operator new and delete (vr_alloccount.cpp) call malloc/free and
// _aligned_malloc/_aligned_free, so they come here too.
//
// How (Windows, the DLL C runtime /MD and /MDd): the C runtime's headers declare its functions dllimport, so the code
// calls them through their import pointers (__imp_malloc, ...), which the import library (ucrt.lib) would bind to
// ucrtbase.dll. This file defines each of those pointers, pointing at its own function, and the function itself: the
// linker takes an object's definition over a library's, so the import is never made (check: `dumpbin /imports
// ironwail.exe` lists none of these). mimalloc's upstream overrides for Windows are either the redirection DLL
// (mimalloc-redirect.dll, a binary: patches ucrtbase for every module) or a static link with the static C runtime
// (/MT); the engine stays on /MD (the codec libraries, SDL2, libcurl, OpenXR and Steam Audio DLLs are built for it), so
// it does neither.
//
// What it does not cover: the DLLs (SDL2, libcurl, zlib, the OpenXR loader, Steam Audio) and the C runtime's own
// functions keep ucrtbase's heap. A block one of them allocates and the engine frees (_getcwd(NULL), _fullpath(NULL),
// ...) is not mimalloc's: free, realloc, _msize and the rest send any block mimalloc does not own (mi_is_in_heap_region,
// a page-map lookup) to the C runtime's own function, and count it (VR_CrtHeapForeignCalls, `vr_heap`). The other way,
// a mimalloc block freed by a DLL, would corrupt its heap: the engine gives none of them a block to free (audited: SDL
// memory is freed with SDL_free, curl's with its own calls, Steam Audio has no allocator callbacks set; ROUND21.md,
// "mimalloc").
//
// No C runtime header here: they declare these functions dllimport, which their definitions here could not be.
//
// A/B: the C runtime's heap at run time with -nomimalloc on the command line or QVR_MIMALLOC=0 in the environment
// (read at the first allocation, before main), every call then forwarded to ucrtbase; or a build without it
// (MSBuild's QVR_MIMALLOC=false, CMake's QVR_MIMALLOC=OFF: this file then only reports that).

#include "vr_crtheap.h"

#if defined(QVR_MIMALLOC) && defined(_WIN32)

#if !(defined(_M_X64) || defined(_M_ARM64))
#error "vr_crtheap.c: the import pointers are 64-bit Windows' (x86's are __imp__malloc): build without QVR_MIMALLOC"
#endif

#include "mimalloc.h"

// kernel32, declared here (no <windows.h>: it would bring the C runtime's headers in).
typedef void* QvrHmodule;
__declspec(dllimport) QvrHmodule __stdcall GetModuleHandleW(const wchar_t* name);
__declspec(dllimport) void* __stdcall GetProcAddress(QvrHmodule module, const char* name);
__declspec(dllimport) unsigned long __stdcall GetEnvironmentVariableW(const wchar_t* name, wchar_t* buffer, unsigned long size);
__declspec(dllimport) wchar_t* __stdcall GetCommandLineW(void);
__declspec(noreturn) void __fastfail(unsigned int code);

// ---- The C runtime's own functions: for blocks mimalloc does not own, and for everything when it is off ----------

typedef struct
{
    void* (*malloc)(size_t);
    void* (*calloc)(size_t, size_t);
    void* (*realloc)(void*, size_t);
    void (*free)(void*);
    size_t (*msize)(void*);
    void* (*expand)(void*, size_t);
    void* (*recalloc)(void*, size_t, size_t);
    void* (*alignedMalloc)(size_t, size_t);
    void* (*alignedOffsetMalloc)(size_t, size_t, size_t);
    void* (*alignedRealloc)(void*, size_t, size_t);
    void* (*alignedOffsetRealloc)(void*, size_t, size_t, size_t);
    void* (*alignedRecalloc)(void*, size_t, size_t, size_t);
    void* (*alignedOffsetRecalloc)(void*, size_t, size_t, size_t, size_t);
    size_t (*alignedMsize)(void*, size_t, size_t);
    void (*alignedFree)(void*);
    char* (*strdup)(const char*);
    wchar_t* (*wcsdup)(const wchar_t*);
} QvrCrtFns;

// 0: not yet decided (the first call decides), 1: mimalloc, 2: the C runtime's heap. Written once (each racing first
// call writes the same values), before any thread but the main one exists.
enum { modeUnset = 0, modeMimalloc = 1, modeCrt = 2 };
static int qvrHeapMode;
static QvrCrtFns qvrCrt;
static const char* qvrHeapOffReason = "";
static unsigned long long qvrForeignCalls;

static void* qvrCrtFn(QvrHmodule crt, const char* name)
{
    void* fn = GetProcAddress(crt, name);
    if(!fn)
    {
        __fastfail(7); // FAST_FAIL_FATAL_APP_EXIT: the C runtime lacks a heap function (cannot report it: no heap)
    }
    return fn;
}

// The command line has the word (whole, case as given).
static int qvrCommandLineHas(const wchar_t* word)
{
    const wchar_t* line = GetCommandLineW();
    for(const wchar_t* at = line; at && *at; at++)
    {
        if(at != line && at[-1] != L' ' && at[-1] != L'\t' && at[-1] != L'"')
        {
            continue;
        }
        int i = 0;
        while(word[i] && at[i] == word[i])
        {
            i++;
        }
        if(!word[i] && (at[i] == 0 || at[i] == L' ' || at[i] == L'\t' || at[i] == L'"'))
        {
            return 1;
        }
    }
    return 0;
}

static __declspec(noinline) int qvrHeapInit(void)
{
#if defined(_DEBUG)
    QvrHmodule crt = GetModuleHandleW(L"ucrtbased.dll");
#else
    QvrHmodule crt = GetModuleHandleW(L"ucrtbase.dll");
#endif
    if(!crt)
    {
        __fastfail(7);
    }
    QvrCrtFns f;
    f.malloc = (void* (*)(size_t))qvrCrtFn(crt, "malloc");
    f.calloc = (void* (*)(size_t, size_t))qvrCrtFn(crt, "calloc");
    f.realloc = (void* (*)(void*, size_t))qvrCrtFn(crt, "realloc");
    f.free = (void (*)(void*))qvrCrtFn(crt, "free");
    f.msize = (size_t (*)(void*))qvrCrtFn(crt, "_msize");
    f.expand = (void* (*)(void*, size_t))qvrCrtFn(crt, "_expand");
    f.recalloc = (void* (*)(void*, size_t, size_t))qvrCrtFn(crt, "_recalloc");
    f.alignedMalloc = (void* (*)(size_t, size_t))qvrCrtFn(crt, "_aligned_malloc");
    f.alignedOffsetMalloc = (void* (*)(size_t, size_t, size_t))qvrCrtFn(crt, "_aligned_offset_malloc");
    f.alignedRealloc = (void* (*)(void*, size_t, size_t))qvrCrtFn(crt, "_aligned_realloc");
    f.alignedOffsetRealloc = (void* (*)(void*, size_t, size_t, size_t))qvrCrtFn(crt, "_aligned_offset_realloc");
    f.alignedRecalloc = (void* (*)(void*, size_t, size_t, size_t))qvrCrtFn(crt, "_aligned_recalloc");
    f.alignedOffsetRecalloc = (void* (*)(void*, size_t, size_t, size_t, size_t))qvrCrtFn(crt, "_aligned_offset_recalloc");
    f.alignedMsize = (size_t (*)(void*, size_t, size_t))qvrCrtFn(crt, "_aligned_msize");
    f.alignedFree = (void (*)(void*))qvrCrtFn(crt, "_aligned_free");
    f.strdup = (char* (*)(const char*))qvrCrtFn(crt, "_strdup");
    f.wcsdup = (wchar_t* (*)(const wchar_t*))qvrCrtFn(crt, "_wcsdup");
    qvrCrt = f;

    int mode = modeMimalloc;
    wchar_t value[8];
    const unsigned long length = GetEnvironmentVariableW(L"QVR_MIMALLOC", value, 8);
    if(length == 1 && value[0] == L'0')
    {
        mode = modeCrt;
        qvrHeapOffReason = "QVR_MIMALLOC=0";
    }
    if(qvrCommandLineHas(L"-nomimalloc"))
    {
        mode = modeCrt;
        qvrHeapOffReason = "-nomimalloc";
    }
    __atomic_store_n(&qvrHeapMode, mode, __ATOMIC_RELEASE);
    return mode;
}

static inline int qvrMode(void)
{
    const int mode = __atomic_load_n(&qvrHeapMode, __ATOMIC_ACQUIRE);
    return __builtin_expect(mode != modeUnset, 1) ? mode : qvrHeapInit();
}

// mimalloc serves new blocks.
static inline int qvrUseMi(void)
{
    return qvrMode() == modeMimalloc;
}

// A block mimalloc does not own (the C runtime's, or everything with mimalloc off): counted, for the C runtime.
static inline int qvrForeign(const void* p)
{
    if(qvrMode() != modeMimalloc) // (the C runtime's functions resolved) off from the first call: every block its
    {
        return 1;
    }
    if(mi_is_in_heap_region(p))
    {
        return 0;
    }
    __atomic_fetch_add(&qvrForeignCalls, 1, __ATOMIC_RELAXED);
    return 1;
}

// ---- The overrides ------------------------------------------------------------------------------------------------

void* __cdecl malloc(size_t size)
{
    return qvrUseMi() ? mi_malloc(size) : qvrCrt.malloc(size);
}

void* __cdecl calloc(size_t count, size_t size)
{
    return qvrUseMi() ? mi_calloc(count, size) : qvrCrt.calloc(count, size);
}

void __cdecl free(void* p)
{
    if(!p)
    {
        return;
    }
    if(qvrForeign(p))
    {
        qvrCrt.free(p);
        return;
    }
    mi_free(p);
}

// realloc(p, 0) frees p and returns null (the C runtime's behaviour; mimalloc's would keep a minimal block).
void* __cdecl realloc(void* p, size_t size)
{
    if(!p)
    {
        return malloc(size);
    }
    if(qvrForeign(p))
    {
        return qvrCrt.realloc(p, size);
    }
    if(size == 0)
    {
        mi_free(p);
        return 0;
    }
    return mi_realloc(p, size);
}

size_t __cdecl _msize(void* p)
{
    return p && qvrForeign(p) ? qvrCrt.msize(p) : mi_usable_size(p);
}

void* __cdecl _expand(void* p, size_t size)
{
    return p && qvrForeign(p) ? qvrCrt.expand(p, size) : mi_expand(p, size);
}

void* __cdecl _recalloc(void* p, size_t count, size_t size)
{
    if(!p)
    {
        return calloc(count, size);
    }
    if(qvrForeign(p))
    {
        return qvrCrt.recalloc(p, count, size);
    }
    if(count == 0 || size == 0)
    {
        mi_free(p);
        return 0;
    }
    return mi_recalloc(p, count, size);
}

void* __cdecl _aligned_malloc(size_t size, size_t alignment)
{
    return qvrUseMi() ? mi_malloc_aligned(size, alignment) : qvrCrt.alignedMalloc(size, alignment);
}

void* __cdecl _aligned_offset_malloc(size_t size, size_t alignment, size_t offset)
{
    return qvrUseMi() ? mi_malloc_aligned_at(size, alignment, offset) : qvrCrt.alignedOffsetMalloc(size, alignment, offset);
}

void __cdecl _aligned_free(void* p)
{
    if(!p)
    {
        return;
    }
    if(qvrForeign(p))
    {
        qvrCrt.alignedFree(p);
        return;
    }
    mi_free(p);
}

void* __cdecl _aligned_realloc(void* p, size_t size, size_t alignment)
{
    if(!p)
    {
        return _aligned_malloc(size, alignment);
    }
    if(qvrForeign(p))
    {
        return qvrCrt.alignedRealloc(p, size, alignment);
    }
    if(size == 0)
    {
        mi_free(p);
        return 0;
    }
    return mi_realloc_aligned(p, size, alignment);
}

void* __cdecl _aligned_offset_realloc(void* p, size_t size, size_t alignment, size_t offset)
{
    if(!p)
    {
        return _aligned_offset_malloc(size, alignment, offset);
    }
    if(qvrForeign(p))
    {
        return qvrCrt.alignedOffsetRealloc(p, size, alignment, offset);
    }
    if(size == 0)
    {
        mi_free(p);
        return 0;
    }
    return mi_realloc_aligned_at(p, size, alignment, offset);
}

void* __cdecl _aligned_recalloc(void* p, size_t count, size_t size, size_t alignment)
{
    if(!p)
    {
        return qvrUseMi() ? mi_calloc_aligned(count, size, alignment) : qvrCrt.alignedRecalloc(0, count, size, alignment);
    }
    if(qvrForeign(p))
    {
        return qvrCrt.alignedRecalloc(p, count, size, alignment);
    }
    if(count == 0 || size == 0)
    {
        mi_free(p);
        return 0;
    }
    return mi_recalloc_aligned(p, count, size, alignment);
}

void* __cdecl _aligned_offset_recalloc(void* p, size_t count, size_t size, size_t alignment, size_t offset)
{
    if(!p)
    {
        return qvrUseMi() ? mi_calloc_aligned_at(count, size, alignment, offset)
                          : qvrCrt.alignedOffsetRecalloc(0, count, size, alignment, offset);
    }
    if(qvrForeign(p))
    {
        return qvrCrt.alignedOffsetRecalloc(p, count, size, alignment, offset);
    }
    if(count == 0 || size == 0)
    {
        mi_free(p);
        return 0;
    }
    return mi_recalloc_aligned_at(p, count, size, alignment, offset);
}

size_t __cdecl _aligned_msize(void* p, size_t alignment, size_t offset)
{
    return p && qvrForeign(p) ? qvrCrt.alignedMsize(p, alignment, offset) : mi_usable_size(p);
}

char* __cdecl _strdup(const char* s)
{
    return qvrUseMi() ? mi_strdup(s) : qvrCrt.strdup(s);
}

char* __cdecl strdup(const char* s)
{
    return _strdup(s);
}

wchar_t* __cdecl _wcsdup(const wchar_t* s)
{
    return qvrUseMi() ? mi_wcsdup(s) : qvrCrt.wcsdup(s);
}

wchar_t* __cdecl wcsdup(const wchar_t* s)
{
    return _wcsdup(s);
}

// The import pointers the dllimport calls go through (see the top). `used`: kept whatever the optimiser sees.
#define QVR_IMPORT(name) __attribute__((used)) void* const __imp_##name = (void*)&name;
QVR_IMPORT(malloc)
QVR_IMPORT(calloc)
QVR_IMPORT(realloc)
QVR_IMPORT(free)
QVR_IMPORT(_msize)
QVR_IMPORT(_expand)
QVR_IMPORT(_recalloc)
QVR_IMPORT(_aligned_malloc)
QVR_IMPORT(_aligned_offset_malloc)
QVR_IMPORT(_aligned_free)
QVR_IMPORT(_aligned_realloc)
QVR_IMPORT(_aligned_offset_realloc)
QVR_IMPORT(_aligned_recalloc)
QVR_IMPORT(_aligned_offset_recalloc)
QVR_IMPORT(_aligned_msize)
QVR_IMPORT(_strdup)
QVR_IMPORT(strdup)
QVR_IMPORT(_wcsdup)
QVR_IMPORT(wcsdup)
#undef QVR_IMPORT

// ---- What the console reports (vr_crtheap.h; the statistics: vr_crtheap_info.c) -----------------------------------

int VR_CrtHeapIsMimalloc(void)
{
    return qvrUseMi();
}

const char* VR_CrtHeapOffReason(void)
{
    qvrMode();
    return qvrHeapOffReason;
}

int VR_CrtHeapOwns(const void* p)
{
    return p && mi_is_in_heap_region(p);
}

unsigned long long VR_CrtHeapForeignCalls(void)
{
    return __atomic_load_n(&qvrForeignCalls, __ATOMIC_RELAXED);
}

#elif defined(QVR_MIMALLOC) // elsewhere: mimalloc's own override (MI_MALLOC_OVERRIDE, vr.cmake) defines malloc and the rest

#include "mimalloc.h"

int VR_CrtHeapIsMimalloc(void)
{
    return 1;
}

const char* VR_CrtHeapOffReason(void)
{
    return "";
}

int VR_CrtHeapOwns(const void* p)
{
    return p && mi_is_in_heap_region(p);
}

unsigned long long VR_CrtHeapForeignCalls(void)
{
    return 0;
}

#else // !QVR_MIMALLOC: the C runtime's heap, untouched

int VR_CrtHeapIsMimalloc(void)
{
    return 0;
}

const char* VR_CrtHeapOffReason(void)
{
    return "a build without mimalloc";
}

int VR_CrtHeapOwns(const void* p)
{
    (void)p;
    return 0;
}

unsigned long long VR_CrtHeapForeignCalls(void)
{
    return 0;
}

#endif
