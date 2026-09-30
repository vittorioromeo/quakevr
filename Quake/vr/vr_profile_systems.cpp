// vr_profile_systems.cpp -- see vr_profile_systems.hpp.
//
// Each frame's time is kept per system and view (nanoseconds, CPU; milliseconds, GPU), then added into three sums: the
// current second (a row of the CSV, and of the ring of the last minute that vr_profile_report reads), the current half
// second (the panel shows the last two), and, for a frame over budget, the hitch log's line. A sum keeps each system's
// total and its worst frame.

#include "vr_profile_systems.hpp"
#include "vr_alloccount.hpp"
#include "vr_box3d.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_hands.hpp"
#include "vr_main.hpp"
#include "vr_profile.hpp"
#include "vr_text3d.hpp"
#include "vr_units.hpp"

#include "Zancle/Algorithm/Count.hpp"
#include "Zancle/Algorithm/Find.hpp"
#include "Zancle/Algorithm/Sort.hpp"
#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Base/Macros.hpp"
#include "Zancle/Base/PtrDiffT.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Base/Strcmp.hpp"
#include "Zancle/Base/Strlen.hpp"
#include "Zancle/Base/Strncmp.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Atan2.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/Fabs.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Remainder.hpp"
#include "Zancle/Math/Sin.hpp"
#include "Zancle/String/String.hpp"
#include "Zancle/String/StringView.hpp"
#include "vr_zancle.hpp"

#include <cstdarg>
#include <stdio.h>
#include <string.h>
#include <time.h>

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
    {"grapple rope sim", Server, "grapple rope sim"}, // vr_ropesim.cpp: the physical rope's chain and its taut path
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
    {"grapple rope", Render, "grapple rope|grapple rope upload|grapple rope draw"}, // vr_rope.cpp: its curve (made with the temp entities), its links bent on the GPU
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
    Allocations,
    CountCount
};
constexpr const char* countNames[CountCount] = {"traces", "hull traces", "draw calls", "alias models", "box3d bodies",
    "box3d awake", "box3d contacts", "edicts", "allocations"};
constexpr const char* countColumns[CountCount] = {"traces", "hull_traces", "draw_calls", "alias_models", "box3d_bodies",
    "box3d_awake", "box3d_contacts", "edicts", "allocations"};

[[nodiscard]] bool inList(const char* list, const char* name)
{
    const za::SizeT n = ZA_STRLEN(name);
    for(const char* p = list; *p;)
    {
        const char* bar = strchr(p, '|');
        const za::SizeT len = bar ? static_cast<za::SizeT>(bar - p) : ZA_STRLEN(p);
        if(len == n && ZA_STRNCMP(p, name, n) == 0)
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

za::I64 frameCpu[SysCount]{};
za::U64 allocationsBefore = 0; // the main thread's allocation count at the last frame collected's end
int allocationsFrame = -1;           // and that frame's host_framecount
za::I64 frameViewCpu[ViewCount]{};
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
    max = za::max(max, v);
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
        max = za::max(max, m2);
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
    za::String name;
    double instructions;
};

struct Second
{
    Sum sum;
    za::String map;
    double qcInstructions{0.0};
    za::Vector<QcFunction> qcTop; // the costliest few
};

constexpr int ringSize = 120; // seconds kept for vr_profile_report
Second ring[ringSize];
int ringCount = 0; // filled so far
int ringNext = 0;

Sum second;       // the current second's
Sum half;         // the current half second's (the panel)
Sum lastHalf;     // the one before
za::I64 secondStart = 0;
za::I64 halfStart = 0;
za::I64 captureStart = 0; // the CSV capture's first second's start
za::String mapNow = "none";

// QuakeC's per-function counts when last read (sv.qcvm's; restarted when its progs change).
za::Vector<int> qcLast;
const void* qcProgs = nullptr;

// ---- Files ----

// Kept open while in use (a row flushed at a time): opening and closing a file each second cost up to 2 ms, a hitch of
// its own.
za::String csvPath;   // this capture's (empty: none)
FILE* csvFile = nullptr;
za::String hitchPath; // this session's hitch log (made at the first hitch)
FILE* hitchFile = nullptr;
int hitchLines = 0;    // console lines this second (at most a few a second: the file has every one)
int hitchQuiet = 0;    // those not printed

za::String panelText;
za::Vector<text3d::OverlayBar> panelBars;
float panelYaw = 0.f; // vr_profile_overlay 2: the panel's direction, easing after the head's
bool panelPlaced = false;

void appendf(za::String& out, const char* fmt, ...)
{
    char buf[512];
    va_list args;
    va_start(args, fmt);
    q_vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    out += buf;
}

[[nodiscard]] za::String stamp(const char* fmt)
{
    const time_t now = time(nullptr);
    char buf[64];
    strftime(buf, sizeof(buf), fmt, localtime(&now));
    return buf;
}

// A system's name as a CSV column: letters and digits, '_' for the rest.
[[nodiscard]] za::String column(const char* name)
{
    za::String c;
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
        c.popBack();
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
        qcLast.clear();
        qcLast.resize(static_cast<za::SizeT>(count), 0);
        qcProgs = sv.qcvm.progs;
    }
    struct Ran
    {
        int instructions;
        int function;
    };
    za::Vector<Ran> ran;
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
            ran.pushBack({delta, i});
        }
    }
    constexpr za::SizeT keep = 8;
    const za::SizeT n = za::min(keep, ran.size());
    za::quickSort(ran.begin(), ran.end(), [](const Ran& a, const Ran& b) { return a.instructions > b.instructions; });
    qcvm_t* old = nullptr;
    PR_PushQCVM(&sv.qcvm, &old);
    for(za::SizeT k = 0; k < n; k++)
    {
        s.qcTop.pushBack({PR_GetString(sv.qcvm.functions[ran[k].function].s_name), static_cast<double>(ran[k].instructions)});
    }
    PR_PopQCVM(old);
}

// ---- The CSV capture (vr_profile_csv) ----

void csvHeader(FILE* f)
{
    fprintf(f, "# Quake VR profile by system (vr_profile_csv), %s: a row a second; ms a frame, averaged over its "
                    "frames (max: its worst frame); each system's own time, so they add up to period_ms; gpu_*: the GPU's "
                    "time; counts a frame\n",
        stamp("%Y-%m-%d %H:%M:%S").cStr());
    fprintf(f, "time_s,clock,map,frames,fps,period_ms,period_max_ms,budget_ms,slow_frames,long_frames,hitches,busy_ms,"
                    "busy_max_ms,gpu_ms,gpu_max_ms");
    for(const char* g : groupNames)
    {
        fprintf(f, ",group_%s_ms", column(g).cStr());
    }
    for(const Def& d : defs)
    {
        fprintf(f, ",%s_ms", column(d.name).cStr());
    }
    for(const Def& d : defs)
    {
        fprintf(f, ",%s_max_ms", column(d.name).cStr());
    }
    for(const Def& d : defs)
    {
        fprintf(f, ",gpu_%s_ms", column(d.name).cStr());
    }
    for(const char* v : viewNames)
    {
        fprintf(f, ",view_%s_ms,view_%s_gpu_ms", column(v).cStr(), column(v).cStr());
    }
    for(const char* c : countColumns)
    {
        fprintf(f, ",%s,%s_max", c, c);
    }
    fprintf(f, ",qc_top\n");
}

void csvRow(const Second& s, double time)
{
    if(csvPath.empty())
    {
        const za::String dir = za::String{com_gamedir} + "/profile";
        Sys_mkdir(dir.cStr());
        csvPath = dir + "/systems_" + stamp("%Y-%m-%d_%H-%M-%S") + ".csv";
        csvFile = fopen(csvPath.cStr(), "w");
        if(!csvFile)
        {
            Con_Warning("vr_profile_csv: can't write %s\n", csvPath.cStr());
            return;
        }
        csvHeader(csvFile);
        Con_Printf("vr_profile_csv: writing %s\n", csvPath.cStr());
    }
    FILE* f = csvFile;
    if(!f)
    {
        return;
    }
    const Sum& m = s.sum;
    const double frames = za::max(m.frames, 1);
    const double gpuFrames = za::max(m.gpuFrames, 1);
    const double period = m.periodSum / frames;
    fprintf(f, "%.2f,%s,%s,%d,%.1f,%.3f,%.2f,%.3f,%d,%d,%d,%.3f,%.2f,%.3f,%.2f", time, stamp("%H:%M:%S").cStr(),
        s.map.cStr(), m.frames, period > 0.0 ? 1000.0 / period : 0.0, period, m.periodMax, m.budgetMs, m.slowFrames,
        m.longFrames, m.hitches, m.busySum / frames, m.busyMax, m.gpuTotalSum / gpuFrames, m.gpuTotalMax);
    for(int g = 0; g < GroupCount; g++)
    {
        fprintf(f, ",%.3f", m.groupSum[g] / frames);
    }
    for(int i = 0; i < SysCount; i++)
    {
        fprintf(f, ",%.3f", m.cpuSum[i] / frames);
    }
    for(int i = 0; i < SysCount; i++)
    {
        fprintf(f, ",%.3f", m.cpuMax[i]);
    }
    for(int i = 0; i < SysCount; i++)
    {
        fprintf(f, ",%.3f", m.gpuSum[i] / gpuFrames);
    }
    for(int v = 0; v < ViewCount; v++)
    {
        fprintf(f, ",%.3f,%.3f", m.viewSum[v] / frames, m.gpuViewSum[v] / gpuFrames);
    }
    for(int c = 0; c < CountCount; c++)
    {
        fprintf(f, ",%.1f,%.0f", m.countSum[c] / frames, m.countMax[c]);
    }
    za::String top;
    for(za::SizeT k = 0; k < s.qcTop.size() && k < 5; k++)
    {
        appendf(top, "%s%s %.0f%%", top.empty() ? "" : " ", s.qcTop[k].name.cStr(),
            100.0 * s.qcTop[k].instructions / za::max(s.qcInstructions, 1.0));
    }
    fprintf(f, ",%s\n", top.cStr());
    fflush(f);
}

void closeCsv()
{
    if(csvFile)
    {
        fclose(csvFile);
        csvFile = nullptr;
    }
    if(!csvPath.empty())
    {
        Con_Printf("vr_profile_csv: closed %s\n", csvPath.cStr());
        csvPath.clear();
    }
}

// ---- The hitch log (vr_profile_hitch) ----

void hitch(double periodMs, double hostMs, double budget, const int counts[CountCount], const za::String& scopes)
{
    struct Part
    {
        double ms;
        int system;
    };
    za::Vector<Part> parts;
    for(int i = 0; i < SysCount; i++)
    {
        const double ms = static_cast<double>(frameCpu[i]) / 1e6;
        if(ms >= 0.05)
        {
            parts.pushBack({ms, i});
        }
    }
    za::quickSort(parts.begin(), parts.end(), [](const Part& a, const Part& b) { return a.ms > b.ms; });

    if(hitchLines < 5)
    {
        za::String line;
        for(za::SizeT k = 0; k < parts.size() && k < 5; k++)
        {
            appendf(line, "%s%s %.1f", k ? ", " : "", defs[parts[k].system].name, parts[k].ms);
        }
        Con_Printf("vr_profile: hitch %.1f ms (%.1f budgets), %s: %s\n", periodMs, periodMs / budget, mapNow.cStr(),
            line.cStr());
        if(!scopes.empty())
        {
            Con_Printf("  scopes: %s\n", scopes.cStr());
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
        const za::String dir = za::String{com_gamedir} + "/profile";
        Sys_mkdir(dir.cStr());
        hitchPath = dir + "/hitches_" + stamp("%Y-%m-%d_%H-%M-%S") + ".csv";
    }
    if(!hitchFile)
    {
        hitchFile = fopen(hitchPath.cStr(), "a"); // (again after a stop: the session's one file)
    }
    FILE* f = hitchFile;
    if(!f)
    {
        return;
    }
    if(created)
    {
        fprintf(f, "# Quake VR hitches (vr_profile_hitch): frames over %.2f budgets, each with its systems' own time "
                        "(ms), which add up to period_ms\nclock,map,period_ms,budget_ms,host_ms",
            vr_profile_hitch.value);
        for(const Def& d : defs)
        {
            fprintf(f, ",%s_ms", column(d.name).cStr());
        }
        for(const char* c : countColumns)
        {
            fprintf(f, ",%s", c);
        }
        fprintf(f, ",scopes\n");
    }
    fprintf(f, "%s,%s,%.2f,%.2f,%.2f", stamp("%H:%M:%S").cStr(), mapNow.cStr(), periodMs, budget, hostMs);
    for(int i = 0; i < SysCount; i++)
    {
        fprintf(f, ",%.3f", static_cast<double>(frameCpu[i]) / 1e6);
    }
    for(int c = 0; c < CountCount; c++)
    {
        fprintf(f, ",%d", counts[c]);
    }
    fprintf(f, ",\"%s\"\n", scopes.cStr()); // (quoted: commas in it)
    fflush(f);
}

// ---- Reading the sums ----

// The last `seconds` seconds (the ring's, and the current second's so far).
[[nodiscard]] Sum lastSeconds(double seconds, za::String& map, double& qcInstructions, za::Vector<QcFunction>& qcTop)
{
    Sum s;
    Sum current = second; // this second so far
    current.seconds = za::max(0.0, nowSeconds() - static_cast<double>(secondStart) / 1e9);
    merge(s, current);
    map = mapNow;
    qcInstructions = 0.0;
    za::Vector<QcFunction> all;
    for(int k = 0; k < ringCount && s.seconds < seconds - 0.25; k++)
    {
        const Second& r = ring[(ringNext - 1 - k + ringSize) % ringSize];
        merge(s, r.sum);
        qcInstructions += r.qcInstructions;
        for(const QcFunction& q : r.qcTop)
        {
            auto it = za::findIf(all.begin(), all.end(), [&](const QcFunction& a) { return a.name == q.name; });
            if(it != all.end())
            {
                it->instructions += q.instructions;
            }
            else
            {
                all.pushBack(q);
            }
        }
    }
    za::quickSort(all.begin(), all.end(), [](const QcFunction& a, const QcFunction& b) { return a.instructions > b.instructions; });
    if(all.size() > 6)
    {
        all.resize(6);
    }
    qcTop = ZA_MOVE(all);
    return s;
}

struct Row
{
    int index;
    double avg;
};

// The systems of a group (or all, group -1), costliest first, down to `least` ms (or their max to 10 times that).
[[nodiscard]] za::Vector<Row> rows(const Sum& s, int group, bool gpuSide, double least)
{
    const double frames = za::max(gpuSide ? s.gpuFrames : s.frames, 1);
    za::Vector<Row> out;
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
            out.pushBack({i, avg});
        }
    }
    za::quickSort(out.begin(), out.end(), [](const Row& a, const Row& b) { return a.avg > b.avg; });
    return out;
}

void report_f()
{
    if(!active)
    {
        Con_Printf("vr_profile_report: not collecting (vr_profile 1, vr_profile_overlay 1 or vr_profile_csv 1)\n");
        return;
    }
    const double seconds = Cmd_Argc() > 1 ? za::max(0.5, atof(Cmd_Argv(1))) : 5.0;
    za::String map;
    double qcInstructions = 0.0;
    za::Vector<QcFunction> qcTop;
    const Sum s = lastSeconds(seconds, map, qcInstructions, qcTop);
    if(s.frames == 0)
    {
        Con_Printf("vr_profile_report: no frames yet\n");
        return;
    }
    const double frames = s.frames;
    const double gpuFrames = za::max(s.gpuFrames, 1);
    const double period = s.periodSum / frames;
    Con_Printf("vr_profile_report: %s, the last %.1f s: %d frames, %.2f ms apart (%.1f fps; worst %.2f), budget %.2f ms; "
               "%d missed a refresh, %d hitches, %d long (over 250 ms, left out)\n",
        map.cStr(), s.seconds, s.frames, period, period > 0.0 ? 1000.0 / period : 0.0, s.periodMax, s.budgetMs,
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
        for(za::SizeT k = 0; k < qcTop.size(); k++)
        {
            const double share = qcTop[k].instructions / qcInstructions;
            Con_Printf(" %s %.0f%% (~%.3f)%s", qcTop[k].name.cStr(), 100.0 * share, share * qcMs,
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
void panelRow(za::Vector<za::String>& lines, const char* name, double avg, double max, double budget, glm::vec4 color,
    bool waiting)
{
    za::String l;
    appendf(l, "%-16.16s %5.2f %5.2f ", name, avg, max);
    l.resize(l.size() + static_cast<za::SizeT>(barCells) + 1, ' ');
    const float cells = static_cast<float>(avg / budget) * barCells;
    if(avg > budget && !waiting)
    {
        color = glm::vec4{1.f, 0.3f, 0.25f, 0.9f}; // work over the whole budget by itself
    }
    panelBars.pushBack({static_cast<int>(lines.size()), static_cast<float>(barColumn), cells, barCells, color});
    lines.pushBack(ZA_MOVE(l));
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
    const double gpuFrames = za::max(s.gpuFrames, 1);
    const double period = s.periodSum / frames;
    const double budget = s.budgetMs > 0.0 ? s.budgetMs : 1000.0 / 90.0;
    za::Vector<za::String> lines;
    za::String l;
    appendf(l, "PROFILE %-12.12s %5.1f fps %6.2f ms, worst %6.2f", mapNow.cStr(), period > 0.0 ? 1000.0 / period : 0.0,
        period, s.periodMax);
    lines.pushBack(l);
    l.clear();
    appendf(l, "budget %5.2f   CPU busy %5.2f   GPU %5.2f", budget, s.busySum / frames, s.gpuTotalSum / gpuFrames);
    lines.pushBack(l);
    l.clear();
    for(int g = 0; g < GroupCount; g++)
    {
        appendf(l, "%s%s %.2f", g ? "  " : "", groupShort[g], s.groupSum[g] / frames);
    }
    lines.pushBack(l);
    lines.emplaceBack("CPU               avg   max  (bar: the budget)");
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
    lines.emplaceBack("GPU               avg   max");
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
    lines.pushBack(l);
    l.clear();
    appendf(l, "box3d %.0f (%.0f awake, %.0f contacts)  edicts %.0f", s.countSum[Box3dBodies] / frames,
        s.countSum[Box3dAwake] / frames, s.countSum[Box3dContacts] / frames, s.countSum[Edicts] / frames);
    lines.pushBack(l);
    if(s.hitches || s.slowFrames)
    {
        l.clear();
        appendf(l, "hitches %d  missed refreshes %d", s.hitches, s.slowFrames);
        lines.pushBack(l);
    }

    // The lines are drawn centred: padded to one width, they line up.
    za::SizeT width = 0;
    for(const za::String& line : lines)
    {
        width = za::max(width, line.size());
    }
    for(const za::String& line : lines)
    {
        panelText += line;
        panelText.resize(panelText.size() + (width - line.size()), ' ');
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
    if(ZA_STRCMP(name, "eye L") == 0)
    {
        view = EyeL;
    }
    else if(ZA_STRCMP(name, "eye R") == 0)
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

void cpu(int system, int view, za::I64 ns)
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
    qza::fill(frameGpu, 0.0);
    qza::fill(frameViewGpu, 0.0);
}

void profilerTime(za::I64 ns)
{
    cpu(Profiler, Shared, ns);
    cpu(Other, Shared, -ns);
}

double hitchMs()
{
    return vr_profile_hitch.value > 0.f ? vr_profile_hitch.value * budgetMs() : 0.0;
}

void frameEnd(za::I64 now, za::I64 periodNs, za::I64 hostNs, const Counts& counts, double longMs,
    const char* map, const za::String& scopes)
{
    mapNow = map;
    // The frame's own time (the root scope's), and the idle time after it.
    cpu(Other, Shared, hostNs);
    cpu(FrameCap, Shared, za::max<za::I64>(0, periodNs - hostNs));

    int n[CountCount]{counts.traces, counts.hullChecks, counts.drawCalls, counts.aliasDrawn, 0, 0, 0, 0, 0};
    box3d::profileCounts(n[Box3dBodies], n[Box3dAwake], n[Box3dContacts]);
    n[Edicts] = sv.active ? dev_stats.edicts : 0;
    // The main thread's C++ allocations since the last frame's end (vr_alloccount.cpp); none counted for the first frame
    // collected (its delta would span the frames before).
    const za::U64 allocationsNow = alloccount::thisThread();
    n[Allocations] = allocationsFrame == host_framecount - 1 ? static_cast<int>(allocationsNow - allocationsBefore) : 0;
    allocationsBefore = allocationsNow;
    allocationsFrame = host_framecount;

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
    qza::fill(frameCpu, za::I64{0});
    qza::fill(frameViewCpu, za::I64{0});

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
        ringCount = za::min(ringCount + 1, ringSize);
        second = Sum{};
        secondStart = now;
        if(hitchQuiet)
        {
            Con_Printf("vr_profile: %d more hitches (in %s)\n", hitchQuiet, hitchPath.cStr());
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

void start(za::I64 now)
{
    second = half = lastHalf = Sum{};
    secondStart = halfStart = now;
    ringCount = ringNext = 0;
    qza::fill(frameCpu, za::I64{0});
    qza::fill(frameViewCpu, za::I64{0});
    qza::fill(frameGpu, 0.0);
    qza::fill(frameViewGpu, 0.0);
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
        fclose(hitchFile);
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
    const auto lineCount = static_cast<float>(za::count(panelText.begin(), panelText.end(), '\n'));
    if(mode == 1)
    {
        // Over the wrist gadget's hand, facing the head: 4.5 mm characters (at 45 cm, about the gadget's).
        const float charSize = 0.0045f * m2u;
        const glm::vec3 at = s.pos[hands::gadgetHand()] +
                             glm::vec3{0.f, 0.f, 0.06f * m2u + 0.5f * lineCount * charSize};
        const glm::vec3 d = at - s.head;
        const float yaw = za::atan2(d.y, d.x) * 180.f / static_cast<float>(M_PI);
        const float pitch = -za::atan2(d.z, qza::hypot(d.x, d.y)) * 180.f / static_cast<float>(M_PI);
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
    float off = za::remainder(headYaw - panelYaw, 360.f);
    if(za::fabs(off) > 30.f)
    {
        const float step = za::min(za::fabs(off) - 30.f, za::max(1.f, 120.f * static_cast<float>(host_frametime)));
        panelYaw += off > 0.f ? step : -step;
    }
    const float yawRad = panelYaw * static_cast<float>(M_PI) / 180.f;
    const glm::vec3 dir{za::cos(yawRad), za::sin(yawRad), 0.f};
    const glm::vec3 at = s.head + dir * (0.9f * m2u) - glm::vec3{0.f, 0.f, 0.12f * m2u};
    const glm::vec3 d = at - s.head;
    const float pitch = -za::atan2(d.z, qza::hypot(d.x, d.y)) * 180.f / static_cast<float>(M_PI);
    const float charSize = 0.0095f * m2u; // 9.5 mm at 0.9 m: about the menu's text
    text3d::queueOverlay(panelText, at, glm::vec3{pitch, panelYaw, 0.f}, charSize / 8.f, panelBars, 0.8f);
}

} // namespace qvr::profile::systems
