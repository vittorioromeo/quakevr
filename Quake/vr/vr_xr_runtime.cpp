// vr_xr_runtime.cpp -- which OpenXR runtime the OpenXR backend loads, and the order it tries the others in
// (vr_xr_runtime.hpp).

#include "vr_strings.hpp"
#include "vr_xr_runtime.hpp"
#include "vr_runlogs.hpp"

#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/String/String.hpp"
#include "vr_api.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <tlhelp32.h>
#else
#include <dirent.h>
#include <limits.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#include "Zancle/Concurrency/Atomic.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Base/Strcmp.hpp"
#include "Zancle/Base/Strlen.hpp"
#include "Zancle/Base/Strncmp.hpp"
#include "Zancle/Base/Strstr.hpp"
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
    const char* processes[3]; // (lower case)
    const char* reason;       // why, when one of them runs
    const char* usualPlace;
};

#ifndef _WIN32
// Linux: SteamVR's runtime (Monado, WiVRn and the rest: Other, found as the active runtime). Its processes: the server,
// the compositor, the status window (vrmonitor), matched by /proc/<pid>/comm, the executable's or argv's file name.
constexpr Known known[] = {
    {Kind::SteamVR, "SteamVR", "steamxr_linux64.json", {"vrserver", "vrcompositor", "vrmonitor"}, "running",
        "~/.steam/steam/steamapps/common/SteamVR/steamxr_linux64.json"},
};

// The game's architecture as the OpenXR loader names it (manifests' <name>.<arch>.json, active_runtime.<arch>.json),
// with its ELF class and machine (e_machine).
#if defined(__x86_64__)
constexpr const char* ourArch = "x86_64";
constexpr int ourElfMachine = 62; // EM_X86_64
#elif defined(__i386__)
constexpr const char* ourArch = "i686";
constexpr int ourElfMachine = 3; // EM_386
#elif defined(__aarch64__)
constexpr const char* ourArch = "aarch64";
constexpr int ourElfMachine = 183; // EM_AARCH64
#elif defined(__arm__)
constexpr const char* ourArch = "armv7a-vfp";
constexpr int ourElfMachine = 40; // EM_ARM
#else
constexpr const char* ourArch = "unknown";
constexpr int ourElfMachine = 0; // (not checked)
#endif
// The loader spec's architecture names.
constexpr const char* loaderArches[] = {"x86_64", "i686", "aarch64", "armv7a-vfp", "armv5te", "mips64", "mips", "ppc64",
    "ppc64el", "s390x", "hppa", "alpha", "ia64", "m68k", "riscv64", "sparc64"};
#else
constexpr Known known[] = {
    {Kind::VirtualDesktop, "Virtual Desktop (VDXR)", "virtualdesktop-openxr.json", {"virtualdesktop.streamer.exe", nullptr},
        "Streamer running", "C:\\Program Files\\Virtual Desktop Streamer\\OpenXR\\virtualdesktop-openxr.json"},
    {Kind::SteamVR, "SteamVR", "steamxr_win64.json", {"vrserver.exe", "vrmonitor.exe"}, "running",
        "C:\\Program Files (x86)\\Steam\\steamapps\\common\\SteamVR\\steamxr_win64.json"},
    {Kind::Meta, "Meta (Oculus)", "oculus_openxr_64.json", {"ovrserver_x64.exe", nullptr}, "OVRServer running",
        "C:\\Program Files\\Oculus\\Support\\oculus-runtime\\oculus_openxr_64.json"},
};
#endif

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
    struct stat st{};
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
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
    for(za::SizeT n; (n = fread(buffer, 1, sizeof(buffer), f)) > 0 && text.size() < 1024 * 1024;)
    {
        for(za::SizeT i = 0; i < n; i++)
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
    const char* p = key + ZA_STRLEN("\"OpenXRRuntime\"");
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
            if(!q_strncasecmp(p + 1, name, ZA_STRLEN(name)) && p[1 + ZA_STRLEN(name)] == '"')
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
    za::SizeT n = 0;
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
    bool sandbox = false; // Linux: in a Flatpak sandbox (/.flatpak-info), whose /proc shows its own processes only
};

#ifndef _WIN32
// A process's names, to match the known runtimes' (Linux): /proc/<pid>/comm (its name, at most 15 characters), its
// executable's file name (/proc/<pid>/exe: the other users' are unreadable) and its first two arguments' (argv[0]; argv[1]
// for a program started through ld.so or a shell, as the Steam Linux Runtime's container may start SteamVR's).
[[nodiscard]] za::Vector<za::String> processNames(const char* pid)
{
    za::Vector<za::String> names;
    char text[PATH_MAX] = "";
    if(FILE* f = fopen(va("/proc/%s/comm", pid), "rb"))
    {
        if(fgets(text, sizeof(text), f))
        {
            text[strcspn(text, "\r\n")] = 0;
            names.pushBack(za::String{text});
        }
        fclose(f);
    }
    const ssize_t length = readlink(va("/proc/%s/exe", pid), text, sizeof(text) - 1);
    if(length > 0)
    {
        text[length] = 0;
        names.pushBack(za::String{fileName(text)});
    }
    if(FILE* f = fopen(va("/proc/%s/cmdline", pid), "rb"))
    {
        const za::SizeT got = fread(text, 1, sizeof(text) - 1, f);
        fclose(f);
        text[got] = 0;
        const char* arg = text;
        for(int i = 0; i < 2 && arg < text + got; i++)
        {
            if(arg[0])
            {
                names.pushBack(za::String{fileName(arg)});
            }
            arg += ZA_STRLEN(arg) + 1;
        }
    }
    return names;
}

// In a Flatpak sandbox: the devices it was given ("dri;all;shm;", /.flatpak-info's [Context] devices), "" outside one.
[[nodiscard]] za::String flatpakDevices()
{
    za::String devices;
    if(FILE* f = fopen("/.flatpak-info", "rb"))
    {
        char line[1024];
        bool context = false;
        while(fgets(line, sizeof(line), f))
        {
            line[strcspn(line, "\r\n")] = 0;
            if(line[0] == '[')
            {
                context = !ZA_STRCMP(line, "[Context]");
            }
            else if(context && !ZA_STRNCMP(line, "devices=", 8))
            {
                devices = line + 8;
            }
        }
        fclose(f);
    }
    return devices;
}

// The sandbox shares the host's /dev/shm (--device=shm). --device=all alone gives the sandbox its own (Flatpak mounts a
// tmpfs there), where SteamVR's client finds none of its server's shared memory.
[[nodiscard]] bool sandboxSharesShm()
{
    const za::String devices = flatpakDevices();
    for(const za::String& d : splitList(devices.cStr()))
    {
        if(d == za::StringView{"shm"})
        {
            return true;
        }
    }
    return false;
}

// SteamVR's logs folder (~/.local/share/Steam/logs, the native Steam's): "writable", "read-only" or "none" (no such
// folder). vrclient, loaded in the game's process, writes its log there (vrclient_ironwail.txt) and fails to start
// without it (-51; GitHub #82): the Flatpak shares the Steam folder read-only, its logs folder written (the manifest's
// --filesystem=xdg-data/Steam/logs, from 2026-11-18). (HOME, not XDG_DATA_HOME: the Flatpak points that at its own.)
[[nodiscard]] const char* steamLogsAccess()
{
    const char* home = getenv("HOME");
    if(!home || !*home)
    {
        return "none";
    }
    const char* dir = va("%s/.local/share/Steam/logs", home);
    struct stat st{};
    if(stat(dir, &st) != 0 || !S_ISDIR(st.st_mode))
    {
        return "none";
    }
    return access(dir, W_OK) == 0 ? "writable" : "read-only";
}

// The /dev/shm entries that look like SteamVR's, OpenVR's or Steam's (for the report: what a running SteamVR leaves
// there, as seen from the game; "none" when nothing is).
[[nodiscard]] za::String vrShmEntries()
{
    za::String list;
    int count = 0;
    if(DIR* d = opendir("/dev/shm"))
    {
        while(const dirent* e = readdir(d))
        {
            const za::String name = qvr::strings::lower(e->d_name);
            bool vr = false;
            for(const char* word : {"steamvr", "openvr", "vrserver", "vrcompositor", "valve"})
            {
                vr = vr || ZA_STRSTR(name.cStr(), word) != nullptr;
            }
            if(vr && ++count <= 8)
            {
                list += list.empty() ? "" : ", ";
                list += e->d_name;
            }
        }
        closedir(d);
    }
    if(count > 8)
    {
        list += va(" (+%d more)", count - 8);
    }
    return list.empty() ? za::String{"none"} : list;
}
#endif

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
            env.processes.pushBack(qvr::strings::lower(p.cStr()));
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
#else
    // Linux (the loader's search, OpenXR loader spec "Linux active runtime location"): active_runtime.json in
    // $XDG_CONFIG_HOME (~/.config), then each of $XDG_CONFIG_DIRS (/etc/xdg), then /etc; usually a symlink to the
    // runtime's manifest, named by its target. Available: the manifests in /usr/share/openxr/1 and
    // /usr/local/share/openxr/1 (where the runtimes install theirs: there is no registry of them).
    char path[PATH_MAX];
    za::Vector<za::String> configDirs;
    const char* home = getenv("HOME");
    const char* configHome = getenv("XDG_CONFIG_HOME");
    if(configHome && configHome[0])
    {
        configDirs.pushBack(za::String{configHome});
    }
    else if(home)
    {
        configDirs.pushBack(za::String{va("%s/.config", home)});
    }
    const char* xdgDirs = getenv("XDG_CONFIG_DIRS");
    for(const za::String& d : splitList(xdgDirs && xdgDirs[0] ? xdgDirs : "/etc/xdg"))
    {
        configDirs.pushBack(d);
    }
    configDirs.pushBack(za::String{"/etc"});
    // (In each folder the loader reads active_runtime.<arch>.json first, then active_runtime.json: loader 1.0.29+.)
    for(const za::String& d : configDirs)
    {
        const char* archFile = va("%s/openxr/1/active_runtime.%s.json", d.cStr(), ourArch);
        const char* file = fileExists(archFile) ? archFile : va("%s/openxr/1/active_runtime.json", d.cStr());
        if(fileExists(file))
        {
            env.active = realpath(file, path) ? path : file;
            break;
        }
    }
    for(const char* dir : {"/usr/share/openxr/1", "/usr/local/share/openxr/1"})
    {
        if(DIR* d = opendir(dir))
        {
            while(const dirent* e = readdir(d))
            {
                const za::SizeT n = ZA_STRLEN(e->d_name);
                if(n > 5 && !ZA_STRCMP(e->d_name + n - 5, ".json") && ZA_STRNCMP(e->d_name, "active_runtime.", 15))
                {
                    env.available.pushBack(za::String{va("%s/%s", dir, e->d_name)});
                }
            }
            closedir(d);
        }
    }
    // The known runtimes' processes, by each process's names (processNames). In a Flatpak sandbox /proc has the
    // sandbox's processes only (its own PID namespace): the host's SteamVR can't be seen ("can't tell" in the report).
    env.sandbox = fileExists("/.flatpak-info");
    if(DIR* proc = opendir("/proc"))
    {
        while(const dirent* e = readdir(proc))
        {
            if(e->d_name[0] < '0' || e->d_name[0] > '9')
            {
                continue;
            }
            const za::Vector<za::String> names = processNames(e->d_name);
            for(const Known& k : known)
            {
                for(const char* process : k.processes)
                {
                    bool match = false;
                    for(const za::String& name : names)
                    {
                        match = match || (process && name == za::StringView{process});
                    }
                    if(match)
                    {
                        env.processes.pushBack(za::String{process});
                    }
                }
            }
        }
        closedir(proc);
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
    bool exists;         // (false too for another architecture's)
    za::String foreign;  // why it is another architecture's ("" when it is ours: Linux)
};

// Linux: why a runtime manifest is for another architecture than the game's ("" when it is ours or unknown): its name's
// architecture (openxr_wivrn.i686.json, the loader spec's <name>.<arch>.json) or its library's ELF class and machine.
// The loader skips those; tried, they fail (XR_ERROR_RUNTIME_UNAVAILABLE). (Windows: "".)
[[nodiscard]] za::String foreignArch(const char* manifest);

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
        za::String foreign = foreignArch(manifest);
        const bool exists = fileExists(manifest) && foreign.empty();
        list.pushBack(Installed{za::String{manifest}, kind, !env.active.empty() && sameFile(manifest, env.active.cStr()),
            runningProcess(env, kind), exists, static_cast<za::String&&>(foreign)});
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
#ifndef _WIN32
            // (Linux: the usual place under the home folder)
            const char* home = getenv("HOME");
            const za::String place = k.usualPlace[0] == '~' && home ? za::String{home} + (k.usualPlace + 1)
                                                                    : za::String{k.usualPlace};
#else
            const za::String place{k.usualPlace};
#endif
            if(!listed && fileExists(place.cStr()))
            {
                add(place.cStr());
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
    auto take = [&](za::SizeT i, const char* reason) {
        taken[i] = true;
        plan.attempts.pushBack(Attempt{installed[i].manifest, labelOf(installed[i].manifest.cStr()), za::String{reason}});
    };
    auto takeKind = [&](Kind kind, bool running) {
        for(za::SizeT i = 0; i < installed.size(); i++)
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
        for(za::SizeT i = 0; i < installed.size(); i++)
        {
            if(!taken[i] && installed[i].exists && installed[i].kind == Kind::SteamVR)
            {
                take(i, installed[i].running ? "Virtual Desktop set to SteamVR, running" : "Virtual Desktop set to SteamVR");
                steamFirst = true;
            }
        }
    }
    for(za::SizeT i = 0; i < installed.size(); i++)
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
    for(za::SizeT i = 0; i < installed.size(); i++)
    {
        if(!taken[i] && installed[i].exists && installed[i].active && installed[i].running)
        {
            take(i, knownOf(installed[i].kind)->reason);
        }
    }
    takeKind(Kind::Meta, true);
    takeKind(Kind::SteamVR, true);
    const bool anyRunning = !plan.attempts.empty();
    for(za::SizeT i = 0; i < installed.size(); i++)
    {
        if(!taken[i] && installed[i].exists && installed[i].active)
        {
            take(i, anyRunning ? "the system's active runtime" : "nothing running: the system's active runtime");
        }
    }
    takeKind(Kind::VirtualDesktop, false);
    takeKind(Kind::Meta, false);
    for(za::SizeT i = 0; i < installed.size(); i++)
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
    za::String videoNote; // what beforeVideoInit did (Linux), in each start's log
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
// console: the text went to the console too (note, warn), whose lines Sys_Printf already puts in logcat on Android.
void logText(const char* text, bool always = false, bool console = false)
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
    runlogs::append("qvr_openxr.txt", text); // (the run's copy, kept with its logs: vr_runlogs.cpp)
#ifdef __ANDROID__
    if(!console)
    {
        VR_AndroidLog(text); // (logcat as well: adb reads no file the app makes in its folder, FUSE makes them 0660)
    }
#else
    (void)console;
#endif
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
#ifdef __ANDROID__ // (no manifests to read: the loader asks the system's runtime broker; C4b)
            plan.attempts.pushBack(Attempt{za::String{}, za::String{"the system's runtime"},
                za::String{"Android: the loader's runtime broker (a Quest: Meta's)"}});
#else
            systemDefault("no runtime installed");
#endif
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
        const Known* k = knownOf(kind); // null for VDXR on Linux (known[] has only SteamVR there)
        if(manifest.empty() && !env.test && k)
        {
            manifest = k->usualPlace;
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

// A JSON string's value: the first "key": "value" in text, its escapes (\\ and \/) undone ("" when there is none).
[[nodiscard]] za::String jsonString(const char* text, const char* key)
{
    za::String value;
    char quoted[128];
    q_snprintf(quoted, sizeof(quoted), "\"%s\"", key);
    const char* p = ZA_STRSTR(text, quoted);
    if(!p)
    {
        return value;
    }
    p += ZA_STRLEN(quoted);
    while(*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n' || *p == ':')
    {
        p++;
    }
    if(*p != '"')
    {
        return value;
    }
    for(p++; *p && *p != '"'; p++)
    {
        if(*p == '\\' && p[1])
        {
            p++;
        }
        value += *p;
    }
    return value;
}

[[nodiscard]] za::String libraryOf(const char* manifest);

#ifdef _WIN32
// The implicit OpenXR API layers installed: the loader puts each one between the game and the runtime unless its
// registry value is nonzero, its manifest's disable_environment variable is set, or it has an enable_environment
// variable that isn't. An overlay's or a toolkit's layer that misuses the runtime breaks it for the game: the leading
// suspect of 1.0.0's VDXR crash (vr_backend_openxr.cpp, guardedCall).
struct ApiLayer
{
    za::String manifest;
    za::String name;
    za::String library;
    za::String disableVar;
    za::String enableVar;
    bool registryOff = false;
};

// The disable_environment variables the game set (vr_xr_api_layers 0, vr_xr_skip_incompatible_layers 1: cleared again
// at each start, then set as those say), and those of them set for a known incompatibility.
za::Vector<za::String> layerVarsSet;
za::Vector<za::String> skippedVarsSet;

// The implicit layers known not to work with this game (it renders with OpenGL): turned off by their own switch
// (vr_xr_skip_incompatible_layers 1), as each manifest's "name" says. Add to it as players report them.
struct IncompatibleLayer
{
    const char* name;
    const char* why;
};

constexpr IncompatibleLayer incompatibleLayers[] = {
    {"XR_APILAYER_NOVENDOR_OBSMirror",
        "OBS's OpenXR mirror supports Direct3D 11/12 only, not OpenGL (with VDXR it crashed in xrEnumerateSwapchainImages: 1.0.1)"},
};

// Why a layer is skipped ("" when it isn't known to be incompatible).
[[nodiscard]] const char* incompatibility(const ApiLayer& layer)
{
    for(const IncompatibleLayer& known : incompatibleLayers)
    {
        if(!q_strcasecmp(layer.name.cStr(), known.name))
        {
            return known.why;
        }
    }
    return "";
}

// A layer from its manifest (name, its variables, its library).
[[nodiscard]] ApiLayer readLayer(const char* manifest, bool registryOff)
{
    ApiLayer layer;
    layer.manifest = manifest;
    layer.registryOff = registryOff;
    if(FILE* f = fopen(manifest, "rb"))
    {
        char text[8192];
        const za::SizeT n = fread(text, 1, sizeof(text) - 1, f);
        fclose(f);
        text[n] = 0;
        layer.name = jsonString(text, "name");
        layer.disableVar = jsonString(text, "disable_environment");
        layer.enableVar = jsonString(text, "enable_environment");
        layer.library = libraryOf(manifest);
    }
    return layer;
}

[[nodiscard]] za::Vector<ApiLayer> implicitLayers()
{
    za::Vector<ApiLayer> layers;
    // The test environment: vr_xr_test_layers' manifests instead of the registry's (docs/vr-port/TESTING.md).
    if(vr_xr_test.value != 0.f)
    {
        za::String manifest;
        for(const char* c = vr_xr_test_layers.string;; c++)
        {
            if(*c && *c != ',')
            {
                manifest += *c;
                continue;
            }
            if(!manifest.empty())
            {
                layers.pushBack(readLayer(manifest.cStr(), false));
            }
            manifest = "";
            if(!*c)
            {
                break;
            }
        }
        return layers;
    }
    for(HKEY root : {HKEY_LOCAL_MACHINE, HKEY_CURRENT_USER})
    {
        HKEY key = nullptr;
        if(RegOpenKeyExA(root, "SOFTWARE\\Khronos\\OpenXR\\1\\ApiLayers\\Implicit", 0, KEY_READ, &key) != ERROR_SUCCESS)
        {
            continue;
        }
        for(DWORD i = 0; i < 64; i++)
        {
            char name[1024];
            DWORD length = sizeof(name);
            DWORD type = 0;
            DWORD off = 0;
            DWORD size = sizeof(off);
            const LONG r = RegEnumValueA(key, i, name, &length, nullptr, &type, reinterpret_cast<BYTE*>(&off), &size);
            if(r == ERROR_NO_MORE_ITEMS)
            {
                break;
            }
            if(r != ERROR_SUCCESS)
            {
                continue;
            }
            layers.pushBack(readLayer(name, type == REG_DWORD && off != 0));
        }
        RegCloseKey(key);
    }
    return layers;
}

[[nodiscard]] bool envSet(const za::String& var)
{
    char value[64];
    return !var.empty() && GetEnvironmentVariableA(var.cStr(), value, sizeof(value)) > 0;
}

void setEnv(const char* var, const char* value)
{
    SetEnvironmentVariableA(var, value); // (the loader's getenv reads the process environment)
    _putenv_s(var, value ? value : "");
}

// Before each xrCreateInstance (the loader reads the variables then): the ones the game set before cleared, then
// vr_xr_api_layers 0: every implicit layer's disable_environment variable set; else, with
// vr_xr_skip_incompatible_layers 1, the known incompatible layers' (each said, to the console and qvr_openxr.txt). A
// variable set outside the game is left as it is.
void applyLayerSwitch(const za::Vector<ApiLayer>& layers)
{
    for(const za::String& var : layerVarsSet)
    {
        setEnv(var.cStr(), nullptr);
    }
    layerVarsSet.clear();
    skippedVarsSet.clear();
    const bool allOff = vr_xr_api_layers.value == 0.f;
    const bool skip = vr_xr_skip_incompatible_layers.value != 0.f;
    for(const ApiLayer& layer : layers)
    {
        const char* why = incompatibility(layer);
        if(!allOff && (!skip || !why[0]))
        {
            if(why[0] && !layer.registryOff)
            {
                note("OpenXR: API layer %s allowed (vr_xr_skip_incompatible_layers 0), though known not to work: %s\n",
                    layer.name.cStr(), why);
            }
            continue;
        }
        const char* name = layer.name.empty() ? layer.manifest.cStr() : layer.name.cStr();
        if(layer.registryOff || envSet(layer.disableVar))
        {
            if(why[0] && !allOff)
            {
                note("OpenXR: API layer %s: off already (%s)\n", name, layer.registryOff ? "its registry value" : "its variable");
            }
            continue;
        }
        if(layer.disableVar.empty())
        {
            if(why[0])
            {
                note("OpenXR: API layer %s is known not to work (%s), but its manifest has no disable_environment: not "
                     "skipped\n",
                    name, why);
            }
            continue;
        }
        setEnv(layer.disableVar.cStr(), "1");
        layerVarsSet.pushBack(layer.disableVar);
        if(why[0] && !allOff)
        {
            skippedVarsSet.pushBack(layer.disableVar);
            note("OpenXR: API layer %s skipped: %s (%s=1 set; vr_xr_skip_incompatible_layers 0 allows it)\n", name, why,
                layer.disableVar.cStr());
        }
    }
}

// "on", "off (...)": what the loader will do with it.
[[nodiscard]] za::String layerState(const ApiLayer& layer)
{
    if(layer.name.empty())
    {
        return "manifest unreadable";
    }
    if(layer.registryOff)
    {
        return "off (its registry value)";
    }
    if(envSet(layer.disableVar))
    {
        bool ours = false;
        bool skipped = false;
        for(const za::String& var : layerVarsSet)
        {
            ours = ours || var == layer.disableVar;
        }
        for(const za::String& var : skippedVarsSet)
        {
            skipped = skipped || var == layer.disableVar;
        }
        return va("off (%s set%s)", layer.disableVar.cStr(),
            skipped ? " by vr_xr_skip_incompatible_layers 1: known not to work with OpenGL"
            : ours  ? " by vr_xr_api_layers 0"
                    : "");
    }
    if(!layer.enableVar.empty() && !envSet(layer.enableVar))
    {
        return va("off (only with %s set)", layer.enableVar.cStr());
    }
    if(incompatibility(layer)[0] && !layer.disableVar.empty() && vr_xr_skip_incompatible_layers.value != 0.f)
    {
        return "ON now; skipped at each VR start (vr_xr_skip_incompatible_layers 1: known not to work with OpenGL)";
    }
    return layer.disableVar.empty() ? "ON (no switch to turn it off)" : "ON";
}

// The other programs' DLLs in the game's process, with what the known ones are: overlays and hooks that sit on the
// graphics APIs (they also load d3d11.dll and dxgi.dll before any runtime does), proxies next to the exe.
struct KnownModule
{
    const char* file; // lower case
    const char* what;
};
constexpr KnownModule knownModules[] = {
    {"rtsshooks64.dll", "RivaTuner Statistics Server / MSI Afterburner overlay"},
    {"gameoverlayrenderer64.dll", "Steam overlay"},
    {"discordhook64.dll", "Discord overlay"},
    {"graphics-hook64.dll", "OBS game capture"},
    {"reshade64.dll", "ReShade"},
    {"nvspcap64.dll", "NVIDIA overlay (ShadowPlay)"},
    {"nvcamera64.dll", "NVIDIA Ansel / filters"},
    {"ow-graphics-hook64.dll", "Overwolf"},
    {"owclient.dll", "Overwolf"},
    {"virtualdesktop.injector64.dll", "Virtual Desktop's injector"},
    {"fpsmonitorhook64.dll", "FPS Monitor overlay"},
    {"mirillishook64.dll", "Mirillis Action! capture"},
    {"bdcamvk64.dll", "Bandicam capture"},
    {"xsplit.core.hook64.dll", "XSplit capture"},
};
// Names a proxy DLL next to the exe takes (ReShade and other injectors): ours never are.
constexpr const char* proxyNames[] = {"opengl32.dll", "dxgi.dll", "d3d11.dll", "d3d9.dll", "d3d12.dll", "dinput8.dll",
    "version.dll", "winmm.dll"};

[[nodiscard]] bool startsWithNoCase(const char* text, const char* prefix)
{
    return prefix[0] && _strnicmp(text, prefix, ZA_STRLEN(prefix)) == 0;
}

// Each DLL of another program: "name (what) at path" lines for the log; the summary's "name (what)".
void foreignModules(za::Vector<za::String>& lines, za::String& summary)
{
    char windows[MAX_PATH] = "";
    GetWindowsDirectoryA(windows, sizeof(windows));
    if(windows[0] && windows[ZA_STRLEN(windows) - 1] != '\\')
    {
        q_strlcat(windows, "\\", sizeof(windows)); // "C:\Windows\" so "C:\WindowsApps\..." is not a system DLL
    }
    char exeDir[MAX_PATH] = "";
    GetModuleFileNameA(nullptr, exeDir, sizeof(exeDir));
    if(char* slash = strrchr(exeDir, '\\'))
    {
        slash[1] = 0;
    }
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, GetCurrentProcessId());
    if(snap == INVALID_HANDLE_VALUE)
    {
        return;
    }
    MODULEENTRY32 me{};
    me.dwSize = sizeof(me);
    for(BOOL ok = Module32First(snap, &me); ok; ok = Module32Next(snap, &me))
    {
        char lower[MAX_PATH];
        q_strlcpy(lower, me.szModule, sizeof(lower));
        for(char* c = lower; *c; c++)
        {
            *c = static_cast<char>(tolower(static_cast<unsigned char>(*c)));
        }
        const char* what = nullptr;
        for(const KnownModule& k : knownModules)
        {
            what = !what && !ZA_STRCMP(lower, k.file) ? k.what : what;
        }
        const bool inWindows = startsWithNoCase(me.szExePath, windows);
        const bool inGame = startsWithNoCase(me.szExePath, exeDir);
        bool proxy = false;
        for(const char* p : proxyNames)
        {
            proxy = proxy || (!inWindows && !ZA_STRCMP(lower, p));
        }
        if((!what && (inWindows || (inGame && !proxy))) || me.hModule == GetModuleHandleA(nullptr))
        {
            continue;
        }
        const char* tag = what ? what : proxy ? "a proxy of a system DLL (ReShade or another injector?)" : nullptr;
        lines.pushBack(va("%s%s%s%s at %s", me.szModule, tag ? " (" : "", tag ? tag : "", tag ? ")" : "", me.szExePath));
        if(tag && summary.size() < 600)
        {
            summary += summary.empty() ? "" : ", ";
            summary += va("%s (%s)", me.szModule, tag);
        }
    }
    CloseHandle(snap);
}
#endif

// The graphics DLLs a runtime loads that the GPU driver keeps pointers into, kept loaded for good once a runtime
// loaded them (keepGraphicsModules()). VDXR loads d3d11.dll (it renders with D3D11 and shares the game's OpenGL images
// through NVIDIA's GL/D3D interop) and unloads it with itself; NVIDIA's OpenGL driver thread then reads d3d11.dll's
// memory and the game crashes (the author's crashes of 2026-10-09 switching from VDXR: an access violation in
// nvapi64_impl.dll called from nvoglv64.dll's thread, reading the unloaded d3d11.dll's memory).
#ifdef _WIN32
constexpr const char* graphicsModules[] = {"d3d11.dll", "dxgi.dll", "d3d12.dll", "vulkan-1.dll"};
constexpr za::SizeT graphicsModuleCount = sizeof(graphicsModules) / sizeof(graphicsModules[0]);
bool graphicsModuleKept[graphicsModuleCount] = {};
#endif

// What Auto sees and the order it would try the runtimes in (vr_xr_runtime_explain; the log of each start).
void describe()
{
    const Environment env = environment();
    const za::Vector<Installed> installed = installedRuntimes(env);
    const Plan p = makePlan(env, installed);

    note("vr_xr_runtime %d (%s)%s\n", static_cast<int>(vr_xr_runtime.value), modeName(static_cast<int>(vr_xr_runtime.value)),
        env.test ? "; test environment (vr_xr_test 1)" : "");
    if(const char* pick = commandLineChoice())
    {
        note(fromCommandLine() ? "  set on the command line (+vr_xr_runtime %s): set again at every start, over the menu's choice\n"
                              : "  the command line's +vr_xr_runtime %s is set again at the next start, over the menu's choice\n",
            pick);
    }
    checkOutside();
    note("  XR_RUNTIME_JSON when the game started: %s\n", state.outside.empty() ? "not set" : state.outside.cStr());
#ifdef _WIN32
    za::String modules;
    for(za::SizeT i = 0; i < graphicsModuleCount; i++)
    {
        modules += i ? ", " : "";
        modules += graphicsModules[i];
        modules += graphicsModuleKept[i] ? " kept" : GetModuleHandleA(graphicsModules[i]) ? " loaded" : " not loaded";
    }
    note("  graphics DLLs: %s\n", modules.cStr());
    const za::Vector<ApiLayer> layers = implicitLayers();
    note("  implicit OpenXR API layers: %d installed%s\n", static_cast<int>(layers.size()),
        vr_xr_api_layers.value == 0.f ? " (vr_xr_api_layers 0: each turned off by its own variable)" : "");
    for(const ApiLayer& layer : layers)
    {
        note("    %s: %s\n", layer.name.empty() ? fileName(layer.manifest.cStr()) : layer.name.cStr(), layerState(layer).cStr());
        note("       %s\n", layer.library.empty() ? layer.manifest.cStr() : layer.library.cStr());
    }
    za::Vector<za::String> foreign;
    za::String foreignSummary;
    foreignModules(foreign, foreignSummary);
    note("  other programs' DLLs in the game: %s\n", foreign.empty() ? "none" : va("%d", static_cast<int>(foreign.size())));
    for(const za::String& line : foreign)
    {
        note("    %s\n", line.cStr());
    }
#endif
    for(const Known& k : known)
    {
        const char* running = runningProcess(env, k.kind);
        note("  %s: %s\n", k.label,
            running       ? va("%s running", running)
            : env.sandbox ? "can't tell: the Flatpak sandbox hides the host's processes (Auto: as not running)"
                          : "not running");
    }
#ifndef _WIN32
    if(env.sandbox)
    {
        const za::String devices = flatpakDevices();
        note("  Flatpak sandbox: devices %s; /dev/shm %s\n", devices.empty() ? "none listed" : devices.cStr(),
            sandboxSharesShm() ? "shared with the host"
                               : "the sandbox's own (SteamVR's IPC needs the host's: flatpak override --user "
                                 "--device=shm io.github.vittorioromeo.QuakeVR)");
        const char* logs = steamLogsAccess();
        note("  Flatpak sandbox: SteamVR's logs folder (~/.local/share/Steam/logs) %s\n",
            !ZA_STRCMP(logs, "read-only") ? "read-only (SteamVR's client needs to write it: flatpak override --user "
                                         "--filesystem=xdg-data/Steam/logs io.github.vittorioromeo.QuakeVR)"
            : !ZA_STRCMP(logs, "none")    ? "not found (no native Steam's SteamVR)"
                                       : "writable");
    }
    note("  /dev/shm, SteamVR's or OpenVR's: %s\n", vrShmEntries().cStr());
#endif
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
            i.exists              ? ""
            : !i.foreign.empty() ? va(", %s: left out", i.foreign.cStr())
                                 : ", NOT FOUND (left out)",
            idleSteamVR ? ", not running: left out (it would start SteamVR; vr_xr_runtime_fallback 2 tries it)" : "");
        note("     %s\n", i.manifest.cStr());
    }
    if(installed.empty())
    {
        note("  installed: none\n");
    }
    note("choice: %s\n", p.summary.cStr());
    for(za::SizeT i = 0; i < p.attempts.size(); i++)
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
    for(za::SizeT i = 0; i < p.attempts.size() && first < 0; i++)
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
    const za::SizeT n = fread(text, 1, sizeof(text) - 1, f);
    fclose(f);
    text[n] = 0;
    const za::String path = jsonString(text, "library_path");
    if(path.empty())
    {
        return library;
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

za::String foreignArch(const char* manifest)
{
#ifdef _WIN32
    (void)manifest;
    return za::String{};
#else
    // Its name: <name>.<arch>.json.
    const char* name = fileName(manifest);
    const za::SizeT n = ZA_STRLEN(name);
    if(n > 5 && !q_strcasecmp(name + n - 5, ".json"))
    {
        const char* end = name + n - 5;
        const char* dot = end;
        while(dot > name && dot[-1] != '.')
        {
            dot--;
        }
        if(dot > name)
        {
            const za::String suffix{dot, static_cast<za::SizeT>(end - dot)};
            for(const char* arch : loaderArches)
            {
                if(suffix == za::StringView{arch} && ZA_STRCMP(arch, ourArch) != 0)
                {
                    return za::String{va("for %s (the game is %s)", arch, ourArch)};
                }
            }
        }
    }
    // Its library's ELF header: class (byte 4: 1 32-bit, 2 64-bit) and machine (bytes 18-19, little-endian).
    const za::String library = libraryOf(manifest);
    FILE* f = library.empty() ? nullptr : fopen(library.cStr(), "rb");
    if(!f)
    {
        return za::String{};
    }
    unsigned char header[20] = {};
    const za::SizeT got = fread(header, 1, sizeof(header), f);
    fclose(f);
    if(got < sizeof(header) || header[0] != 0x7f || header[1] != 'E' || header[2] != 'L' || header[3] != 'F')
    {
        return za::String{};
    }
    const int bits = header[4] == 1 ? 32 : header[4] == 2 ? 64 : 0;
    const int machine = header[18] | (header[19] << 8);
    if(bits && bits != static_cast<int>(sizeof(void*) * 8))
    {
        return za::String{va("its library is %d-bit (the game is %d-bit): %s", bits, static_cast<int>(sizeof(void*) * 8),
            library.cStr())};
    }
    if(ourElfMachine && header[5] == 1 && machine != ourElfMachine)
    {
        return za::String{va("its library is for another CPU (ELF machine %d, the game's %d): %s", machine, ourElfMachine,
            library.cStr())};
    }
    return za::String{};
#endif
}

// vr_xr_debug's XR_LOADER_DEBUG, once set (said once).
bool loaderDebugSet = false;

#ifndef _WIN32
// Linux: the loader's (and the runtime's) stdout and stderr while it loads a runtime, caught in qvr_openxr_loader.txt
// (captureLoaderOutput .. endLoaderOutput).
struct LoaderCapture
{
    FILE* file = nullptr;
    int savedOut = -1;
    int savedErr = -1;
};
LoaderCapture loaderCapture;
// The terminal's stdout and stderr (the dups) while they are caught, -1 otherwise: VR_TerminalFd's, read by the start-up
// watchdog's thread and the crash handler (a lock-free int: signal-safe).
za::Atomic<int> terminalOut{-1};
za::Atomic<int> terminalErr{-1};
#endif

} // namespace

void loaderDebug()
{
    if(vr_xr_debug.value == 0.f || loaderDebugSet)
    {
        return;
    }
    loaderDebugSet = true;
    const char* outside = getenv("XR_LOADER_DEBUG");
    if(outside && outside[0])
    {
        Con_Printf("OpenXR: vr_xr_debug: XR_LOADER_DEBUG=%s, set outside the game, kept\n", outside);
        return;
    }
#ifdef _WIN32
    SetEnvironmentVariableA("XR_LOADER_DEBUG", "all");
    _putenv_s("XR_LOADER_DEBUG", "all");
    Con_Printf("OpenXR: vr_xr_debug: XR_LOADER_DEBUG=all (Windows: diagnostics mode catches the loader's messages)\n");
#else
    setenv("XR_LOADER_DEBUG", "all", 1);
    Con_Printf("OpenXR: vr_xr_debug: XR_LOADER_DEBUG=all: every message of the OpenXR loader goes to qvr_openxr.txt (it "
               "reads the variable at its first message: set vr_xr_debug before VR starts, or restart the game)\n");
#endif
}

const char* loaderOutputPath()
{
    return va("%s/qvr_openxr_loader.txt", com_gamedir);
}

void captureLoaderOutput(const char* call)
{
#ifndef _WIN32
    LoaderCapture& c = loaderCapture;
    if(c.file)
    {
        return;
    }
    fflush(stdout);
    fflush(stderr);
    c.file = fopen(loaderOutputPath(), "w+b");
    if(!c.file)
    {
        return;
    }
    char when[32] = "";
    const time_t now = time(nullptr);
    if(const tm* t = localtime(&now))
    {
        strftime(when, sizeof(when), "%Y-%m-%d %H:%M:%S", t);
    }
    fprintf(c.file, "== %s, %s: the OpenXR loader's and the runtime's stdout and stderr while it runs\n", call, when);
    fflush(c.file);
    c.savedOut = dup(STDOUT_FILENO);
    c.savedErr = dup(STDERR_FILENO);
    terminalOut.storeRelease(c.savedOut); // (before the redirect: the terminal stays reachable)
    terminalErr.storeRelease(c.savedErr);
    dup2(fileno(c.file), STDOUT_FILENO);
    dup2(fileno(c.file), STDERR_FILENO);
#else
    (void)call;
#endif
}

void endLoaderOutput(const char* call, bool failed)
{
#ifndef _WIN32
    LoaderCapture& c = loaderCapture;
    if(!c.file)
    {
        return;
    }
    fflush(stdout);
    fflush(stderr);
    if(c.savedOut >= 0)
    {
        dup2(c.savedOut, STDOUT_FILENO);
    }
    if(c.savedErr >= 0)
    {
        dup2(c.savedErr, STDERR_FILENO);
    }
    terminalOut.storeRelease(-1); // (fds 1 and 2 the terminal's again: the dups no longer needed)
    terminalErr.storeRelease(-1);
    if(c.savedOut >= 0)
    {
        close(c.savedOut);
    }
    if(c.savedErr >= 0)
    {
        close(c.savedErr);
    }
    fflush(c.file);
    rewind(c.file);
    char line[1024];
    char last[1024] = "";
    int shown = 0;
    while(fgets(line, sizeof(line), c.file))
    {
        line[strcspn(line, "\r\n")] = 0;
        // (With XR_LOADER_DEBUG the loader writes each error to stderr and stdout both: once. The file's own header
        // line, "== <call>, <time>: ...", not.)
        if(!line[0] || !ZA_STRCMP(line, last) || !ZA_STRNCMP(line, "== ", 3))
        {
            continue;
        }
        q_strlcpy(last, line, sizeof(last));
        logText(va("OpenXR loader output (%s): %s\n", call, line), true);
        // A failure's errors to the console too (the loader's "Error [GENERAL | ...]" lines; at most 8).
        if(failed && (ZA_STRSTR(line, "Error") || ZA_STRSTR(line, "error") || ZA_STRSTR(line, "ERROR")) && shown++ < 8)
        {
            Con_Warning("OpenXR loader: %s\n", line);
        }
    }
    fclose(c.file);
    c = LoaderCapture{};
#else
    (void)call;
    (void)failed;
#endif
}

void runtimeUnavailable(const Attempt& attempt)
{
    const bool steam = isSteamVR(attempt) || attempt.label == za::StringView{"SteamVR"};
#ifdef _WIN32
    warn("OpenXR: %s isn't available (XR_ERROR_RUNTIME_UNAVAILABLE): %s, then vr_restart (Options > Restart VR)\n",
        attempt.label.cStr(), steam ? "start SteamVR and connect the headset" : "start the runtime's app");
#else
    const Environment env = environment();
    const char* running = runningProcess(env, Kind::SteamVR);
    if(!steam)
    {
        warn("OpenXR: %s isn't available (XR_ERROR_RUNTIME_UNAVAILABLE): the loader couldn't load its library (the "
             "loader's messages: qvr_openxr.txt), or its service isn't running (monado-service, WiVRn's server): start "
             "it, then vr_restart\n",
            attempt.label.cStr());
        return;
    }
    warn("OpenXR: SteamVR isn't available (XR_ERROR_RUNTIME_UNAVAILABLE): its client (vrclient.so) didn't load or "
         "didn't reach SteamVR's server%s\n",
        running ? va(" (%s is running)", running) : env.sandbox ? "" : " (no vrserver running)");
    note("  Start SteamVR first and wait for the headset in its window, then vr_restart (Options > Restart VR).\n");
    if(env.sandbox && !sandboxSharesShm())
    {
        note("  The Flatpak sandbox has its own /dev/shm, where SteamVR's client looks for its server: run 'flatpak "
             "override --user --device=shm io.github.vittorioromeo.QuakeVR' and start the game again.\n");
    }
    const bool logsReadOnly = env.sandbox && !ZA_STRCMP(steamLogsAccess(), "read-only");
    if(logsReadOnly)
    {
        note("  The Flatpak can't write SteamVR's logs folder, which SteamVR's client needs: run 'flatpak override "
             "--user --filesystem=xdg-data/Steam/logs io.github.vittorioromeo.QuakeVR' and start the game again.\n");
    }
    note("  The loader's messages: qvr_openxr.txt (all of them with vr_xr_debug 1, set before VR starts); SteamVR's: "
         "~/.local/share/Steam/logs (vrserver.txt, vrclient_*.txt)%s.\n",
        logsReadOnly ? ", which the Flatpak only reads" : "");
    note("  Other runtimes: Monado, WiVRn (docs/vr-port/LINUX.md, \"SteamVR\").\n");
#endif
}

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
    if(!state.videoNote.empty())
    {
        logText(state.videoNote.cStr());
    }
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
    logText(text, false, true);
}

void warn(const char* format, ...)
{
    char text[2048];
    va_list args;
    va_start(args, format);
    q_vsnprintf(text, sizeof(text), format, args);
    va_end(args);
    Con_Warning("%s", text);
    logText(va("WARNING: %s", text), false, true);
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
    for(za::SizeT i = 0; i < graphicsModuleCount && vr_xr_keep_graphics_dlls.value != 0.f; i++)
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

void beforeVideoInit()
{
#ifndef _WIN32
    loaderDebug();
    auto set = [](const char* name) {
        const char* v = getenv(name);
        return v && v[0];
    };
    const char* why = nullptr;
    if(set("SDL_VIDEODRIVER") || set("SDL_VIDEO_DRIVER") || set("SDL_VIDEO_X11_FORCE_EGL") || set("SDL_VIDEO_FORCE_EGL"))
    {
        why = "the player's SDL variables kept";
    }
    else if(!set("DISPLAY"))
    {
        why = "no X server (DISPLAY not set): SDL's choice kept";
    }
    if(!why)
    {
        const Environment env = environment();
        const Plan p = makePlan(env, installedRuntimes(env));
        bool steam = false;
        for(const Attempt& a : p.attempts)
        {
            const bool loaderSteam = a.manifest.empty() && !env.active.empty() && kindOf(env.active.cStr()) == Kind::SteamVR;
            steam = steam || isSteamVR(a) || loaderSteam;
        }
        if(steam)
        {
            setenv("SDL_VIDEODRIVER", "x11", 1);
            setenv("SDL_VIDEO_DRIVER", "x11", 1);
            setenv("SDL_VIDEO_X11_FORCE_EGL", "0", 1);
            setenv("SDL_VIDEO_FORCE_EGL", "0", 1);
            why = "SDL asked for X11 with GLX (SDL_VIDEODRIVER=x11, SDL_VIDEO_X11_FORCE_EGL=0): the start may try SteamVR, "
                  "whose OpenXR binds GLX contexts only";
        }
        else
        {
            why = "SDL's choice kept (no SteamVR in the plan: an EGL context binds with XR_MNDX_egl_enable)";
        }
    }
    state.videoNote = va("OpenXR: before video init: %s\n", why);
    Con_Printf("%s", state.videoNote.cStr());
#endif
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
    setEnv("XR_RUNTIME_JSON", value);
    applyLayerSwitch(implicitLayers());
#else
    if(value)
    {
        setenv("XR_RUNTIME_JSON", value, 1);
    }
    else
    {
        unsetenv("XR_RUNTIME_JSON");
    }
#endif
    note("OpenXR: trying %s (%s): %s\n", attempt.label.cStr(), attempt.reason.cStr(),
        value ? value : "the system's active runtime");
}

za::String layersAndOverlays()
{
    za::String text;
#ifdef _WIN32
    int on = 0;
    za::String names;
    for(const ApiLayer& layer : implicitLayers())
    {
        const za::String st = layerState(layer);
        const bool active = !ZA_STRNCMP(st.cStr(), "ON", 2);
        on += active ? 1 : 0;
        if(names.size() < 400)
        {
            names += names.empty() ? "" : ", ";
            names += va("%s%s", layer.name.empty() ? fileName(layer.manifest.cStr()) : layer.name.cStr(), active ? "" : " (off)");
        }
    }
    text = va("API layers: %d on%s%s%s", on, names.empty() ? "" : " (", names.cStr(), names.empty() ? "" : ")");
    za::Vector<za::String> foreign;
    za::String overlays;
    foreignModules(foreign, overlays);
    text += va("; overlays/hooks: %s", overlays.empty() ? "none known" : overlays.cStr());
#endif
    return text;
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
        const Attempt& a = plan.attempts[static_cast<za::SizeT>(started)];
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

// The terminal's own stdout (1) or stderr (2) while the OpenXR loader's output is caught (captureLoaderOutput): the
// start-up watchdog's "still in" lines and the crash report reach it during a hang or a crash inside the call; -1 when
// nothing is caught (fds 1 and 2 are the terminal's then). Signal-safe (an atomic int's load).
extern "C" int VR_TerminalFd(int fd)
{
#ifndef _WIN32
    if(fd == 1)
    {
        return qvr::xrruntime::terminalOut.loadAcquire();
    }
    if(fd == 2)
    {
        return qvr::xrruntime::terminalErr.loadAcquire();
    }
#else
    (void)fd;
#endif
    return -1;
}
