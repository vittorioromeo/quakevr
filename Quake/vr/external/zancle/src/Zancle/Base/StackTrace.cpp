// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include "Zancle/Base/StackTrace.hpp"

#include "Zancle/Config.hpp" // IWYU pragma: keep

#ifdef ZA_ENABLE_STACK_TRACES

    #ifdef ZA_SYSTEM_EMSCRIPTEN
        #include <emscripten.h>
    #else

        #include "Zancle/Base/ScopeGuard.hpp"
        #include "Zancle/Base/UIntPtrT.hpp"

        #ifdef ZA_SYSTEM_WINDOWS
            #include "Zancle/Base/WindowsHeader.hpp"
        #endif

        #include <backtrace.h>

        #include <cstdlib>
        #include <cxxabi.h>

        #ifdef ZA_SYSTEM_WINDOWS
            #include <io.h>

            #ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
                #define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x00'04
            #endif
        #else
            #include <unistd.h>
        #endif

    #endif

    #include <cstdio>

#endif


#ifdef ZA_ENABLE_STACK_TRACES
    #ifndef ZA_SYSTEM_EMSCRIPTEN
namespace
{
////////////////////////////////////////////////////////////
// ANSI color codes; empty strings when stderr is not a TTY or NO_COLOR is set.
struct Palette
{
    const char* reset;
    const char* cyan;   // function name
    const char* yellow; // program counter
    const char* grey;   // file:line
};


////////////////////////////////////////////////////////////
bool stderrIsTty()
{
        #ifdef ZA_SYSTEM_WINDOWS
    return _isatty(_fileno(stderr)) != 0;
        #else
    return isatty(fileno(stderr)) != 0;
        #endif
}


////////////////////////////////////////////////////////////
bool detectColorSupport()
{
    if (std::getenv("NO_COLOR") != nullptr)
        return false;

    if (!stderrIsTty())
        return false;

        #ifdef ZA_SYSTEM_WINDOWS
    const HANDLE h = GetStdHandle(STD_ERROR_HANDLE);
    if (h == INVALID_HANDLE_VALUE)
        return false;

    DWORD mode = 0;
    if (!GetConsoleMode(h, &mode))
        return false;

    return SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0;
        #else
    return true;
        #endif
}


////////////////////////////////////////////////////////////
void errorCallback(void* /* data */, const char* msg, int errnum)
{
    std::fprintf(stderr, "  <stack trace error: %s", msg); // NOLINT(modernize-use-std-print)

    if (errnum > 0)
        std::fprintf(stderr, " (errno %d)", errnum); // NOLINT(modernize-use-std-print)

    std::fprintf(stderr, ">\n"); // NOLINT(modernize-use-std-print)
}


////////////////////////////////////////////////////////////
backtrace_state* getState()
{
    static backtrace_state* const state = backtrace_create_state(
        /* filename */ nullptr,
        /* threaded */ 1,
        /* error_cb */ &errorCallback,
        /* data     */ nullptr);

    return state;
}


////////////////////////////////////////////////////////////
const Palette& getPalette()
{
    static const Palette palette = detectColorSupport() ? Palette{"\x1b[0m", "\x1b[36m", "\x1b[33m", "\x1b[90m"}
                                                        : Palette{"", "", "", ""};
    return palette;
}


////////////////////////////////////////////////////////////
void syminfoCallback(void* data, za::UIntPtrT /* pc */, const char* symname, za::UIntPtrT /* symval */, za::UIntPtrT /* symsize */)
{
    *static_cast<const char**>(data) = symname;
}


////////////////////////////////////////////////////////////
int frameCallback(void* data, za::UIntPtrT pc, const char* filename, int lineno, const char* function)
{
    int& index = *static_cast<int*>(data);

    // If DWARF had nothing for this PC, fall back to the ELF dynamic symbol
    // table. This recovers names for libc/ld.so/_start frames.
    const char* symtabName = nullptr;
    if (function == nullptr)
        backtrace_syminfo(getState(), pc, &syminfoCallback, &errorCallback, static_cast<void*>(&symtabName));

    const char* const rawName   = function ? function : symtabName;
    char* const       demangled = rawName ? abi::__cxa_demangle(rawName, nullptr, nullptr, nullptr) : nullptr;
    ZA_SCOPE_GUARD({ std::free(demangled); }); // NOLINT(cppcoreguidelines-no-malloc, hicpp-no-malloc)

    const char* displayName = demangled ? demangled : rawName;
    if (displayName == nullptr)
        displayName = "??";

    const Palette& palette = getPalette();

    // NOLINTNEXTLINE(modernize-use-std-print)
    std::fprintf(stderr,
                 "#%-2d %s0x%016llx%s  %s%s%s\n      %sat %s:%d%s\n",
                 index++,
                 palette.yellow,
                 static_cast<unsigned long long>(pc),
                 palette.reset,
                 palette.cyan,
                 displayName,
                 palette.reset,
                 palette.grey,
                 filename ? filename : "??",
                 lineno,
                 palette.reset);

    return 0;
}
} // namespace
    #endif
#endif


namespace za::priv
{
////////////////////////////////////////////////////////////
void printStackTrace()
{
#ifdef ZA_ENABLE_STACK_TRACES
    std::fputs("\n", stderr);

    #ifdef ZA_SYSTEM_EMSCRIPTEN

    EM_ASM(console.error("printStackTrace was called"); console.trace(););

    char callstack[4096];
    emscripten_get_callstack(EM_LOG_NO_PATHS | EM_LOG_JS_STACK, callstack, sizeof(callstack));
    std::fprintf(stderr, "%s\n", callstack); // NOLINT(modernize-use-std-print)

    #else

    backtrace_state* const state = getState();
    if (state == nullptr)
    {
        std::fputs("  <stack trace unavailable>\n", stderr);
        return;
    }

    int index = 0;
    backtrace_full(state, /* skip */ 1, &frameCallback, &errorCallback, &index);

    #endif

#endif
}

} // namespace za::priv
