// vr_xr_runtime.cpp -- which OpenXR runtime the OpenXR backend loads, and the order it tries the others in
// (vr_xr_runtime.hpp).

#include "vr_xr_runtime.hpp"

#include "vr_cvars.hpp"
#include "vr_engine.hpp"

#ifdef _WIN32
#include <windows.h>
#include <tlhelp32.h>
#endif

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

namespace qvr::xrruntime
{
namespace
{

enum class Kind
{
    VirtualDesktop,
    SteamVR,
    Meta,
    Other,
};

// The runtimes the game knows: their manifests' file names, the apps that run them, where they usually are.
struct Known
{
    Kind kind;
    const char* label;
    const char* manifestName;
    const char* processes[2]; // (lower case)
    const char* reason;       // why, when one of them runs
    const char* usualPlace;
};

constexpr Known known[] = {
    {Kind::VirtualDesktop, "Virtual Desktop (VDXR)", "virtualdesktop-openxr.json", {"virtualdesktop.streamer.exe", nullptr},
        "Streamer running", "C:\\Program Files\\Virtual Desktop Streamer\\OpenXR\\virtualdesktop-openxr.json"},
    {Kind::SteamVR, "SteamVR", "steamxr_win64.json", {"vrserver.exe", "vrmonitor.exe"}, "running",
        "C:\\Program Files (x86)\\Steam\\steamapps\\common\\SteamVR\\steamxr_win64.json"},
    {Kind::Meta, "Meta (Oculus)", "oculus_openxr_64.json", {"ovrserver_x64.exe", nullptr}, "OVRServer running",
        "C:\\Program Files\\Oculus\\Support\\oculus-runtime\\oculus_openxr_64.json"},
};

[[nodiscard]] const Known* knownOf(Kind kind)
{
    for(const Known& k : known)
    {
        if(k.kind == kind)
        {
            return &k;
        }
    }
    return nullptr;
}

[[nodiscard]] const char* fileName(const char* path)
{
    const char* name = path;
    for(const char* p = path; *p; p++)
    {
        if(*p == '\\' || *p == '/')
        {
            name = p + 1;
        }
    }
    return name;
}

[[nodiscard]] Kind kindOf(const char* manifest)
{
    const char* name = fileName(manifest);
    for(const Known& k : known)
    {
        if(!q_strcasecmp(name, k.manifestName))
        {
            return k.kind;
        }
    }
    return Kind::Other;
}

[[nodiscard]] za::String labelOf(const char* manifest)
{
    const Known* k = knownOf(kindOf(manifest));
    return za::String{k ? k->label : fileName(manifest)};
}

[[nodiscard]] bool sameFile(const char* a, const char* b)
{
    for(; *a && *b; a++, b++)
    {
        const char x = *a == '/' ? '\\' : static_cast<char>(tolower(static_cast<unsigned char>(*a)));
        const char y = *b == '/' ? '\\' : static_cast<char>(tolower(static_cast<unsigned char>(*b)));
        if(x != y)
        {
            return false;
        }
    }
    return *a == *b;
}

[[nodiscard]] bool fileExists(const char* path)
{
#ifdef _WIN32
    const DWORD attributes = GetFileAttributesA(path);
    return attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY);
#else
    (void)path;
    return false;
#endif
}

// The items of a list cvar, split at commas and semicolons (spaces round them dropped).
[[nodiscard]] za::Vector<za::String> splitList(const char* text)
{
    za::Vector<za::String> items;
    za::String item;
    for(const char* p = text;; p++)
    {
        if(*p && *p != ',' && *p != ';')
        {
            if(!item.empty() || *p != ' ')
            {
                item += *p;
            }
            continue;
        }
        while(!item.empty() && item.back() == ' ')
        {
            item.popBack();
        }
        if(!item.empty())
        {
            items.pushBack(item);
        }
        item.clear();
        if(!*p)
        {
            break;
        }
    }
    return items;
}

// Virtual Desktop's own OpenXR runtime setting (the Streamer's Options: Automatic, SteamVR or VDXR): "OpenXRRuntime" in
// %ProgramData%\Virtual Desktop\StreamerSettings.json, the Streamer's enum VirtualDesktop.Interfaces.OpenXRRuntime
// (0 Automatic, 1 SteamVR, 2 VDXR; read from the Streamer 1.34's .NET metadata). The Streamer hands it to Virtual
// Desktop's service (SetOpenXRRuntime), which likely sets the system's ActiveRuntime to match (VDXR's there with VDXR
// chosen); that isn't verified for SteamVR, and the file says it directly anyway.
enum class VdChoice
{
    Unknown,
    Automatic,
    SteamVR,
    VDXR,
};

struct VdSetting
{
    VdChoice choice = VdChoice::Unknown;
    za::String source; // where it was read, or why it wasn't
};

[[nodiscard]] const char* vdChoiceName(VdChoice choice)
{
    switch(choice)
    {
        case VdChoice::Automatic: return "Automatic";
        case VdChoice::SteamVR: return "SteamVR";
        case VdChoice::VDXR: return "VDXR";
        default: return "unknown";
    }
}

[[nodiscard]] VdChoice vdChoiceOf(long value)
{
    switch(value)
    {
        case 0: return VdChoice::Automatic;
        case 1: return VdChoice::SteamVR;
        case 2: return VdChoice::VDXR;
        default: return VdChoice::Unknown;
    }
}

// "OpenXRRuntime" in a StreamerSettings.json: a number (the enum), or its name (should a later Streamer write names).
[[nodiscard]] VdSetting readVdSettings(const char* path)
{
    VdSetting vd;
    FILE* f = fopen(path, "rb");
    if(!f)
    {
        vd.source = va("%s: not found", path);
        return vd;
    }
    za::String text;
    char buffer[4096];
    for(size_t n; (n = fread(buffer, 1, sizeof(buffer), f)) > 0 && text.size() < 1024 * 1024;)
    {
        for(size_t i = 0; i < n; i++)
        {
            text += buffer[i];
        }
    }
    fclose(f);
    const char* key = q_strcasestr(text.cStr(), "\"OpenXRRuntime\"");
    if(!key)
    {
        vd.source = va("%s: no OpenXRRuntime in it", path);
        return vd;
    }
    const char* p = key + strlen("\"OpenXRRuntime\"");
    while(*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n' || *p == ':')
    {
        p++;
    }
    if(*p == '"')
    {
        constexpr VdChoice named[] = {VdChoice::Automatic, VdChoice::SteamVR, VdChoice::VDXR};
        for(VdChoice c : named)
        {
            const char* name = vdChoiceName(c);
            if(!q_strncasecmp(p + 1, name, strlen(name)) && p[1 + strlen(name)] == '"')
            {
                vd.choice = c;
            }
        }
    }
    else if(isdigit(static_cast<unsigned char>(*p)) || *p == '-')
    {
        vd.choice = vdChoiceOf(strtol(p, nullptr, 10));
    }
    char value[32];
    size_t n = 0;
    for(; p[n] && p[n] != ',' && p[n] != '\r' && p[n] != '\n' && p[n] != '}' && n + 1 < sizeof(value); n++)
    {
        value[n] = p[n];
    }
    value[n] = 0;
    vd.source = va("OpenXRRuntime %s in %s", value, path);
    return vd;
}

// Virtual Desktop's setting: vr_xr_test_vd_runtime's override (a number: -1 unknown, 0 Automatic, 1 SteamVR, 2 VDXR; or
// a StreamerSettings.json to read), else the Streamer's file (none in the test environment).
[[nodiscard]] VdSetting vdSetting(bool test)
{
    const char* o = vr_xr_test_vd_runtime.string;
    if(o[0] && (isdigit(static_cast<unsigned char>(o[0])) || o[0] == '-'))
    {
        return VdSetting{vdChoiceOf(strtol(o, nullptr, 10)), za::String{va("vr_xr_test_vd_runtime %s", o)}};
    }
    if(o[0])
    {
        return readVdSettings(o);
    }
    if(test)
    {
        return VdSetting{VdChoice::Unknown, za::String{"not read (test environment; vr_xr_test_vd_runtime)"}};
    }
#ifdef _WIN32
    const char* programData = getenv("ProgramData");
    return readVdSettings(
        va("%s\\Virtual Desktop\\StreamerSettings.json", programData && programData[0] ? programData : "C:\\ProgramData"));
#else
    return VdSetting{VdChoice::Unknown, za::String{"not read (Windows only)"}};
#endif
}

// What the system has: the installed runtimes (enabled ones), the active one, the running processes (lower case),
// Virtual Desktop's runtime setting. The real ones, or vr_xr_test_*'s (vr_xr_test 1: nothing real is read, nor loaded;
// no usual places).
struct Environment
{
    bool test = false;
    za::Vector<za::String> available;
    za::String active;
    za::Vector<za::String> processes;
    VdSetting vd;
};

[[nodiscard]] za::String lower(const char* s)
{
    za::String out;
    for(; *s; s++)
    {
        out += static_cast<char>(tolower(static_cast<unsigned char>(*s)));
    }
    return out;
}

[[nodiscard]] Environment environment()
{
    Environment env;
    if(vr_xr_test.value != 0.f)
    {
        env.test = true;
        env.available = splitList(vr_xr_test_runtimes.string);
        env.active = vr_xr_test_active.string;
        for(const za::String& p : splitList(vr_xr_test_processes.string))
        {
            env.processes.pushBack(lower(p.cStr()));
        }
        env.vd = vdSetting(true);
        return env;
    }
    env.vd = vdSetting(false);
#ifdef _WIN32
    HKEY key = nullptr;
    if(RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Khronos\\OpenXR\\1\\AvailableRuntimes", 0, KEY_READ, &key) == ERROR_SUCCESS)
    {
        char name[1024];
        for(DWORD i = 0;; i++)
        {
            DWORD length = sizeof(name);
            DWORD type = 0;
            DWORD disabled = 0;
            DWORD size = sizeof(disabled);
            const LONG r = RegEnumValueA(key, i, name, &length, nullptr, &type, reinterpret_cast<BYTE*>(&disabled), &size);
            if(r == ERROR_NO_MORE_ITEMS)
            {
                break;
            }
            if(r == ERROR_SUCCESS && !(type == REG_DWORD && disabled != 0)) // (0: enabled)
            {
                env.available.pushBack(za::String{name});
            }
        }
        RegCloseKey(key);
    }
    char active[1024];
    DWORD size = sizeof(active);
    if(RegGetValueA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Khronos\\OpenXR\\1", "ActiveRuntime", RRF_RT_REG_SZ, nullptr, active,
           &size) == ERROR_SUCCESS)
    {
        env.active = active;
    }
    const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if(snapshot != INVALID_HANDLE_VALUE)
    {
        PROCESSENTRY32 entry{};
        entry.dwSize = sizeof(entry);
        for(BOOL more = Process32First(snapshot, &entry); more; more = Process32Next(snapshot, &entry))
        {
            for(const Known& k : known)
            {
                for(const char* process : k.processes)
                {
                    if(process && !q_strcasecmp(entry.szExeFile, process))
                    {
                        env.processes.pushBack(za::String{process});
                    }
                }
            }
        }
        CloseHandle(snapshot);
    }
#endif
    return env;
}

// A known runtime's running process (null: none running).
[[nodiscard]] const char* runningProcess(const Environment& env, Kind kind)
{
    const Known* k = knownOf(kind);
    if(!k)
    {
        return nullptr;
    }
    for(const char* process : k->processes)
    {
        for(const za::String& p : env.processes)
        {
            if(process && p == za::StringView{process})
            {
                return process;
            }
        }
    }
    return nullptr;
}

// An installed runtime, as Auto sees it.
struct Installed
{
    za::String manifest;
    Kind kind;
    bool active;
    const char* running; // its app's process, or null
    bool exists;
};

[[nodiscard]] za::Vector<Installed> installedRuntimes(const Environment& env)
{
    za::Vector<Installed> list;
    auto add = [&](const char* manifest) {
        if(!manifest[0])
        {
            return;
        }
        for(const Installed& i : list)
        {
            if(sameFile(i.manifest.cStr(), manifest))
            {
                return;
            }
        }
        const Kind kind = kindOf(manifest);
        list.pushBack(Installed{za::String{manifest}, kind, !env.active.empty() && sameFile(manifest, env.active.cStr()),
            runningProcess(env, kind), fileExists(manifest)});
    };
    for(const za::String& m : env.available)
    {
        add(m.cStr());
    }
    add(env.active.cStr());
    // Virtual Desktop's, SteamVR's and Meta's in their usual places, when the registry doesn't list them (not in the
    // test environment).
    if(!env.test)
    {
        for(const Known& k : known)
        {
            bool listed = false;
            for(const Installed& i : list)
            {
                listed = listed || i.kind == k.kind;
            }
            if(!listed && fileExists(k.usualPlace))
            {
                add(k.usualPlace);
            }
        }
    }
    return list;
}

// Auto's order: the runtime whose app runs (Virtual Desktop's first: with a headset connected through the Streamer its
// xrGetSystem works, else it fails and the next is tried; then the system's active runtime when its app runs, then
// Meta's, SteamVR's); then the system's active runtime; then the other installed ones, but SteamVR's only with
// vr_xr_runtime_fallback 2 (loading it starts SteamVR, its windows and all, to find no headset: last). Manifests that
// don't exist are left out. While the Streamer runs, Virtual Desktop's own runtime setting decides between its two:
// SteamVR -> SteamVR's first (running or not: loading it starts SteamVR, which reaches the headset through VD's
// driver), then VDXR; VDXR, Automatic or unreadable -> VDXR first, as before.
void autoOrder(const Environment& env, const za::Vector<Installed>& installed, Plan& plan)
{
    za::Vector<bool> taken(installed.size(), false);
    auto take = [&](size_t i, const char* reason) {
        taken[i] = true;
        plan.attempts.pushBack(Attempt{installed[i].manifest, labelOf(installed[i].manifest.cStr()), za::String{reason}});
    };
    auto takeKind = [&](Kind kind, bool running) {
        for(size_t i = 0; i < installed.size(); i++)
        {
            const Installed& r = installed[i];
            if(!taken[i] && r.exists && r.kind == kind && (r.running != nullptr) == running)
            {
                const Known* k = knownOf(kind);
                take(i, running ? k->reason : r.active ? "the system's active runtime" : "installed, not running");
            }
        }
    };

    const VdChoice vd = env.vd.choice;
    const bool streamer = runningProcess(env, Kind::VirtualDesktop) != nullptr;
    bool steamFirst = false;
    if(streamer && vd == VdChoice::SteamVR)
    {
        for(size_t i = 0; i < installed.size(); i++)
        {
            if(!taken[i] && installed[i].exists && installed[i].kind == Kind::SteamVR)
            {
                take(i, installed[i].running ? "Virtual Desktop set to SteamVR, running" : "Virtual Desktop set to SteamVR");
                steamFirst = true;
            }
        }
    }
    for(size_t i = 0; i < installed.size(); i++)
    {
        if(!taken[i] && installed[i].exists && installed[i].kind == Kind::VirtualDesktop && installed[i].running)
        {
            const char* reason = "Streamer running";
            switch(vd)
            {
                case VdChoice::SteamVR:
                    reason = steamFirst ? "Streamer running, set to SteamVR" : "Streamer running, set to SteamVR (not installed)";
                    break;
                case VdChoice::VDXR: reason = "Streamer running, set to VDXR"; break;
                case VdChoice::Automatic: reason = "Streamer running, set to Automatic"; break;
                default: break;
            }
            take(i, reason);
        }
    }
    for(size_t i = 0; i < installed.size(); i++)
    {
        if(!taken[i] && installed[i].exists && installed[i].active && installed[i].running)
        {
            take(i, knownOf(installed[i].kind)->reason);
        }
    }
    takeKind(Kind::Meta, true);
    takeKind(Kind::SteamVR, true);
    const bool anyRunning = !plan.attempts.empty();
    for(size_t i = 0; i < installed.size(); i++)
    {
        if(!taken[i] && installed[i].exists && installed[i].active)
        {
            take(i, anyRunning ? "the system's active runtime" : "nothing running: the system's active runtime");
        }
    }
    takeKind(Kind::VirtualDesktop, false);
    takeKind(Kind::Meta, false);
    for(size_t i = 0; i < installed.size(); i++)
    {
        if(!taken[i] && installed[i].exists && installed[i].kind == Kind::Other)
        {
            take(i, "installed");
        }
    }
    if(vr_xr_runtime_fallback.value >= 2.f)
    {
        takeKind(Kind::SteamVR, false);
    }
}

// XR_RUNTIME_JSON as the game was started with it (an outside one wins over vr_xr_runtime); the start's log
// (qvr_openxr.txt in the game folder: logPath()).
struct State
{
    bool outsideChecked = false;
    za::String outside;
    za::String status;    // statusLine()
    bool logging = false; // since the backend's first start: lines go to the log too
    bool logStarted = false;
};
State state;

void checkOutside()
{
    if(!state.outsideChecked)
    {
        state.outsideChecked = true;
        const char* env = getenv("XR_RUNTIME_JSON");
        state.outside = env ? env : "";
    }
}

// A line for the log (opened and closed for each: a crash right after loses nothing).
// always: also between starts (the stops), once a start began; else only during a start (not every frame's failure).
void logText(const char* text, bool always = false)
{
    if(!state.logging && !(always && state.logStarted))
    {
        return;
    }
    FILE* f = fopen(logPath(), "a");
    if(f)
    {
        fputs(text, f);
        fclose(f);
    }
}

// +vr_xr_runtime on the command line (the shortcut's or the debugger's arguments): its value, or null.
[[nodiscard]] const char* commandLineChoice()
{
    const int i = COM_CheckParm("+vr_xr_runtime");
    return i && i + 1 < com_argc ? com_argv[i + 1] : nullptr;
}

// vr_xr_runtime is still the command line's (set again at every start, over the menu's choice).
[[nodiscard]] bool fromCommandLine()
{
    const char* value = commandLineChoice();
    return value && static_cast<int>(Q_atof(value)) == static_cast<int>(vr_xr_runtime.value);
}

[[nodiscard]] const char* modeName(int mode)
{
    switch(mode)
    {
        case 0: return "System default";
        case 1: return "Virtual Desktop (VDXR)";
        case 2: return "SteamVR";
        case 3: return "Manifest";
        case 4: return "Auto";
        default: return "?";
    }
}

[[nodiscard]] Plan makePlan(const Environment& env, const za::Vector<Installed>& installed)
{
    Plan plan;
    const int mode = static_cast<int>(vr_xr_runtime.value);
    plan.mode = modeName(mode);
    char line[512];

    checkOutside();
    if(!state.outside.empty())
    {
        plan.keepEnvironment = true;
        plan.mode = "XR_RUNTIME_JSON";
        plan.attempts.pushBack(Attempt{state.outside, labelOf(state.outside.cStr()), za::String{"XR_RUNTIME_JSON set outside the game"}});
        q_snprintf(line, sizeof(line), "%s - XR_RUNTIME_JSON set outside the game", plan.attempts[0].label.cStr());
        plan.summary = line;
        return plan;
    }

    // The system's active runtime: the loader's own choice (in the test environment, the fake active manifest).
    auto systemDefault = [&](const char* why) {
        plan.attempts.pushBack(Attempt{env.test ? env.active : za::String{},
            env.active.empty() ? za::String{"none"} : labelOf(env.active.cStr()), za::String{why}});
    };

    if(mode == 4)
    {
        autoOrder(env, installed, plan);
        plan.fallback = vr_xr_runtime_fallback.value != 0.f;
        if(plan.attempts.empty())
        {
            systemDefault("no runtime installed");
        }
        q_snprintf(line, sizeof(line), "Auto: %s - %s%s", plan.attempts[0].label.cStr(), plan.attempts[0].reason.cStr(),
            fromCommandLine() ? " (from the command line)" : "");
        plan.summary = line;
        return plan;
    }

    // A runtime picked by hand: that one only.
    za::String manifest;
    if(mode == 1 || mode == 2)
    {
        const Kind kind = mode == 1 ? Kind::VirtualDesktop : Kind::SteamVR;
        for(const Installed& i : installed)
        {
            if(i.kind == kind && i.exists && manifest.empty())
            {
                manifest = i.manifest;
            }
        }
        if(manifest.empty() && !env.test)
        {
            manifest = knownOf(kind)->usualPlace;
        }
    }
    else if(mode == 3)
    {
        manifest = vr_xr_runtime_json.string;
    }

    if(mode == 1 || mode == 2 || mode == 3)
    {
        if(!manifest.empty() && fileExists(manifest.cStr()))
        {
            plan.attempts.pushBack(Attempt{manifest, labelOf(manifest.cStr()),
                za::String{fromCommandLine() ? "chosen on the command line" : "chosen in the menu"}});
        }
        else
        {
            q_snprintf(line, sizeof(line), "%s not found: the system's active runtime",
                manifest.empty() ? modeName(mode) : manifest.cStr());
            systemDefault(line);
        }
    }
    else
    {
        systemDefault("the system's active runtime");
    }
    q_snprintf(line, sizeof(line), "%s: %s - %s", modeName(mode), plan.attempts[0].label.cStr(), plan.attempts[0].reason.cStr());
    plan.summary = line;
    return plan;
}

// The graphics DLLs a runtime loads that the GPU driver keeps pointers into, kept loaded for good once a runtime
// loaded them (keepGraphicsModules()). VDXR loads d3d11.dll (it renders with D3D11 and shares the game's OpenGL images
// through NVIDIA's GL/D3D interop) and unloads it with itself; NVIDIA's OpenGL driver thread then reads d3d11.dll's
// memory and the game crashes (the author's crashes of 2026-10-09 switching from VDXR: an access violation in
// nvapi64_impl.dll called from nvoglv64.dll's thread, reading the unloaded d3d11.dll's memory).
constexpr const char* graphicsModules[] = {"d3d11.dll", "dxgi.dll", "d3d12.dll", "vulkan-1.dll"};
constexpr size_t graphicsModuleCount = sizeof(graphicsModules) / sizeof(graphicsModules[0]);
bool graphicsModuleKept[graphicsModuleCount] = {};

// What Auto sees and the order it would try the runtimes in (vr_xr_runtime_explain; the log of each start).
void describe()
{
    const Environment env = environment();
    const za::Vector<Installed> installed = installedRuntimes(env);
    const Plan p = makePlan(env, installed);

    note("vr_xr_runtime %d (%s)%s\n", static_cast<int>(vr_xr_runtime.value), modeName(static_cast<int>(vr_xr_runtime.value)),
        env.test ? "; test environment (vr_xr_test 1)" : "");
    if(const char* cl = commandLineChoice())
    {
        note(fromCommandLine() ? "  set on the command line (+vr_xr_runtime %s): set again at every start, over the menu's choice\n"
                              : "  the command line's +vr_xr_runtime %s is set again at the next start, over the menu's choice\n",
            cl);
    }
    checkOutside();
    note("  XR_RUNTIME_JSON when the game started: %s\n", state.outside.empty() ? "not set" : state.outside.cStr());
#ifdef _WIN32
    za::String modules;
    for(size_t i = 0; i < graphicsModuleCount; i++)
    {
        modules += i ? ", " : "";
        modules += graphicsModules[i];
        modules += graphicsModuleKept[i] ? " kept" : GetModuleHandleA(graphicsModules[i]) ? " loaded" : " not loaded";
    }
    note("  graphics DLLs: %s\n", modules.cStr());
#endif
    for(const Known& k : known)
    {
        const char* running = runningProcess(env, k.kind);
        note("  %s: %s\n", k.label, running ? va("%s running", running) : "not running");
    }
    const bool streamer = runningProcess(env, Kind::VirtualDesktop) != nullptr;
    const char* effect = env.vd.choice == VdChoice::Unknown ? ": Auto as without it"
                         : !streamer                         ? " (ignored: the Streamer isn't running)"
                         : env.vd.choice == VdChoice::SteamVR ? ": SteamVR first"
                                                              : ": VDXR first";
    note("  Virtual Desktop's OpenXR runtime setting: %s%s\n", vdChoiceName(env.vd.choice), effect);
    note("     %s\n", env.vd.source.cStr());
    for(const Installed& i : installed)
    {
        bool planned = false;
        for(const Attempt& a : p.attempts)
        {
            planned = planned || sameFile(a.manifest.cStr(), i.manifest.cStr());
        }
        const bool idleSteamVR = vr_xr_runtime.value == 4.f && i.exists && i.kind == Kind::SteamVR && !i.running &&
                                 !i.active && !planned;
        note("  installed: %s%s%s%s\n", labelOf(i.manifest.cStr()).cStr(), i.active ? ", the system's active" : "",
            i.exists ? "" : ", NOT FOUND (left out)",
            idleSteamVR ? ", not running: left out (it would start SteamVR; vr_xr_runtime_fallback 2 tries it)" : "");
        note("     %s\n", i.manifest.cStr());
    }
    if(installed.empty())
    {
        note("  installed: none\n");
    }
    note("choice: %s\n", p.summary.cStr());
    for(size_t i = 0; i < p.attempts.size(); i++)
    {
        const Attempt& a = p.attempts[i];
        note("  %d. %s (%s)%s\n", static_cast<int>(i + 1), a.label.cStr(), a.reason.cStr(),
            simulatedFailure(a) ? " [fails: simulated]" : "");
        note("     %s\n", a.manifest.empty() ? "(the loader's choice: the system's active runtime)" : a.manifest.cStr());
    }
    if(p.attempts.size() > 1)
    {
        note("fallback: %s\n", p.fallback ? "on to the next while one fails, then flat" : "off (vr_xr_runtime_fallback 0)");
    }
    // Where the tries would end if each simulated failure fails and every other attempt starts.
    int first = -1;
    for(size_t i = 0; i < p.attempts.size() && first < 0; i++)
    {
        if(!simulatedFailure(p.attempts[i]) && (i == 0 || p.fallback))
        {
            first = static_cast<int>(i);
        }
    }
    note("last start: %s\n", state.status.empty() ? "none yet (the OpenXR backend hasn't started)" : state.status.cStr());
    note("outcome: %s\n", first < 0 ? "flat (every runtime fails)" : va("%d. %s", first + 1, p.attempts[first].label.cStr()));
}

void explain_f()
{
    describe();
    note("log of the last start: %s\n", logPath());
}

// A runtime manifest's library: its "library_path", next to the manifest when relative ("" when unreadable).
[[nodiscard]] za::String libraryOf(const char* manifest)
{
    za::String library;
    FILE* f = fopen(manifest, "rb");
    if(!f)
    {
        return library;
    }
    char text[8192];
    const size_t n = fread(text, 1, sizeof(text) - 1, f);
    fclose(f);
    text[n] = 0;
    const char* p = strstr(text, "\"library_path\"");
    if(!p)
    {
        return library;
    }
    p += strlen("\"library_path\"");
    while(*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n' || *p == ':')
    {
        p++;
    }
    if(*p != '"')
    {
        return library;
    }
    za::String path;
    for(p++; *p && *p != '"'; p++)
    {
        if(*p == '\\' && p[1])
        {
            p++; // (JSON's escapes: \\ and \/)
        }
        path += *p;
    }
    const bool absolute = (path.size() > 1 && path.cStr()[1] == ':') || path.cStr()[0] == '\\' || path.cStr()[0] == '/';
    if(!absolute)
    {
        const char* name = fileName(manifest);
        for(const char* c = manifest; c < name; c++)
        {
            library += *c;
        }
    }
    library += path.cStr();
#ifdef _WIN32
    char full[1024];
    if(GetFullPathNameA(library.cStr(), sizeof(full), full, nullptr) > 0)
    {
        library = full;
    }
#endif
    return library;
}

} // namespace

const char* logPath()
{
    return va("%s/qvr_openxr.txt", com_gamedir);
}

void beginLog()
{
    // The first start of the game rewrites the log; restarts (vr_restart, another runtime chosen) add to it.
    if(!state.logStarted)
    {
        state.logStarted = true;
        if(FILE* f = fopen(logPath(), "w"))
        {
            fclose(f);
        }
    }
    state.logging = true;
    char when[64] = "";
    const time_t now = time(nullptr);
    if(const tm* t = localtime(&now))
    {
        strftime(when, sizeof(when), "%Y-%m-%d %H:%M:%S", t);
    }
    logText(va("\n=== OpenXR start, %s; Quake VR %s\n", when, VR_BuildVersion()));
    za::String args;
    for(int i = 0; i < com_argc; i++)
    {
        args += i ? " " : "";
        args += com_argv[i];
    }
    logText(va("command line: %s\n", args.cStr()));
    const char* env = getenv("XR_RUNTIME_JSON");
    logText(va("XR_RUNTIME_JSON now (the game's last attempt's, or the starting one): %s\n", env && env[0] ? env : "not set"));
    describe();
}

void note(const char* format, ...)
{
    char text[2048];
    va_list args;
    va_start(args, format);
    q_vsnprintf(text, sizeof(text), format, args);
    va_end(args);
    Con_Printf("%s", text);
    logText(text);
}

void warn(const char* format, ...)
{
    char text[2048];
    va_list args;
    va_start(args, format);
    q_vsnprintf(text, sizeof(text), format, args);
    va_end(args);
    Con_Warning("%s", text);
    logText(va("WARNING: %s", text));
}

void loaded(const Attempt& attempt, const char* runtimeName)
{
    const char* env = getenv("XR_RUNTIME_JSON");
    note("OpenXR: the loader loaded %s (XR_RUNTIME_JSON %s)\n", runtimeName, env && env[0] ? env : "not set: the system's active runtime");
    if(attempt.manifest.empty())
    {
        return;
    }
    const za::String library = libraryOf(attempt.manifest.cStr());
    if(library.empty())
    {
        note("OpenXR: %s's library unknown (%s unreadable)\n", attempt.label.cStr(), attempt.manifest.cStr());
        return;
    }
#ifdef _WIN32
    if(GetModuleHandleA(library.cStr()))
    {
        note("OpenXR: %s's library is the one loaded: %s\n", attempt.label.cStr(), library.cStr());
    }
    else
    {
        warn("OpenXR: %s's library isn't loaded (%s): the loader loaded another runtime, %s\n", attempt.label.cStr(),
            library.cStr(), runtimeName);
    }
#endif
}

void logLine(const char* text)
{
    logText(text, true);
}

void keepGraphicsModules()
{
#ifdef _WIN32
    for(size_t i = 0; i < graphicsModuleCount && vr_xr_keep_graphics_dlls.value != 0.f; i++)
    {
        HMODULE module = nullptr;
        if(!graphicsModuleKept[i] && GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_PIN, graphicsModules[i], &module))
        {
            graphicsModuleKept[i] = true;
            const char* text =
                va("OpenXR: keeping %s loaded for good (the GPU driver may use it after the runtime unloads)\n", graphicsModules[i]);
            Con_Printf("%s", text);
            logText(text, true);
        }
    }
#endif
}

bool isSteamVR(const Attempt& attempt)
{
    return !attempt.manifest.empty() && kindOf(attempt.manifest.cStr()) == Kind::SteamVR;
}

void registerCommands()
{
    checkOutside();
    Cmd_AddCommand("vr_xr_runtime_explain", explain_f);
}

Plan plan()
{
    const Environment env = environment();
    return makePlan(env, installedRuntimes(env));
}

void use(const Plan& plan, const Attempt& attempt)
{
    if(plan.keepEnvironment)
    {
        note("OpenXR: XR_RUNTIME_JSON is set outside the game (%s): vr_xr_runtime is ignored\n", attempt.manifest.cStr());
        return;
    }
    const char* value = attempt.manifest.empty() ? nullptr : attempt.manifest.cStr();
#ifdef _WIN32
    SetEnvironmentVariableA("XR_RUNTIME_JSON", value); // (the loader's getenv reads the process environment)
    _putenv_s("XR_RUNTIME_JSON", value ? value : "");
#endif
    note("OpenXR: trying %s (%s): %s\n", attempt.label.cStr(), attempt.reason.cStr(),
        value ? value : "the system's active runtime");
}

bool simulatedFailure(const Attempt& attempt)
{
    if(vr_xr_test.value != 0.f && attempt.manifest.empty())
    {
        return true; // the test environment never loads the real system runtime
    }
    for(const za::String& s : splitList(vr_xr_test_fail.string))
    {
        if(s == za::StringView{"*"} || q_strcasestr(attempt.manifest.cStr(), s.cStr()) ||
            q_strcasestr(attempt.label.cStr(), s.cStr()))
        {
            return true;
        }
    }
    return false;
}

void setOutcome(const Plan& plan, int started)
{
    char line[512];
    if(started < 0)
    {
        q_snprintf(line, sizeof(line), "%s: none started (flat)", plan.mode.cStr());
    }
    else if(started == 0)
    {
        q_snprintf(line, sizeof(line), "%s", plan.summary.cStr());
    }
    else
    {
        const Attempt& a = plan.attempts[static_cast<size_t>(started)];
        q_snprintf(line, sizeof(line), "%s: %s - %s (%s failed)", plan.mode.cStr(), a.label.cStr(), a.reason.cStr(),
            plan.attempts[0].label.cStr());
    }
    state.status = line;
    if(started < 0)
    {
        warn("OpenXR: no runtime started (%d tried): VR is off\n", static_cast<int>(plan.attempts.size()));
    }
    else
    {
        note("OpenXR runtime in use: %s\n", line);
    }
    state.logging = false; // (the stops still logged: logLine)
}

const char* statusLine()
{
    return state.status.cStr();
}

} // namespace qvr::xrruntime
