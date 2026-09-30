// vr_steamaudio.cpp -- Steam Audio loaded at run time; see vr_steamaudio.hpp.

#include "vr_steamaudio.hpp"
#include "vr_engine.hpp"

#if defined(SDL_FRAMEWORK) || defined(NO_SDL_CONFIG)
#include <SDL2/SDL.h>
#else
#include "SDL.h"
#endif

#include <mutex>
#include <string>

namespace qvr::steamaudio
{

namespace
{

#if defined(_WIN32)
constexpr const char* libraryName = "phonon.dll";
#elif defined(__APPLE__)
constexpr const char* libraryName = "libphonon.dylib";
#else
constexpr const char* libraryName = "libphonon.so";
#endif

struct State
{
    bool tried{false};
    void* library{nullptr};
    Api table;
    bool ok{false};
    IPLContext context{nullptr};
    std::string status{"not tried"};
};
State state;

// Steam Audio's log (any thread): kept here, printed by flushLog on the main thread.
struct Log
{
    std::mutex mutex;
    std::string pending;
};
Log logged;

void IPLCALL logMessage(IPLLogLevel level, const char* message)
{
    if(level != IPL_LOGLEVEL_WARNING && level != IPL_LOGLEVEL_ERROR)
    {
        return;
    }
    const std::lock_guard lock{logged.mutex};
    if(logged.pending.size() < 4096)
    {
        logged.pending += level == IPL_LOGLEVEL_ERROR ? "Steam Audio error: " : "Steam Audio warning: ";
        logged.pending += message ? message : "";
        if(logged.pending.empty() || logged.pending.back() != '\n')
        {
            logged.pending += '\n';
        }
    }
}

void unload()
{
    if(state.context && state.table.iplContextRelease)
    {
        state.table.iplContextRelease(&state.context);
    }
    state.context = nullptr;
    if(state.library)
    {
        SDL_UnloadObject(state.library);
    }
    state.library = nullptr;
    state.table = Api{};
    state.ok = false;
}

void load()
{
    state.tried = true;
    state.library = SDL_LoadObject(libraryName);
    if(!state.library)
    {
        state.status = std::string{libraryName} + " not found: Quake's own panning";
        return;
    }
    bool missing = false;
    std::string missingName;
#define QVR_IPL_LOAD(name)                                                                                              \
    state.table.name = reinterpret_cast<decltype(state.table.name)>(SDL_LoadFunction(state.library, #name));           \
    if(!state.table.name && !missing)                                                                                   \
    {                                                                                                                   \
        missing = true;                                                                                                 \
        missingName = #name;                                                                                            \
    }
    QVR_IPL_FUNCTIONS(QVR_IPL_LOAD)
#undef QVR_IPL_LOAD
    if(missing)
    {
        state.status = std::string{libraryName} + " has no " + missingName + ": Quake's own panning";
        unload();
        return;
    }

    IPLContextSettings settings{};
    settings.version = STEAMAUDIO_VERSION;
    settings.logCallback = logMessage;
    settings.simdLevel = IPL_SIMDLEVEL_AVX2; // (AVX-512 can throttle the clock: Steam Audio's own advice)
    if(state.table.iplContextCreate(&settings, &state.context) != IPL_STATUS_SUCCESS || !state.context)
    {
        state.status = std::string{libraryName} + " refused the context (another version than " +
                       std::to_string(STEAMAUDIO_VERSION_MAJOR) + "." + std::to_string(STEAMAUDIO_VERSION_MINOR) +
                       "?): Quake's own panning";
        state.context = nullptr;
        unload();
        return;
    }
    state.ok = true;
    state.status = std::string{"loaded "} + libraryName + " (Steam Audio " + std::to_string(STEAMAUDIO_VERSION_MAJOR) +
                   "." + std::to_string(STEAMAUDIO_VERSION_MINOR) + "." + std::to_string(STEAMAUDIO_VERSION_PATCH) + ")";
}

} // namespace

const Api* api()
{
    if(!state.tried)
    {
        load();
        Con_DPrintf("Steam Audio: %s\n", state.status.c_str());
    }
    return state.ok ? &state.table : nullptr;
}

IPLContext context()
{
    return api() ? state.context : nullptr;
}

const char* status()
{
    return state.status.c_str();
}

void flushLog()
{
    std::string text;
    {
        const std::lock_guard lock{logged.mutex};
        if(logged.pending.empty())
        {
            return;
        }
        text.swap(logged.pending);
    }
    Con_Printf("%s", text.c_str());
}

void shutdown()
{
    flushLog();
    // The context released; the library stays loaded until the process ends (nothing needs it gone, and a library
    // with threads of its own is safest left to the process's exit).
    if(state.context && state.table.iplContextRelease)
    {
        state.table.iplContextRelease(&state.context);
    }
    state.context = nullptr;
    state.ok = false;
    state.status = "shut down";
}

} // namespace qvr::steamaudio
