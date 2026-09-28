// vr_profile_systems.cpp -- see vr_profile_systems.hpp.
//
// Each frame's time is kept per system and view (nanoseconds, CPU; milliseconds, GPU), then added into three sums: the
// current second (a row of the CSV, and of the ring of the last minute that vr_profile_report reads), the current half
// second (the panel shows the last two), and, for a frame over budget, the hitch log's line. A sum keeps each system's
// total and its worst frame.

#include "vr_profile_systems.hpp"
#include "vr_box3d.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_hands.hpp"
#include "vr_main.hpp"
#include "vr_profile.hpp"
#include "vr_text3d.hpp"
#include "vr_units.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>

namespace qvr::profile::systems
{
namespace
{

// ---- The systems ----

enum Group
{
    Waiting, // the headset's pacing, the swap chain, the window's swap, the frame rate cap: idle, not work
    Input,
    Server,
    Client,
    Render,  // the CPU's side of drawing: issuing the GL commands
    Misc,
    GroupCount
};
constexpr const char* groupNames[GroupCount] = {"waiting", "input", "server", "client", "render", "other"};
constexpr const char* groupShort[GroupCount] = {"idle", "in", "sv", "cl", "draw", "etc"};

struct Def
{
    const char* name; // at most 16 characters (the panel's column)
    Group group;
    const char* scopes; // the scopes' names that are this system, '|' between them
};

// The report's order within each group. A scope not named here is its parent's system.
constexpr Def defs[] = {
    {"xr wait frame", Waiting, "xr wait|xrWaitFrame|xrBeginFrame"},
    {"xr swapchain", Waiting, "xr acquire|xr release"},
    {"xr end frame", Waiting, "xr submit"},
    {"window swap", Waiting, "swap"},
    {"frame cap", Waiting, ""}, // the frame's period past _Host_Frame (host_maxfps's sleep)
    {"input/tracking", Input, "input|tracking|vr frame setup"},
    {"console cmds", Input, "commands"},
    {"quakec", Server, "quakec"},
    {"qc builtins", Server, "qc builtin"},
    {"quake physics", Server, "SV_Physics|run clients"},
    {"traces", Server, "trace"},
    {"box3d", Server, "box3d|box3d sync|box3d hands|box3d water and hits|box3d step|box3d write|rigid bodies"},
    {"vr gameplay", Server, "climb|vr hand touches|vr swim|carry2h"},
    {"server send", Server, "send"},
    {"server other", Server, "server"},
    {"net parse", Client, "parse"},
    {"cl entities", Client, "relink|temp entities|wall torches|gore|held"},
    {"vr view setup", Client, "view entities|hands"},
    {"particle sim", Client, "run particles|particle sim|particle spawn|particle lying"},
    {"sound", Client, "sound|sound mix"},
    {"client other", Client, "client read|client send"},
    {"world", Render, "world+brush|brush|mark surfaces"},
    {"models", Render, "alias|sprite models|view model|shells|blob shadows"},
    {"light setup", Render, "lights|shadow select|torch lights|flashlight beam|model ambient|ao occluders"},
    {"shadows dlight", Render, "dlight shadows|dlight casters"},
    {"shadows maplight", Render, "map light shadows|map light world"},
    {"shadows other", Render, "shadow maps|atlas clear"},
    {"particles", Render, "particles|vr particles|particle verts|particle upload|vr scene distances|sprites"},
    {"decals", Render, "decals|decal verts|gib trail"},
    {"water", Render, "water|water mesh pick"},
    {"heat haze", Render, "heat haze|haze"},
    {"sky", Render, "sky"},
    {"translucent", Render, "translucent"},
    {"env cubes", Render, "env cube|env cube build"},
    {"post-process", Render, "postprocess|bloom|upscale|resolve/warp|refraction resolve"},
    {"ui/hud/menu", Render, "ui|hud panel|2D|gadget hologram|gadget fps|gadget screen|lines|vr opaque (text3d)"},
    {"wound paint", Render, "wounds"},
    {"flashlight cord", Render, "flashlight cord|flashlight cord draw"},
    {"mirror/spectator", Render, "mirror|spectator|window view"},
    {"render other", Render, "screen|3D|eye L|eye R|scene|scene setup|setup view|hidden area"},
    {"map loading", Misc, "map spawn|map load"},
    {"memory log", Misc, "memory log"}, // vr_memstats_log's row (a minute apart)
    {"profiler", Misc, ""}, // its own frame processing (the panel's text, the files)
    {"other", Misc, ""},    // the frame's own: what no scope covers
};
constexpr int SysCount = static_cast<int>(sizeof(defs) / sizeof(defs[0]));

[[nodiscard]] constexpr int sysIndex(const char* name)
{
    for(int i = 0; i < SysCount; i++)
    {
        const char* a = defs[i].name;
        const char* b = name;
        while(*a && *a == *b)
        {
            ++a;
            ++b;
        }
        if(*a == *b)
        {
            return i;
        }
    }
    return -1;
}
constexpr int FrameCap = sysIndex("frame cap");
constexpr int Profiler = sysIndex("profiler");
constexpr int Other = sysIndex("other");
static_assert(FrameCap >= 0 && Profiler >= 0 && Other >= 0);

// The views: each scope's time is also one view's (the eyes' scopes, the window's; the rest is shared: the simulation,
// the shadow maps drawn in the left eye's setup are the left eye's).
enum View
{
    Shared,
    EyeL,
    EyeR,
    Window,
    ViewCount
};
constexpr const char* viewNames[ViewCount] = {"shared", "eye L", "eye R", "window"};

// Counts a frame.
enum Count
{
    Traces,
    HullChecks,
    DrawCalls,
    AliasDrawn,
    Box3dBodies,
    Box3dAwake,
    Box3dContacts,
    Edicts,
    CountCount
};
constexpr const char* countNames[CountCount] = {"traces", "hull traces", "draw calls", "alias models", "box3d bodies",
    "box3d awake", "box3d contacts", "edicts"};
constexpr const char* countColumns[CountCount] = {"traces", "hull_traces", "draw_calls", "alias_models", "box3d_bodies",
    "box3d_awake", "box3d_contacts", "edicts"};

[[nodiscard]] bool inList(const char* list, const char* name)
{
    const std::size_t n = std::strlen(name);
    for(const char* p = list; *p;)
    {
        const char* bar = std::strchr(p, '|');
        const std::size_t len = bar ? static_cast<std::size_t>(bar - p) : std::strlen(p);
        if(len == n && std::strncmp(p, name, n) == 0)
        {
            return true;
        }
        if(!bar)
        {
            break;
        }
        p = bar + 1;
    }
    return false;
}

// ---- This frame ----

std::int64_t frameCpu[SysCount]{};
std::int64_t frameViewCpu[ViewCount]{};
double frameGpu[SysCount]{};
double frameViewGpu[ViewCount]{};

// ---- Sums ----

struct Sum
{
    double seconds{0.0};
    int frames{0};
    int gpuFrames{0};
    int longFrames{0}; // over the long-frame limit: left out
    int slowFrames{0}; // over 1.25 budgets: a refresh missed
    int hitches{0};    // over vr_profile_hitch budgets (long ones too)
    double budgetMs{0.0};
    double periodSum{0.0}, periodMax{0.0};
    double busySum{0.0}, busyMax{0.0}; // the period less the waiting
    double cpuSum[SysCount]{}, cpuMax[SysCount]{};
    double groupSum[GroupCount]{}, groupMax[GroupCount]{};
    double viewSum[ViewCount]{}, viewMax[ViewCount]{};
    double gpuTotalSum{0.0}, gpuTotalMax{0.0};
    double gpuQueries{0.0}; // the timer queries read (vr_profile_gpu)
    double gpuSum[SysCount]{}, gpuMax[SysCount]{};
    double gpuGroupSum[GroupCount]{}, gpuGroupMax[GroupCount]{};
    double gpuViewSum[ViewCount]{}, gpuViewMax[ViewCount]{};
    bool gpuSeen[SysCount]{};
    double countSum[CountCount]{}, countMax[CountCount]{};
};

void addMax(double& sum, double& max, double v)
{
    sum += v;
    max = std::max(max, v);
}

void merge(Sum& into, const Sum& s)
{
    into.seconds += s.seconds;
    into.frames += s.frames;
    into.gpuFrames += s.gpuFrames;
    into.longFrames += s.longFrames;
    into.slowFrames += s.slowFrames;
    into.hitches += s.hitches;
    into.gpuQueries += s.gpuQueries;
    into.budgetMs = s.budgetMs > 0.0 ? s.budgetMs : into.budgetMs;
    const auto both = [](double& sum, double& max, double s2, double m2) {
        sum += s2;
        max = std::max(max, m2);
    };
    both(into.periodSum, into.periodMax, s.periodSum, s.periodMax);
    both(into.busySum, into.busyMax, s.busySum, s.busyMax);
    both(into.gpuTotalSum, into.gpuTotalMax, s.gpuTotalSum, s.gpuTotalMax);
    for(int i = 0; i < SysCount; i++)
    {
        both(into.cpuSum[i], into.cpuMax[i], s.cpuSum[i], s.cpuMax[i]);
        both(into.gpuSum[i], into.gpuMax[i], s.gpuSum[i], s.gpuMax[i]);
        into.gpuSeen[i] = into.gpuSeen[i] || s.gpuSeen[i];
    }
    for(int g = 0; g < GroupCount; g++)
    {
        both(into.groupSum[g], into.groupMax[g], s.groupSum[g], s.groupMax[g]);
        both(into.gpuGroupSum[g], into.gpuGroupMax[g], s.gpuGroupSum[g], s.gpuGroupMax[g]);
    }
    for(int v = 0; v < ViewCount; v++)
    {
        both(into.viewSum[v], into.viewMax[v], s.viewSum[v], s.viewMax[v]);
        both(into.gpuViewSum[v], into.gpuViewMax[v], s.gpuViewSum[v], s.gpuViewMax[v]);
    }
    for(int c = 0; c < CountCount; c++)
    {
        both(into.countSum[c], into.countMax[c], s.countSum[c], s.countMax[c]);
    }
}

// QuakeC's costliest functions over a second, by the instructions they ran themselves (the VM's own count per function,
// dfunction_t::profile: kept anyway). Their share of the instructions is about their share of "quakec"'s time.
struct QcFunction
{
    std::string name;
    double instructions;
};

struct Second
{
    Sum sum;
    std::string map;
    double qcInstructions{0.0};
    std::vector<QcFunction> qcTop; // the costliest few
};

constexpr int ringSize = 120; // seconds kept for vr_profile_report
Second ring[ringSize];
int ringCount = 0; // filled so far
int ringNext = 0;

Sum second;       // the current second's
Sum half;         // the current half second's (the panel)
Sum lastHalf;     // the one before
std::int64_t secondStart = 0;
std::int64_t halfStart = 0;
std::int64_t captureStart = 0; // the CSV capture's first second's start
std::string mapNow = "none";

// QuakeC's per-function counts when last read (sv.qcvm's; restarted when its progs change).
std::vector<int> qcLast;
const void* qcProgs = nullptr;

// ---- Files ----

// Kept open while in use (a row flushed at a time): opening and closing a file each second cost up to 2 ms, a hitch of
// its own.
std::string csvPath;   // this capture's (empty: none)
FILE* csvFile = nullptr;
std::string hitchPath; // this session's hitch log (made at the first hitch)
FILE* hitchFile = nullptr;
int hitchLines = 0;    // console lines this second (at most a few a second: the file has every one)
int hitchQuiet = 0;    // those not printed

std::string panelText;
std::vector<text3d::OverlayBar> panelBars;
float panelYaw = 0.f; // vr_profile_overlay 2: the panel's direction, easing after the head's
bool panelPlaced = false;

void appendf(std::string& out, const char* fmt, ...)
{
    char buf[512];
    va_list args;
    va_start(args, fmt);
    q_vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    out += buf;
}

[[nodiscard]] std::string stamp(const char* fmt)
{
    const std::time_t now = std::time(nullptr);
    char buf[64];
    std::strftime(buf, sizeof(buf), fmt, std::localtime(&now));
    return buf;
}

// A system's name as a CSV column: letters and digits, '_' for the rest.
[[nodiscard]] std::string column(const char* name)
{
    std::string c;
    for(const char* p = name; *p; ++p)
    {
        const bool alnum = (*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9');
        if(alnum)
        {
            c += *p;
        }
        else if(!c.empty() && c.back() != '_')
        {
            c += '_';
        }
    }
    while(!c.empty() && c.back() == '_')
    {
        c.pop_back();
    }
    return c;
}

// The frame budget: the headset's refresh period, else (the mock, which does not tell it) 90 Hz; the flat screen's
// host_maxfps.
[[nodiscard]] double budgetMs()
{
    const double display = displayPeriodMs();
    if(display > 0.0)
    {
        return display;
    }
    static const cvar_t* maxfps = Cvar_FindVar("host_maxfps");
    if(!vrActive() && maxfps && maxfps->value > 0.f)
    {
        return 1000.0 / maxfps->value;
    }
    return 1000.0 / 90.0;
}

// ---- QuakeC's costliest functions ----

void readQuakeC(Second& s)
{
    s.qcInstructions = 0.0;
    s.qcTop.clear();
    if(!sv.active || !sv.qcvm.progs || !sv.qcvm.functions)
    {
        qcProgs = nullptr;
        return;
    }
    const int count = sv.qcvm.progs->numfunctions;
    const bool fresh = qcProgs != sv.qcvm.progs || static_cast<int>(qcLast.size()) != count;
    if(fresh)
    {
        qcLast.assign(count, 0);
        qcProgs = sv.qcvm.progs;
    }
    std::vector<std::pair<int, int>> ran; // (instructions, function)
    for(int i = 1; i < count; i++)
    {
        const int now = sv.qcvm.functions[i].profile;
        int delta = now - qcLast[i];
        if(delta < 0)
        {
            delta = now; // the profile command cleared it
        }
        qcLast[i] = now;
        if(delta > 0 && !fresh)
        {
            s.qcInstructions += delta;
            ran.emplace_back(delta, i);
        }
    }
    constexpr std::size_t keep = 8;
    const std::size_t n = std::min(keep, ran.size());
    std::partial_sort(ran.begin(), ran.begin() + static_cast<std::ptrdiff_t>(n), ran.end(),
        [](const auto& a, const auto& b) { return a.first > b.first; });
    qcvm_t* old = nullptr;
    PR_PushQCVM(&sv.qcvm, &old);
    for(std::size_t k = 0; k < n; k++)
    {
        s.qcTop.push_back({PR_GetString(sv.qcvm.functions[ran[k].second].s_name), static_cast<double>(ran[k].first)});
    }
    PR_PopQCVM(old);
}

// ---- The CSV capture (vr_profile_csv) ----

void csvHeader(FILE* f)
{
    std::fprintf(f, "# Quake VR profile by system (vr_profile_csv), %s: a row a second; ms a frame, averaged over its "
                    "frames (max: its worst frame); each system's own time, so they add up to period_ms; gpu_*: the GPU's "
                    "time; counts a frame\n",
        stamp("%Y-%m-%d %H:%M:%S").c_str());
    std::fprintf(f, "time_s,clock,map,frames,fps,period_ms,period_max_ms,budget_ms,slow_frames,long_frames,hitches,busy_ms,"
                    "busy_max_ms,gpu_ms,gpu_max_ms");
    for(const char* g : groupNames)
    {
        std::fprintf(f, ",group_%s_ms", column(g).c_str());
    }
    for(const Def& d : defs)
    {
        std::fprintf(f, ",%s_ms", column(d.name).c_str());
    }
    for(const Def& d : defs)
    {
        std::fprintf(f, ",%s_max_ms", column(d.name).c_str());
    }
    for(const Def& d : defs)
    {
        std::fprintf(f, ",gpu_%s_ms", column(d.name).c_str());
    }
    for(const char* v : viewNames)
    {
        std::fprintf(f, ",view_%s_ms,view_%s_gpu_ms", column(v).c_str(), column(v).c_str());
    }
    for(const char* c : countColumns)
    {
        std::fprintf(f, ",%s,%s_max", c, c);
    }
    std::fprintf(f, ",qc_top\n");
}

void csvRow(const Second& s, double time)
{
    if(csvPath.empty())
    {
        const std::string dir = std::string{com_gamedir} + "/profile";
        Sys_mkdir(dir.c_str());
        csvPath = dir + "/systems_" + stamp("%Y-%m-%d_%H-%M-%S") + ".csv";
        csvFile = std::fopen(csvPath.c_str(), "w");
        if(!csvFile)
        {
            Con_Warning("vr_profile_csv: can't write %s\n", csvPath.c_str());
            return;
        }
        csvHeader(csvFile);
        Con_Printf("vr_profile_csv: writing %s\n", csvPath.c_str());
    }
    FILE* f = csvFile;
    if(!f)
    {
        return;
    }
    const Sum& m = s.sum;
    const double frames = std::max(m.frames, 1);
    const double gpuFrames = std::max(m.gpuFrames, 1);
    const double period = m.periodSum / frames;
    std::fprintf(f, "%.2f,%s,%s,%d,%.1f,%.3f,%.2f,%.3f,%d,%d,%d,%.3f,%.2f,%.3f,%.2f", time, stamp("%H:%M:%S").c_str(),
        s.map.c_str(), m.frames, period > 0.0 ? 1000.0 / period : 0.0, period, m.periodMax, m.budgetMs, m.slowFrames,
        m.longFrames, m.hitches, m.busySum / frames, m.busyMax, m.gpuTotalSum / gpuFrames, m.gpuTotalMax);
    for(int g = 0; g < GroupCount; g++)
    {
        std::fprintf(f, ",%.3f", m.groupSum[g] / frames);
    }
    for(int i = 0; i < SysCount; i++)
    {
        std::fprintf(f, ",%.3f", m.cpuSum[i] / frames);
    }
    for(int i = 0; i < SysCount; i++)
    {
        std::fprintf(f, ",%.3f", m.cpuMax[i]);
    }
    for(int i = 0; i < SysCount; i++)
    {
        std::fprintf(f, ",%.3f", m.gpuSum[i] / gpuFrames);
    }
    for(int v = 0; v < ViewCount; v++)
    {
        std::fprintf(f, ",%.3f,%.3f", m.viewSum[v] / frames, m.gpuViewSum[v] / gpuFrames);
    }
    for(int c = 0; c < CountCount; c++)
    {
        std::fprintf(f, ",%.1f,%.0f", m.countSum[c] / frames, m.countMax[c]);
    }
    std::string top;
    for(std::size_t k = 0; k < s.qcTop.size() && k < 5; k++)
    {
        appendf(top, "%s%s %.0f%%", top.empty() ? "" : " ", s.qcTop[k].name.c_str(),
            100.0 * s.qcTop[k].instructions / std::max(s.qcInstructions, 1.0));
    }
    std::fprintf(f, ",%s\n", top.c_str());
    std::fflush(f);
}

void closeCsv()
{
    if(csvFile)
    {
        std::fclose(csvFile);
        csvFile = nullptr;
    }
    if(!csvPath.empty())
    {
        Con_Printf("vr_profile_csv: closed %s\n", csvPath.c_str());
        csvPath.clear();
    }
}

// ---- The hitch log (vr_profile_hitch) ----

void hitch(double periodMs, double hostMs, double budget, const int counts[CountCount], const std::string& scopes)
{
    std::vector<std::pair<double, int>> parts;
    for(int i = 0; i < SysCount; i++)
    {
        const double ms = static_cast<double>(frameCpu[i]) / 1e6;
        if(ms >= 0.05)
        {
            parts.emplace_back(ms, i);
        }
    }
    std::sort(parts.begin(), parts.end(), [](const auto& a, const auto& b) { return a.first > b.first; });

    if(hitchLines < 5)
    {
        std::string line;
        for(std::size_t k = 0; k < parts.size() && k < 5; k++)
        {
            appendf(line, "%s%s %.1f", k ? ", " : "", defs[parts[k].second].name, parts[k].first);
        }
        Con_Printf("vr_profile: hitch %.1f ms (%.1f budgets), %s: %s\n", periodMs, periodMs / budget, mapNow.c_str(),
            line.c_str());
        if(!scopes.empty())
        {
            Con_Printf("  scopes: %s\n", scopes.c_str());
        }
        ++hitchLines;
    }
    else
    {
        ++hitchQuiet;
    }

    const bool created = hitchPath.empty();
    if(created)
    {
        const std::string dir = std::string{com_gamedir} + "/profile";
        Sys_mkdir(dir.c_str());
        hitchPath = dir + "/hitches_" + stamp("%Y-%m-%d_%H-%M-%S") + ".csv";
    }
    if(!hitchFile)
    {
        hitchFile = std::fopen(hitchPath.c_str(), "a"); // (again after a stop: the session's one file)
    }
    FILE* f = hitchFile;
    if(!f)
    {
        return;
    }
    if(created)
    {
        std::fprintf(f, "# Quake VR hitches (vr_profile_hitch): frames over %.2f budgets, each with its systems' own time "
                        "(ms), which add up to period_ms\nclock,map,period_ms,budget_ms,host_ms",
            vr_profile_hitch.value);
        for(const Def& d : defs)
        {
            std::fprintf(f, ",%s_ms", column(d.name).c_str());
        }
        for(const char* c : countColumns)
        {
            std::fprintf(f, ",%s", c);
        }
        std::fprintf(f, ",scopes\n");
    }
    std::fprintf(f, "%s,%s,%.2f,%.2f,%.2f", stamp("%H:%M:%S").c_str(), mapNow.c_str(), periodMs, budget, hostMs);
    for(int i = 0; i < SysCount; i++)
    {
        std::fprintf(f, ",%.3f", static_cast<double>(frameCpu[i]) / 1e6);
    }
    for(int c = 0; c < CountCount; c++)
    {
        std::fprintf(f, ",%d", counts[c]);
    }
    std::fprintf(f, ",\"%s\"\n", scopes.c_str()); // (quoted: commas in it)
    std::fflush(f);
}

// ---- Reading the sums ----

// The last `seconds` seconds (the ring's, and the current second's so far).
[[nodiscard]] Sum lastSeconds(double seconds, std::string& map, double& qcInstructions, std::vector<QcFunction>& qcTop)
{
    Sum s;
    Sum current = second; // this second so far
    current.seconds = std::max(0.0, nowSeconds() - static_cast<double>(secondStart) / 1e9);
    merge(s, current);
    map = mapNow;
    qcInstructions = 0.0;
    std::vector<QcFunction> all;
    for(int k = 0; k < ringCount && s.seconds < seconds - 0.25; k++)
    {
        const Second& r = ring[(ringNext - 1 - k + ringSize) % ringSize];
        merge(s, r.sum);
        qcInstructions += r.qcInstructions;
        for(const QcFunction& q : r.qcTop)
        {
            auto it = std::find_if(all.begin(), all.end(), [&](const QcFunction& a) { return a.name == q.name; });
            if(it != all.end())
            {
                it->instructions += q.instructions;
            }
            else
            {
                all.push_back(q);
            }
        }
    }
    std::sort(all.begin(), all.end(), [](const QcFunction& a, const QcFunction& b) { return a.instructions > b.instructions; });
    if(all.size() > 6)
    {
        all.resize(6);
    }
    qcTop = std::move(all);
    return s;
}

struct Row
{
    int index;
    double avg;
};

// The systems of a group (or all, group -1), costliest first, down to `least` ms (or their max to 10 times that).
[[nodiscard]] std::vector<Row> rows(const Sum& s, int group, bool gpuSide, double least)
{
    const double frames = std::max(gpuSide ? s.gpuFrames : s.frames, 1);
    std::vector<Row> out;
    for(int i = 0; i < SysCount; i++)
    {
        if(group >= 0 && defs[i].group != group)
        {
            continue;
        }
        const double avg = (gpuSide ? s.gpuSum[i] : s.cpuSum[i]) / frames;
        const double max = gpuSide ? s.gpuMax[i] : s.cpuMax[i];
        if(avg >= least || max >= 10.0 * least)
        {
            out.push_back({i, avg});
        }
    }
    std::sort(out.begin(), out.end(), [](const Row& a, const Row& b) { return a.avg > b.avg; });
    return out;
}

void report_f()
{
    if(!active)
    {
        Con_Printf("vr_profile_report: not collecting (vr_profile 1, vr_profile_overlay 1 or vr_profile_csv 1)\n");
        return;
    }
    const double seconds = Cmd_Argc() > 1 ? std::max(0.5, std::atof(Cmd_Argv(1))) : 5.0;
    std::string map;
    double qcInstructions = 0.0;
    std::vector<QcFunction> qcTop;
    const Sum s = lastSeconds(seconds, map, qcInstructions, qcTop);
    if(s.frames == 0)
    {
        Con_Printf("vr_profile_report: no frames yet\n");
        return;
    }
    const double frames = s.frames;
    const double gpuFrames = std::max(s.gpuFrames, 1);
    const double period = s.periodSum / frames;
    Con_Printf("vr_profile_report: %s, the last %.1f s: %d frames, %.2f ms apart (%.1f fps; worst %.2f), budget %.2f ms; "
               "%d missed a refresh, %d hitches, %d long (over 250 ms, left out)\n",
        map.c_str(), s.seconds, s.frames, period, period > 0.0 ? 1000.0 / period : 0.0, s.periodMax, s.budgetMs,
        s.slowFrames, s.hitches, s.longFrames);
    Con_Printf("%-22s %7s %7s %6s %7s %7s\n", "ms a frame", "cpu", "max", "share", "gpu", "max");
    Con_Printf("%-22s %7.3f %7.2f %5.1f%% %7.3f %7.2f\n", "frame", period, s.periodMax, 100.0, s.gpuTotalSum / gpuFrames,
        s.gpuTotalMax);
    double total = 0.0;
    for(int g = 0; g < GroupCount; g++)
    {
        const double avg = s.groupSum[g] / frames;
        total += avg;
        Con_Printf("%-22s %7.3f %7.2f %5.1f%% %7.3f %7.2f\n", groupNames[g], avg, s.groupMax[g],
            period > 0.0 ? 100.0 * avg / period : 0.0, s.gpuGroupSum[g] / gpuFrames, s.gpuGroupMax[g]);
        for(const Row& r : rows(s, g, false, 0.005))
        {
            char name[40];
            q_snprintf(name, sizeof(name), "  %s", defs[r.index].name);
            if(s.gpuSeen[r.index])
            {
                Con_Printf("%-22s %7.3f %7.2f %5.1f%% %7.3f %7.2f\n", name, r.avg, s.cpuMax[r.index],
                    period > 0.0 ? 100.0 * r.avg / period : 0.0, s.gpuSum[r.index] / gpuFrames, s.gpuMax[r.index]);
            }
            else
            {
                Con_Printf("%-22s %7.3f %7.2f %5.1f%%\n", name, r.avg, s.cpuMax[r.index],
                    period > 0.0 ? 100.0 * r.avg / period : 0.0);
            }
        }
        // GPU-only rows (a system with GPU time and next to no CPU time).
        for(const Row& r : rows(s, g, true, 0.005))
        {
            if(s.cpuSum[r.index] / frames < 0.005 && s.cpuMax[r.index] < 0.05)
            {
                char name[40];
                q_snprintf(name, sizeof(name), "  %s", defs[r.index].name);
                Con_Printf("%-22s %7s %7s %6s %7.3f %7.2f\n", name, "", "", "", r.avg, s.gpuMax[r.index]);
            }
        }
    }
    Con_Printf("the parts add up to %.3f ms (the frame: %.3f); CPU busy (less waiting) %.3f, worst %.2f\n", total, period,
        s.busySum / frames, s.busyMax);
    Con_Printf("views (the rest is shared):");
    for(int v = EyeL; v < ViewCount; v++)
    {
        Con_Printf("  %s %.3f (gpu %.3f)", viewNames[v], s.viewSum[v] / frames, s.gpuViewSum[v] / gpuFrames);
    }
    Con_Printf("\ncounts a frame (avg/max):");
    for(int c = 0; c < CountCount; c++)
    {
        Con_Printf(" %s %.0f/%.0f%s", countNames[c], s.countSum[c] / frames, s.countMax[c], c + 1 < CountCount ? "," : "\n");
    }
    if(qcInstructions > 0.0 && !qcTop.empty())
    {
        const double qcMs = s.cpuSum[sysIndex("quakec")] / frames;
        Con_Printf("quakec by its own instructions (%.0f a frame; ~ms of quakec's %.3f):", qcInstructions / frames, qcMs);
        for(std::size_t k = 0; k < qcTop.size(); k++)
        {
            const double share = qcTop[k].instructions / qcInstructions;
            Con_Printf(" %s %.0f%% (~%.3f)%s", qcTop[k].name.c_str(), 100.0 * share, share * qcMs,
                k + 1 < qcTop.size() ? "," : "\n");
        }
    }
    if(vr_profile_gpu.value == 0.f)
    {
        Con_Printf("(no GPU times: vr_profile_gpu 0)\n");
    }
    else
    {
        Con_Printf("GPU: %d frames timed, one in %d (vr_profile_gpu), %.0f timer queries each "
                   "(vr_profile_gpu 0: none, the CPU's times alone)\n",
            s.gpuFrames, static_cast<int>(vr_profile_gpu.value), s.gpuQueries / gpuFrames);
    }
    if(vr_profile_detail.value < 2.f)
    {
        Con_Printf("(traces and builtins are timed as their callers' time: vr_profile_detail 2 times them apart)\n");
    }
}

// ---- The panel ----

constexpr float barCells = 12.f; // the bar's track: the frame budget
constexpr int barColumn = 29;    // after "%-16.16s %5.2f %5.2f "

// A table row with its bar: a system's average (the bar), its worst frame.
void panelRow(std::vector<std::string>& lines, const char* name, double avg, double max, double budget, glm::vec4 color,
    bool waiting)
{
    std::string l;
    appendf(l, "%-16.16s %5.2f %5.2f ", name, avg, max);
    l.append(static_cast<std::size_t>(barCells) + 1, ' ');
    const float cells = static_cast<float>(avg / budget) * barCells;
    if(avg > budget && !waiting)
    {
        color = glm::vec4{1.f, 0.3f, 0.25f, 0.9f}; // work over the whole budget by itself
    }
    panelBars.push_back({static_cast<int>(lines.size()), static_cast<float>(barColumn), cells, barCells, color});
    lines.push_back(std::move(l));
}

void buildPanel()
{
    Sum s;
    merge(s, lastHalf);
    merge(s, half);
    panelText.clear();
    panelBars.clear();
    if(s.frames == 0)
    {
        return;
    }
    const double frames = s.frames;
    const double gpuFrames = std::max(s.gpuFrames, 1);
    const double period = s.periodSum / frames;
    const double budget = s.budgetMs > 0.0 ? s.budgetMs : 1000.0 / 90.0;
    std::vector<std::string> lines;
    std::string l;
    appendf(l, "PROFILE %-12.12s %5.1f fps %6.2f ms, worst %6.2f", mapNow.c_str(), period > 0.0 ? 1000.0 / period : 0.0,
        period, s.periodMax);
    lines.push_back(l);
    l.clear();
    appendf(l, "budget %5.2f   CPU busy %5.2f   GPU %5.2f", budget, s.busySum / frames, s.gpuTotalSum / gpuFrames);
    lines.push_back(l);
    l.clear();
    for(int g = 0; g < GroupCount; g++)
    {
        appendf(l, "%s%s %.2f", g ? "  " : "", groupShort[g], s.groupSum[g] / frames);
    }
    lines.push_back(l);
    lines.emplace_back("CPU               avg   max  (bar: the budget)");
    constexpr glm::vec4 cpuColor{0.45f, 0.8f, 1.f, 0.85f}, waitColor{0.55f, 0.55f, 0.6f, 0.7f},
        gpuColor{1.f, 0.72f, 0.3f, 0.85f};
    int shown = 0;
    for(const Row& r : rows(s, -1, false, 0.01))
    {
        if(++shown > 13)
        {
            break;
        }
        const bool waiting = defs[r.index].group == Waiting;
        panelRow(lines, defs[r.index].name, r.avg, s.cpuMax[r.index], budget, waiting ? waitColor : cpuColor, waiting);
    }
    lines.emplace_back("GPU               avg   max");
    shown = 0;
    for(const Row& r : rows(s, -1, true, 0.01))
    {
        if(++shown > 7)
        {
            break;
        }
        panelRow(lines, defs[r.index].name, r.avg, s.gpuMax[r.index], budget, gpuColor, false);
    }
    l.clear();
    appendf(l, "traces %.0f  draw calls %.0f  models %.0f", s.countSum[Traces] / frames, s.countSum[DrawCalls] / frames,
        s.countSum[AliasDrawn] / frames);
    lines.push_back(l);
    l.clear();
    appendf(l, "box3d %.0f (%.0f awake, %.0f contacts)  edicts %.0f", s.countSum[Box3dBodies] / frames,
        s.countSum[Box3dAwake] / frames, s.countSum[Box3dContacts] / frames, s.countSum[Edicts] / frames);
    lines.push_back(l);
    if(s.hitches || s.slowFrames)
    {
        l.clear();
        appendf(l, "hitches %d  missed refreshes %d", s.hitches, s.slowFrames);
        lines.push_back(l);
    }

    // The lines are drawn centred: padded to one width, they line up.
    std::size_t width = 0;
    for(const std::string& line : lines)
    {
        width = std::max(width, line.size());
    }
    for(const std::string& line : lines)
    {
        panelText += line;
        panelText.append(width - line.size(), ' ');
        panelText += '\n';
    }
}

// ---- Commands ----

void csvToggle_f()
{
    Cvar_SetValueQuick(&vr_profile_csv, vr_profile_csv.value != 0.f ? 0.f : 1.f);
    Con_Printf("vr_profile_csv %s\n", vr_profile_csv.value != 0.f ? "on: a row a second into quakevr/profile/systems_*.csv"
                                                                  : "off");
}

} // namespace

void classify(const char* name, int parentSystem, int parentView, int& system, int& view)
{
    system = parentSystem;
    for(int i = 0; i < SysCount; i++)
    {
        if(defs[i].scopes[0] && inList(defs[i].scopes, name))
        {
            system = i;
            break;
        }
    }
    view = parentView;
    if(std::strcmp(name, "eye L") == 0)
    {
        view = EyeL;
    }
    else if(std::strcmp(name, "eye R") == 0)
    {
        view = EyeR;
    }
    else if(inList("mirror|spectator|window view", name))
    {
        view = Window;
    }
}

int rootSystem()
{
    return Other;
}

int rootView()
{
    return Shared;
}

void cpu(int system, int view, std::int64_t ns)
{
    frameCpu[system] += ns;
    frameViewCpu[view] += ns;
}

void gpu(int system, int view, double ms)
{
    frameGpu[system] += ms;
    frameViewGpu[view] += ms;
}

void gpuFrameDone(int queries)
{
    double total = 0.0;
    double groups[GroupCount]{};
    for(int i = 0; i < SysCount; i++)
    {
        total += frameGpu[i];
        groups[defs[i].group] += frameGpu[i];
    }
    for(Sum* s : {&second, &half})
    {
        ++s->gpuFrames;
        s->gpuQueries += queries;
        addMax(s->gpuTotalSum, s->gpuTotalMax, total);
        for(int i = 0; i < SysCount; i++)
        {
            if(frameGpu[i] != 0.0)
            {
                addMax(s->gpuSum[i], s->gpuMax[i], frameGpu[i]);
                s->gpuSeen[i] = true;
            }
        }
        for(int g = 0; g < GroupCount; g++)
        {
            addMax(s->gpuGroupSum[g], s->gpuGroupMax[g], groups[g]);
        }
        for(int v = 0; v < ViewCount; v++)
        {
            addMax(s->gpuViewSum[v], s->gpuViewMax[v], frameViewGpu[v]);
        }
    }
    std::fill(std::begin(frameGpu), std::end(frameGpu), 0.0);
    std::fill(std::begin(frameViewGpu), std::end(frameViewGpu), 0.0);
}

void profilerTime(std::int64_t ns)
{
    cpu(Profiler, Shared, ns);
    cpu(Other, Shared, -ns);
}

double hitchMs()
{
    return vr_profile_hitch.value > 0.f ? vr_profile_hitch.value * budgetMs() : 0.0;
}

void frameEnd(std::int64_t now, std::int64_t periodNs, std::int64_t hostNs, const Counts& counts, double longMs,
    const char* map, const std::string& scopes)
{
    mapNow = map;
    // The frame's own time (the root scope's), and the idle time after it.
    cpu(Other, Shared, hostNs);
    cpu(FrameCap, Shared, std::max<std::int64_t>(0, periodNs - hostNs));

    int n[CountCount]{counts.traces, counts.hullChecks, counts.drawCalls, counts.aliasDrawn, 0, 0, 0, 0};
    box3d::profileCounts(n[Box3dBodies], n[Box3dAwake], n[Box3dContacts]);
    n[Edicts] = sv.active ? dev_stats.edicts : 0;

    const double period = static_cast<double>(periodNs) / 1e6;
    const double budget = budgetMs();
    // The frame rate cap's sleep (host_maxfps's, after _Host_Frame: the flat screen's and the mock's; the headset's frames
    // are paced by the runtime, in the frame) is left out: Windows' timer oversleeping it is not the game's doing.
    const double host = static_cast<double>(hostNs) / 1e6;
    const bool isHitch = vr_profile_hitch.value > 0.f && host > vr_profile_hitch.value * budget;
    if(isHitch)
    {
        hitch(period, host, budget, n, scopes);
    }

    const bool keep = period <= longMs;
    double groups[GroupCount]{};
    for(int i = 0; i < SysCount; i++)
    {
        groups[defs[i].group] += static_cast<double>(frameCpu[i]) / 1e6;
    }
    for(Sum* s : {&second, &half})
    {
        s->budgetMs = budget;
        s->hitches += isHitch ? 1 : 0;
        if(!keep)
        {
            ++s->longFrames;
            continue;
        }
        ++s->frames;
        s->slowFrames += period > 1.25 * budget ? 1 : 0;
        addMax(s->periodSum, s->periodMax, period);
        addMax(s->busySum, s->busyMax, period - groups[Waiting]);
        for(int i = 0; i < SysCount; i++)
        {
            addMax(s->cpuSum[i], s->cpuMax[i], static_cast<double>(frameCpu[i]) / 1e6);
        }
        for(int g = 0; g < GroupCount; g++)
        {
            addMax(s->groupSum[g], s->groupMax[g], groups[g]);
        }
        for(int v = 0; v < ViewCount; v++)
        {
            addMax(s->viewSum[v], s->viewMax[v], static_cast<double>(frameViewCpu[v]) / 1e6);
        }
        for(int c = 0; c < CountCount; c++)
        {
            addMax(s->countSum[c], s->countMax[c], n[c]);
        }
    }
    std::fill(std::begin(frameCpu), std::end(frameCpu), std::int64_t{0});
    std::fill(std::begin(frameViewCpu), std::end(frameViewCpu), std::int64_t{0});

    // A second's end: into the ring (and a row of the CSV).
    if(now - secondStart >= 1'000'000'000)
    {
        second.seconds = static_cast<double>(now - secondStart) / 1e9;
        Second& r = ring[ringNext];
        r.sum = second;
        r.map = mapNow;
        readQuakeC(r);
        if(vr_profile_csv.value != 0.f)
        {
            if(csvPath.empty())
            {
                captureStart = secondStart; // a new capture: its time from this second's start
            }
            csvRow(r, static_cast<double>(now - captureStart) / 1e9);
        }
        else
        {
            closeCsv();
        }
        ringNext = (ringNext + 1) % ringSize;
        ringCount = std::min(ringCount + 1, ringSize);
        second = Sum{};
        secondStart = now;
        if(hitchQuiet)
        {
            Con_Printf("vr_profile: %d more hitches (in %s)\n", hitchQuiet, hitchPath.c_str());
        }
        hitchLines = hitchQuiet = 0;
    }
    // Half a second's end: the panel's text.
    if(now - halfStart >= 500'000'000)
    {
        half.seconds = static_cast<double>(now - halfStart) / 1e9;
        if(vr_profile_overlay.value != 0.f || vr_profile.value >= 2.f)
        {
            buildPanel();
        }
        lastHalf = half;
        half = Sum{};
        halfStart = now;
    }
}

void start(std::int64_t now)
{
    second = half = lastHalf = Sum{};
    secondStart = halfStart = now;
    ringCount = ringNext = 0;
    std::fill(std::begin(frameCpu), std::end(frameCpu), std::int64_t{0});
    std::fill(std::begin(frameViewCpu), std::end(frameViewCpu), std::int64_t{0});
    std::fill(std::begin(frameGpu), std::end(frameGpu), 0.0);
    std::fill(std::begin(frameViewGpu), std::end(frameViewGpu), 0.0);
    qcProgs = nullptr;
    panelText.clear();
    panelBars.clear();
    panelPlaced = false;
}

void stop()
{
    closeCsv();
    if(hitchFile)
    {
        std::fclose(hitchFile);
        hitchFile = nullptr;
    }
    panelText.clear();
    panelBars.clear();
}

void init()
{
    Cmd_AddCommand("vr_profile_report", report_f);
    Cmd_AddCommand("vr_profile_csv_toggle", csvToggle_f);
}

void overlay()
{
    const int mode = vr_profile_overlay.value != 0.f ? static_cast<int>(vr_profile_overlay.value)
                                                     : (vr_profile.value >= 2.f ? 1 : 0);
    if(!active || mode == 0 || panelText.empty())
    {
        panelPlaced = false;
        return;
    }
    const hands::State& s = hands::current();
    if(!s.valid)
    {
        return;
    }
    const float m2u = units::metresToUnits();
    const auto lineCount = static_cast<float>(std::count(panelText.begin(), panelText.end(), '\n'));
    if(mode == 1)
    {
        // Over the wrist gadget's hand, facing the head: 4.5 mm characters (at 45 cm, about the gadget's).
        const float charSize = 0.0045f * m2u;
        const glm::vec3 at = s.pos[vr_gadget_hand.value != 0.f ? HAND_MAIN : HAND_OFF] +
                             glm::vec3{0.f, 0.f, 0.06f * m2u + 0.5f * lineCount * charSize};
        const glm::vec3 d = at - s.head;
        const float yaw = std::atan2(d.y, d.x) * 180.f / static_cast<float>(M_PI);
        const float pitch = -std::atan2(d.z, std::hypot(d.x, d.y)) * 180.f / static_cast<float>(M_PI);
        text3d::queueOverlay(panelText, at, glm::vec3{pitch, yaw, 0.f}, charSize / 8.f, panelBars, 0.8f);
        return;
    }
    // In front: 0.9 m ahead and a little below the eyes, turning after the head once it looks 30 degrees away.
    const float headYaw = s.headAngles.y;
    if(!panelPlaced)
    {
        panelYaw = headYaw;
        panelPlaced = true;
    }
    float off = std::remainder(headYaw - panelYaw, 360.f);
    if(std::fabs(off) > 30.f)
    {
        const float step = std::min(std::fabs(off) - 30.f, std::max(1.f, 120.f * static_cast<float>(host_frametime)));
        panelYaw += off > 0.f ? step : -step;
    }
    const float yawRad = panelYaw * static_cast<float>(M_PI) / 180.f;
    const glm::vec3 dir{std::cos(yawRad), std::sin(yawRad), 0.f};
    const glm::vec3 at = s.head + dir * (0.9f * m2u) - glm::vec3{0.f, 0.f, 0.12f * m2u};
    const glm::vec3 d = at - s.head;
    const float pitch = -std::atan2(d.z, std::hypot(d.x, d.y)) * 180.f / static_cast<float>(M_PI);
    const float charSize = 0.0095f * m2u; // 9.5 mm at 0.9 m: about the menu's text
    text3d::queueOverlay(panelText, at, glm::vec3{pitch, panelYaw, 0.f}, charSize / 8.f, panelBars, 0.8f);
}

} // namespace qvr::profile::systems
