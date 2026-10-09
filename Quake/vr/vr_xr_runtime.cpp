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
#include <stdlib.h>
#include <string.h>

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

// What the system has: the installed runtimes (enabled ones), the active one, the running processes (lower case). The
// real ones, or vr_xr_test_*'s (vr_xr_test 1: nothing real is read, nor loaded; no usual places).
struct Environment
{
    bool test = false;
    za::Vector<za::String> available;
    za::String active;
    za::Vector<za::String> processes;
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
        return env;
    }
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
// don't exist are left out.
void autoOrder(const za::Vector<Installed>& installed, Plan& plan)
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

    takeKind(Kind::VirtualDesktop, true);
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

// XR_RUNTIME_JSON as the game was started with it (an outside one wins over vr_xr_runtime).
struct State
{
    bool outsideChecked = false;
    za::String outside;
    za::String status; // statusLine()
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
        autoOrder(installed, plan);
        plan.fallback = vr_xr_runtime_fallback.value != 0.f;
        if(plan.attempts.empty())
        {
            systemDefault("no runtime installed");
        }
        q_snprintf(line, sizeof(line), "Auto: %s - %s", plan.attempts[0].label.cStr(), plan.attempts[0].reason.cStr());
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
            plan.attempts.pushBack(Attempt{manifest, labelOf(manifest.cStr()), za::String{"chosen in the menu"}});
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

void explain_f()
{
    const Environment env = environment();
    const za::Vector<Installed> installed = installedRuntimes(env);
    const Plan p = makePlan(env, installed);

    Con_Printf("vr_xr_runtime %d (%s)%s\n", static_cast<int>(vr_xr_runtime.value), modeName(static_cast<int>(vr_xr_runtime.value)),
        env.test ? "; test environment (vr_xr_test 1)" : "");
    for(const Known& k : known)
    {
        const char* running = runningProcess(env, k.kind);
        Con_Printf("  %s: %s\n", k.label, running ? va("%s running", running) : "not running");
    }
    for(const Installed& i : installed)
    {
        const bool idleSteamVR = vr_xr_runtime.value == 4.f && i.exists && i.kind == Kind::SteamVR && !i.running &&
                                 !i.active && vr_xr_runtime_fallback.value < 2.f;
        Con_Printf("  installed: %s%s%s%s\n", labelOf(i.manifest.cStr()).cStr(), i.active ? ", the system's active" : "",
            i.exists ? "" : ", NOT FOUND (left out)",
            idleSteamVR ? ", not running: left out (it would start SteamVR; vr_xr_runtime_fallback 2 tries it)" : "");
        Con_Printf("     %s\n", i.manifest.cStr());
    }
    if(installed.empty())
    {
        Con_Printf("  installed: none\n");
    }
    Con_Printf("choice: %s\n", p.summary.cStr());
    for(size_t i = 0; i < p.attempts.size(); i++)
    {
        const Attempt& a = p.attempts[i];
        Con_Printf("  %d. %s (%s)%s\n", static_cast<int>(i + 1), a.label.cStr(), a.reason.cStr(),
            simulatedFailure(a) ? " [fails: simulated]" : "");
        Con_Printf("     %s\n", a.manifest.empty() ? "(the loader's choice: the system's active runtime)" : a.manifest.cStr());
    }
    if(p.attempts.size() > 1)
    {
        Con_Printf("fallback: %s\n", p.fallback ? "on to the next while one fails, then flat" : "off (vr_xr_runtime_fallback 0)");
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
    Con_Printf("last start: %s\n", state.status.empty() ? "none yet (the OpenXR backend hasn't started)" : state.status.cStr());
    Con_Printf("outcome: %s\n", first < 0 ? "flat (every runtime fails)" : va("%d. %s", first + 1, p.attempts[first].label.cStr()));
}

} // namespace

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
        Con_Printf("OpenXR: XR_RUNTIME_JSON is set outside the game (%s): vr_xr_runtime is ignored\n", attempt.manifest.cStr());
        return;
    }
    const char* value = attempt.manifest.empty() ? nullptr : attempt.manifest.cStr();
#ifdef _WIN32
    SetEnvironmentVariableA("XR_RUNTIME_JSON", value); // (the loader's getenv reads the process environment)
    _putenv_s("XR_RUNTIME_JSON", value ? value : "");
#endif
    Con_Printf("OpenXR: trying %s (%s): %s\n", attempt.label.cStr(), attempt.reason.cStr(),
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
        Con_Warning("OpenXR: no runtime started (%d tried): VR is off\n", static_cast<int>(plan.attempts.size()));
    }
    else
    {
        Con_Printf("OpenXR runtime in use: %s\n", line);
    }
}

const char* statusLine()
{
    return state.status.cStr();
}

} // namespace qvr::xrruntime
