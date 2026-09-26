// vr_profile.cpp -- see vr_profile.hpp.
//
// Each scope is a node of a call tree, keyed by its parent and its name (the eyes' scopes are
// apart, under "eye L" and "eye R"). A frame adds up each node's CPU time and calls; at the frame's
// end they are folded into the interval's sums and maxima. GPU scopes also record a timestamp
// query at each end, into the frame's slot of a ring: a slot is read back once its last query
// is available (a few frames later), so the CPU never waits for the GPU; only when the ring comes
// round to a slot still pending is it read waiting (counted as a stall in the report).

#include "vr_profile.hpp"
#include "vr_engine.hpp"
#include "vr_cvars.hpp"
#include "vr_hands.hpp"
#include "vr_main.hpp"
#include "vr_text3d.hpp"
#include "vr_units.hpp"

#include <algorithm>
#include <chrono>
#include <cstdarg>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>
#include <unordered_map>
#include <vector>

namespace qvr::profile
{

bool active = false;

namespace
{

// Frames longer than this (a map loading, a hitch) are counted apart, not in the averages.
constexpr double hitchMs = 250.0;
// GPU frames in flight before a slot is read waiting.
constexpr int gpuSlots = 6;
// Overlay refresh, seconds.
constexpr double overlayPeriod = 0.5;

[[nodiscard]] std::int64_t nowNs()
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

struct Node
{
    const char* name{nullptr};
    int parent{-1};
    int depth{0};
    std::vector<int> children;

    // This frame.
    std::int64_t frameNs{0};
    int frameCalls{0};
    bool cpuTouched{false};
    double gpuFrameMs{0.0};
    bool gpuTouched{false};

    // This interval.
    double cpuSum{0.0}; // ms
    double cpuMax{0.0};
    std::int64_t calls{0};
    double gpuSum{0.0};
    double gpuMax{0.0};
    bool gpu{false};
};

std::vector<Node> nodes; // [0]: the frame
std::vector<int> cpuTouched;
std::vector<int> gpuTouched;

struct Open
{
    int node;
    std::int64_t start;
    int gpuRec; // -1: CPU only
};
std::vector<Open> stack;

struct GpuRec
{
    int node;
    int begin;
    int end;
    bool top; // not inside another GPU scope: adds to the frame's GPU time
};

struct GpuSlot
{
    std::vector<GLuint> queries;
    int used{0};
    std::vector<GpuRec> recs;
    bool pending{false};
};
GpuSlot slots[gpuSlots];
int slotIndex = 0; // this frame's
int gpuDepth = 0;
bool gpuOk = false;

std::int64_t frameStart = 0;
std::int64_t frameEnd = 0; // VR_ProfileFrameEnd, when called (else the next frame's start)
bool frameEnded = false;
double periodSum = 0.0; // ms from a frame's start to the next's, idle time included
double periodMax = 0.0;
std::int64_t intervalStart = 0;
int intervalFrames = 0;
int intervalGpuFrames = 0;
int intervalHitches = 0;
int intervalStalls = 0;
int intervalNumber = 0;

std::string capturePath; // this capture's file (empty: none written yet)
std::string captureMap;
std::string lastContext;
std::string mapName = "none";

std::string overlayText;
std::int64_t overlayUpdated = 0;

// ---- The always-on phases (vr_profile.hpp) ----

struct PhaseInfo
{
    const char* name;
    bool gpu;
};
constexpr PhaseInfo phaseInfo[PhaseCount] = {{"xr wait", false}, {"xrWaitFrame", false}, {"commands", false},
    {"server", false}, {"SV_Physics", false}, {"client read", false}, {"view entities", false}, {"screen", false},
    {"eye L", true}, {"eye R", true}, {"xr acquire", true}, {"xr release", true}, {"xr submit", true}, {"swap", false},
    {"run particles", false}, {"sound", false}, {"rigid bodies", false}, {"shadow maps", true}, {"world+brush", true},
    {"alias", true}, {"particles", true}, {"vr particles", true}, {"decals", true}};

std::unordered_map<const void*, int> phaseOfName; // a name literal's address -> its phase, or -1

struct PhaseOpen
{
    int phase; // -1: not a phase
    std::int64_t start;
    int gpuRec; // -1: none
};
std::vector<PhaseOpen> phaseStack;
std::int64_t phaseFrameNs[PhaseCount]{};
PhaseSums phaseSums;
double displayPeriod = 0.0; // ms

constexpr int phaseGpuSlots = 6;
constexpr int phaseGpuQueries = 96; // a frame's at most
struct PhaseGpuRec
{
    int phase;
    int begin;
    int end;
};
struct PhaseGpuSlot
{
    GLuint queries[phaseGpuQueries]{};
    int used{0};
    PhaseGpuRec recs[phaseGpuQueries / 2]{};
    int recCount{0};
    bool pending{false};
};
PhaseGpuSlot phaseSlots[phaseGpuSlots];
int phaseSlot = 0;
bool phaseGpuMade = false;

[[nodiscard]] int phaseOf(const char* name)
{
    const auto it = phaseOfName.find(name);
    if(it != phaseOfName.end())
    {
        return it->second;
    }
    int phase = -1;
    for(int i = 0; i < PhaseCount; i++)
    {
        if(std::strcmp(phaseInfo[i].name, name) == 0)
        {
            phase = i;
            break;
        }
    }
    phaseOfName.emplace(name, phase);
    return phase;
}

void beginPhase(const char* name, bool gpu)
{
    PhaseOpen o{phaseOf(name), 0, -1};
    if(o.phase >= 0)
    {
        o.start = nowNs();
        PhaseGpuSlot& s = phaseSlots[phaseSlot];
        if(gpu && phaseInfo[o.phase].gpu && phaseGpuMade && s.used + 2 <= phaseGpuQueries)
        {
            GL_QueryCounterFunc(s.queries[s.used], GL_TIMESTAMP);
            o.gpuRec = s.recCount++;
            s.recs[o.gpuRec] = {o.phase, s.used++, -1};
        }
    }
    phaseStack.push_back(o);
}

void endPhase()
{
    if(phaseStack.empty())
    {
        return;
    }
    const PhaseOpen o = phaseStack.back();
    phaseStack.pop_back();
    if(o.phase < 0)
    {
        return;
    }
    phaseFrameNs[o.phase] += nowNs() - o.start;
    PhaseGpuSlot& s = phaseSlots[phaseSlot];
    // A begin keeps room for its own end only: scopes nested inside it can fill the slot first (its
    // record is then left without an end, and not read back).
    if(o.gpuRec >= 0 && s.used < phaseGpuQueries)
    {
        GL_QueryCounterFunc(s.queries[s.used], GL_TIMESTAMP);
        s.recs[o.gpuRec].end = s.used++;
    }
}

// Reads a slot's timestamps if the GPU is done with them (never waits); false if not yet.
bool resolvePhases(PhaseGpuSlot& s)
{
    if(!s.pending)
    {
        return true;
    }
    GLint available = 0;
    GL_GetQueryObjectivFunc(s.queries[s.used - 1], GL_QUERY_RESULT_AVAILABLE, &available);
    if(!available)
    {
        return false;
    }
    for(int i = 0; i < s.recCount; i++)
    {
        const PhaseGpuRec& r = s.recs[i];
        if(r.end < 0)
        {
            continue;
        }
        GLuint64 b = 0, e = 0;
        GL_GetQueryObjectui64vFunc(s.queries[r.begin], GL_QUERY_RESULT, &b);
        GL_GetQueryObjectui64vFunc(s.queries[r.end], GL_QUERY_RESULT, &e);
        phaseSums.gpuMs[r.phase] += e > b ? static_cast<double>(e - b) / 1e6 : 0.0;
    }
    ++phaseSums.gpuFrames;
    s.pending = false;
    return true;
}

// The frame's end for the phases (VR_ProfileFrame): its times into the sums, its GPU slot sent off,
// the finished slots read back.
void endPhaseFrame(std::int64_t now, std::int64_t start, std::int64_t end)
{
    phaseStack.clear(); // a Host_Error jumped out of them, or a dialog's frame inside one
    const double period = start > 0 ? static_cast<double>(now - start) / 1e6 : 0.0;
    const bool keep = start > 0 && period <= hitchMs;
    PhaseGpuSlot& s = phaseSlots[phaseSlot];
    if(keep)
    {
        ++phaseSums.frames;
        phaseSums.periodMs += period;
        phaseSums.periodMaxMs = std::max(phaseSums.periodMaxMs, period);
        if(displayPeriod > 0.0 && period > 1.25 * displayPeriod)
        {
            ++phaseSums.slowFrames;
        }
        phaseSums.hostMs += static_cast<double>(end - start) / 1e6;
        for(int i = 0; i < PhaseCount; i++)
        {
            phaseSums.cpuMs[i] += static_cast<double>(phaseFrameNs[i]) / 1e6;
        }
    }
    else if(start > 0)
    {
        ++phaseSums.hitches;
    }
    std::fill(std::begin(phaseFrameNs), std::end(phaseFrameNs), std::int64_t{0});
    s.pending = keep && s.recCount > 0;

    if(!phaseGpuMade && GL_QueryCounterFunc && GL_GetQueryObjectui64vFunc && GL_GetQueryObjectivFunc &&
        GL_GenQueriesFunc)
    {
        for(PhaseGpuSlot& slot : phaseSlots)
        {
            GL_GenQueriesFunc(phaseGpuQueries, slot.queries);
        }
        phaseGpuMade = true;
    }
    phaseSlot = (phaseSlot + 1) % phaseGpuSlots;
    for(int k = 0; k < phaseGpuSlots; k++) // oldest first: the ring's next slot is the oldest
    {
        if(!resolvePhases(phaseSlots[(phaseSlot + k) % phaseGpuSlots]))
        {
            break;
        }
    }
    PhaseGpuSlot& next = phaseSlots[phaseSlot];
    if(next.pending)
    {
        next.pending = false; // not done after this many frames: dropped, not waited for
        ++phaseSums.gpuDropped;
    }
    next.used = 0;
    next.recCount = 0;
}

[[nodiscard]] int child(int parent, const char* name)
{
    for(int c : nodes[parent].children)
    {
        if(nodes[c].name == name || std::strcmp(nodes[c].name, name) == 0)
        {
            return c;
        }
    }
    Node n;
    n.name = name;
    n.parent = parent;
    n.depth = nodes[parent].depth + 1;
    nodes.push_back(n);
    const int index = static_cast<int>(nodes.size()) - 1;
    nodes[parent].children.push_back(index);
    return index;
}

[[nodiscard]] int query(GpuSlot& s)
{
    if(s.used == static_cast<int>(s.queries.size()))
    {
        const std::size_t old = s.queries.size();
        s.queries.resize(old + 64);
        GL_GenQueriesFunc(64, s.queries.data() + old);
    }
    GL_QueryCounterFunc(s.queries[s.used], GL_TIMESTAMP);
    return s.used++;
}

void resetInterval(std::int64_t now)
{
    for(Node& n : nodes)
    {
        n.cpuSum = n.cpuMax = n.gpuSum = n.gpuMax = 0.0;
        n.calls = 0;
    }
    intervalStart = now;
    intervalFrames = intervalGpuFrames = intervalHitches = intervalStalls = 0;
    periodSum = periodMax = 0.0;
}

// Adds this frame's CPU times to the interval (or drops them, for a hitch).
void foldCpu(bool keep)
{
    for(int i : cpuTouched)
    {
        Node& n = nodes[i];
        if(keep)
        {
            const double ms = static_cast<double>(n.frameNs) / 1e6;
            n.cpuSum += ms;
            n.cpuMax = std::max(n.cpuMax, ms);
            n.calls += n.frameCalls;
        }
        n.frameNs = 0;
        n.frameCalls = 0;
        n.cpuTouched = false;
    }
    cpuTouched.clear();
}

void touchGpu(int node, double ms)
{
    Node& n = nodes[node];
    n.gpuFrameMs += ms;
    if(!n.gpuTouched)
    {
        n.gpuTouched = true;
        gpuTouched.push_back(node);
    }
}

// Reads a slot's queries (waiting for them if `wait`); false if they are not available yet.
bool resolve(GpuSlot& s, bool wait)
{
    if(!s.pending)
    {
        return true;
    }
    if(!wait)
    {
        GLint available = 0;
        GL_GetQueryObjectivFunc(s.queries[s.used - 1], GL_QUERY_RESULT_AVAILABLE, &available);
        if(!available)
        {
            return false;
        }
    }
    for(const GpuRec& r : s.recs)
    {
        if(r.end < 0)
        {
            continue;
        }
        GLuint64 b = 0, e = 0;
        GL_GetQueryObjectui64vFunc(s.queries[r.begin], GL_QUERY_RESULT, &b);
        GL_GetQueryObjectui64vFunc(s.queries[r.end], GL_QUERY_RESULT, &e);
        const double ms = e > b ? static_cast<double>(e - b) / 1e6 : 0.0;
        touchGpu(r.node, ms);
        if(r.top)
        {
            touchGpu(0, ms);
        }
    }
    for(int i : gpuTouched)
    {
        Node& n = nodes[i];
        n.gpuSum += n.gpuFrameMs;
        n.gpuMax = std::max(n.gpuMax, n.gpuFrameMs);
        n.gpu = true;
        n.gpuFrameMs = 0.0;
        n.gpuTouched = false;
    }
    gpuTouched.clear();
    ++intervalGpuFrames;
    s.pending = false;
    return true;
}

void dropGpu()
{
    for(GpuSlot& s : slots)
    {
        s.pending = false;
        s.used = 0;
        s.recs.clear();
    }
}

// ---- Reporting ----

struct Stats
{
    double cpuAvg, cpuMax, gpuAvg, gpuMax, cpuSelf, gpuSelf, calls;
};

[[nodiscard]] Stats stats(int i)
{
    const Node& n = nodes[i];
    const double frames = std::max(intervalFrames, 1);
    const double gpuFrames = std::max(intervalGpuFrames, 1);
    Stats s{};
    s.cpuAvg = n.cpuSum / frames;
    s.cpuMax = n.cpuMax;
    s.gpuAvg = n.gpuSum / gpuFrames;
    s.gpuMax = n.gpuMax;
    s.calls = static_cast<double>(n.calls) / frames;
    double cpuChildren = 0.0, gpuChildren = 0.0;
    for(int c : n.children)
    {
        cpuChildren += nodes[c].cpuSum / frames;
        gpuChildren += nodes[c].gpuSum / gpuFrames;
    }
    s.cpuSelf = std::max(0.0, s.cpuAvg - cpuChildren);
    s.gpuSelf = n.gpu ? std::max(0.0, s.gpuAvg - gpuChildren) : 0.0;
    if(i == 0)
    {
        s.calls = 1.0;
    }
    return s;
}

[[nodiscard]] double sumNamed(const char* name)
{
    double sum = 0.0;
    for(std::size_t i = 1; i < nodes.size(); i++)
    {
        if(std::strcmp(nodes[i].name, name) == 0)
        {
            sum += nodes[i].cpuSum;
        }
    }
    return sum / std::max(intervalFrames, 1);
}

// GPU time of the scopes so named (summed over the tree), per frame.
[[nodiscard]] double gpuNamed(const char* name)
{
    double sum = 0.0;
    for(std::size_t i = 1; i < nodes.size(); i++)
    {
        if(std::strcmp(nodes[i].name, name) == 0)
        {
            sum += nodes[i].gpuSum;
        }
    }
    return sum / std::max(intervalGpuFrames, 1);
}

// The eyes' GPU time: the frame's GPU work in VR. The frame's GPU time (its outermost GPU scopes',
// each from its first timestamp to its last) also holds the gaps between the eyes: the runtime's
// calls ("xr acquire", "xr release", "xr submit"), where the GPU idles while the CPU waits in the
// runtime, or runs the runtime's and other processes' work (the compositor, a streaming encoder).
[[nodiscard]] double gpuEyes()
{
    return gpuNamed("eye L") + gpuNamed("eye R");
}

// _Host_Frame's CPU time, less the waits in it (the runtime's frame pacing, the window's buffer swap).
[[nodiscard]] double cpuBusy()
{
    return std::max(0.0, stats(0).cpuAvg - sumNamed("xr wait") - sumNamed("swap"));
}

struct Top
{
    const char* name;
    double ms;
};

// Scopes by self time (the same name summed over the tree: both eyes), costliest first.
[[nodiscard]] std::vector<Top> top(bool gpu, std::size_t count)
{
    std::vector<Top> all;
    for(std::size_t i = 1; i < nodes.size(); i++)
    {
        const Stats s = stats(static_cast<int>(i));
        const double ms = gpu ? s.gpuSelf : s.cpuSelf;
        if(ms <= 0.0)
        {
            continue;
        }
        auto it = std::find_if(all.begin(), all.end(), [&](const Top& t) { return std::strcmp(t.name, nodes[i].name) == 0; });
        if(it != all.end())
        {
            it->ms += ms;
        }
        else
        {
            all.push_back({nodes[i].name, ms});
        }
    }
    std::sort(all.begin(), all.end(), [](const Top& a, const Top& b) { return a.ms > b.ms; });
    if(all.size() > count)
    {
        all.resize(count);
    }
    return all;
}

[[nodiscard]] std::string path(int i)
{
    std::string p = nodes[i].name;
    for(int n = nodes[i].parent; n >= 0; n = nodes[n].parent)
    {
        p = std::string{nodes[n].name} + "/" + p;
    }
    return p;
}

void appendf(std::string& out, const char* fmt, ...)
{
    char buf[512];
    va_list args;
    va_start(args, fmt);
    q_vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    out += buf;
}

// Map, resolution and the graphics settings: "# key,value" lines.
[[nodiscard]] std::string context()
{
    std::string c;
    appendf(c, "# map,%s\n", captureMap.c_str());
    int w = 0, h = 0;
    if(Backend* be = backend())
    {
        be->eyeResolution(w, h);
        appendf(c, "# vr_backend,%s\n", be->name());
        appendf(c, "# xr_runtime,%s\n", be->runtimeName());
    }
    else
    {
        appendf(c, "# vr_backend,off\n");
    }
    appendf(c, "# eye_resolution,%dx%d\n", w, h);
    appendf(c, "# window,%dx%d\n", vid.width, vid.height);
    appendf(c, "# gl_renderer,%s\n", gl_renderer ? gl_renderer : "?");
    static const char* const cvars[] = {"vr_graphics_preset", "vid_fsaa", "r_scale", "vr_render_scale",
        "vr_visibility_mask", "r_oit", "vr_mirror",
        "host_maxfps", "vr_shadow_dlights", "vr_shadow_dlight_size", "vr_shadow_precision", "vr_shadow_muzzleflash",
        "vr_shadow_maplights", "vr_shadow_maplight_size", "vr_shadow_self", "vr_shadow_filter", "vr_shadow_atlas",
        "vr_shadow_distance", "vr_dlight_models", "vr_specular", "vr_normalmaps", "vr_bloom", "vr_bloom_radius",
        "vr_particles", "vr_particle_mult", "r_particles", "vr_decals", "vr_decal_max", "vr_blob_shadows",
        "vr_texture_smooth", "gl_texturemode", "gl_texture_anisotropy", "vr_body_mode", "vr_body_blood",
        "vr_gib_blood", "r_dynamic", "r_softemu", "r_waterwarp", "r_lerpmodels", "vr_flashlight"};
    for(const char* name : cvars)
    {
        if(const cvar_t* v = Cvar_FindVar(name))
        {
            appendf(c, "# cvar,%s,%s\n", name, v->string);
        }
    }
    return c;
}

void writeFile(double seconds)
{
    const bool created = capturePath.empty();
    if(created)
    {
        const std::time_t now = std::time(nullptr);
        char stamp[64];
        std::strftime(stamp, sizeof(stamp), "%Y-%m-%d_%H-%M-%S", std::localtime(&now));
        const std::string dir = std::string{com_gamedir} + "/profile";
        Sys_mkdir(dir.c_str());
        capturePath = dir + "/profile_" + captureMap + "_" + stamp + ".csv";
    }

    FILE* f = std::fopen(capturePath.c_str(), "a");
    if(!f)
    {
        Con_Warning("vr_profile: can't write %s\n", capturePath.c_str());
        return;
    }

    const std::string ctx = context();
    if(created)
    {
        const std::time_t now = std::time(nullptr);
        char when[64];
        std::strftime(when, sizeof(when), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
        std::fprintf(f, "# Quake VR profile (vr_profile), %s\n", when);
        std::fprintf(f, "# per interval and scope: calls and milliseconds per frame, averaged over its frames; "
                        "max is the worst frame; self excludes the child scopes\n");
        std::fputs(ctx.c_str(), f);
        std::fprintf(f, "interval,seconds,frames,gpu_frames,hitches,gpu_stalls,depth,scope,path,calls,cpu_avg_ms,"
                        "cpu_max_ms,cpu_self_ms,gpu_avg_ms,gpu_max_ms,gpu_self_ms\n");
    }
    else if(ctx != lastContext)
    {
        std::fprintf(f, "# settings changed before interval %d:\n", intervalNumber);
        std::fputs(ctx.c_str(), f);
    }
    lastContext = ctx;

    // The frame period (a frame's start to the next's, the frame rate cap's idle time included), then
    // the scopes depth first, in the order they were first seen ("frame": _Host_Frame's time; its GPU
    // time is its outermost GPU scopes').
    std::fprintf(f, "%d,%.2f,%d,%d,%d,%d,0,frame period,frame period,1.00,%.3f,%.3f,,,,\n", intervalNumber, seconds,
        intervalFrames, intervalGpuFrames, intervalHitches, intervalStalls, periodSum / std::max(intervalFrames, 1),
        periodMax);
    std::vector<int> todo{0};
    while(!todo.empty())
    {
        const int i = todo.back();
        todo.pop_back();
        const Node& n = nodes[i];
        for(auto it = n.children.rbegin(); it != n.children.rend(); ++it)
        {
            todo.push_back(*it);
        }
        if(i != 0 && n.calls == 0 && n.gpuSum == 0.0)
        {
            continue;
        }
        const Stats s = stats(i);
        std::fprintf(f, "%d,%.2f,%d,%d,%d,%d,%d,%s,%s,%.2f,%.3f,%.3f,%.3f,", intervalNumber, seconds, intervalFrames,
            intervalGpuFrames, intervalHitches, intervalStalls, n.depth, n.name, path(i).c_str(), s.calls, s.cpuAvg,
            s.cpuMax, s.cpuSelf);
        if(n.gpu)
        {
            std::fprintf(f, "%.3f,%.3f,%.3f\n", s.gpuAvg, s.gpuMax, s.gpuSelf);
        }
        else
        {
            std::fprintf(f, ",,\n");
        }
    }
    std::fclose(f);
}

[[nodiscard]] std::string topLine(bool gpu, std::size_t count)
{
    std::string line;
    for(const Top& t : top(gpu, count))
    {
        appendf(line, "%s%s %.2f", line.empty() ? "" : ", ", t.name, t.ms);
    }
    return line;
}

void printTree()
{
    Con_Printf("%-30s %6s %6s %6s %6s\n", "scope (ms/frame)", "cpu", "max", "gpu", "max");
    std::vector<int> todo{0};
    while(!todo.empty())
    {
        const int i = todo.back();
        todo.pop_back();
        const Node& n = nodes[i];
        for(auto it = n.children.rbegin(); it != n.children.rend(); ++it)
        {
            todo.push_back(*it);
        }
        const Stats s = stats(i);
        if(i != 0 && s.cpuAvg < 0.02 && s.gpuAvg < 0.02)
        {
            continue;
        }
        char name[64];
        q_snprintf(name, sizeof(name), "%*s%s", 2 * n.depth, "", n.name);
        if(n.gpu)
        {
            Con_Printf("%-30s %6.2f %6.2f %6.2f %6.2f\n", name, s.cpuAvg, s.cpuMax, s.gpuAvg, s.gpuMax);
        }
        else
        {
            Con_Printf("%-30s %6.2f %6.2f\n", name, s.cpuAvg, s.cpuMax);
        }
    }
}

void report(std::int64_t now, bool full)
{
    if(intervalFrames == 0)
    {
        if(full)
        {
            Con_Printf("vr_profile: no frames collected%s\n", active ? " yet" : " (set vr_profile 1)");
        }
        return;
    }
    ++intervalNumber;
    const double seconds = static_cast<double>(now - intervalStart) / 1e9;
    writeFile(seconds);

    const Stats frame = stats(0);
    const double period = periodSum / intervalFrames;
    Con_Printf("vr_profile: %s, %d frames in %.1f s, %.2f ms apart (%.0f fps): host frame %.2f ms, CPU busy %.2f, GPU %.2f "
               "(max %.2f; eyes %.2f, runtime's calls %.2f)\n",
        captureMap.c_str(), intervalFrames, seconds, period, period > 0.0 ? 1000.0 / period : 0.0, frame.cpuAvg, cpuBusy(),
        frame.gpuAvg, frame.gpuMax, gpuEyes(), gpuNamed("xr acquire") + gpuNamed("xr submit"));
    if(full)
    {
        printTree();
        if(intervalHitches || intervalStalls)
        {
            Con_Printf("%d hitches (frames over %.0f ms, left out), %d GPU read stalls\n", intervalHitches, hitchMs,
                intervalStalls);
        }
    }
    Con_Printf("  top GPU: %s\n  top CPU: %s\n  -> %s\n", topLine(true, 6).c_str(), topLine(false, 6).c_str(),
        capturePath.c_str());
    resetInterval(now);
}

void startCapture(std::int64_t now)
{
    capturePath.clear();
    lastContext.clear();
    intervalNumber = 0;
    captureMap = mapName;
    resetInterval(now);
    dropGpu();
}

void updateOverlay(std::int64_t now)
{
    if(now - overlayUpdated < static_cast<std::int64_t>(overlayPeriod * 1e9) || intervalFrames == 0)
    {
        return;
    }
    overlayUpdated = now;
    const Stats frame = stats(0);
    overlayText.clear();
    appendf(overlayText, "CPU %.2f  GPU %.2f ms\neyes %.2f ms\n", cpuBusy(), frame.gpuAvg, gpuEyes());
    for(const Top& t : top(true, 6))
    {
        appendf(overlayText, "%-16.16s %5.2f\n", t.name, t.ms);
    }
    appendf(overlayText, "CPU:\n");
    for(const Top& t : top(false, 4))
    {
        appendf(overlayText, "%-16.16s %5.2f\n", t.name, t.ms);
    }
}

void dump_f()
{
    report(nowNs(), true);
}

} // namespace

const char* phaseName(Phase phase)
{
    return phaseInfo[phase].name;
}

bool phaseGpu(Phase phase)
{
    return phaseInfo[phase].gpu;
}

PhaseSums takePhases()
{
    PhaseSums s = phaseSums;
    s.displayPeriodMs = displayPeriod;
    phaseSums = PhaseSums{};
    return s;
}

void noteDisplayPeriod(double ms)
{
    displayPeriod = ms;
}

void begin(const char* name, bool gpu)
{
    beginPhase(name, gpu);
    if(!active)
    {
        return;
    }
    const int parent = stack.empty() ? 0 : stack.back().node;
    Open o{child(parent, name), nowNs(), -1};
    if(gpu && gpuOk)
    {
        GpuSlot& s = slots[slotIndex];
        o.gpuRec = static_cast<int>(s.recs.size());
        s.recs.push_back({o.node, query(s), -1, gpuDepth == 0});
        ++gpuDepth;
    }
    stack.push_back(o);
}

namespace
{

void endScope()
{
    if(!active || stack.empty())
    {
        return;
    }
    const Open o = stack.back();
    stack.pop_back();
    Node& n = nodes[o.node];
    n.frameNs += nowNs() - o.start;
    ++n.frameCalls;
    if(!n.cpuTouched)
    {
        n.cpuTouched = true;
        cpuTouched.push_back(o.node);
    }
    if(o.gpuRec >= 0)
    {
        GpuSlot& s = slots[slotIndex];
        s.recs[o.gpuRec].end = query(s);
        --gpuDepth;
    }
}

} // namespace

void end()
{
    endPhase();
    endScope();
}

void init()
{
    nodes.clear();
    Node root;
    root.name = "frame";
    nodes.push_back(root);
    Cmd_AddCommand("vr_profile_dump", dump_f);
}

void overlay()
{
    if(!active || vr_profile.value < 2.f || overlayText.empty())
    {
        return;
    }
    const hands::State& s = hands::current();
    if(!s.valid)
    {
        return;
    }
    // Over the wrist gadget's hand, facing the head.
    const float m2u = units::metresToUnits();
    const glm::vec3 at = s.pos[vr_gadget_hand.value != 0.f ? HAND_MAIN : HAND_OFF] + glm::vec3{0.f, 0.f, 0.16f * m2u};
    const glm::vec3 d = at - s.head;
    const float yaw = std::atan2(d.y, d.x) * 180.f / static_cast<float>(M_PI);
    text3d::queue(overlayText, at, glm::vec3{0.f, yaw, 0.f}, text3d::Align::Centre, 0.03f);
}

} // namespace qvr::profile

using namespace qvr;

extern "C" void VR_ProfileFrame()
{
    using namespace qvr::profile;
    if(nodes.empty() || cls.state == ca_dedicated)
    {
        return;
    }
    const std::int64_t now = nowNs();
    endPhaseFrame(now, frameStart, frameEnded ? frameEnd : now);

    if(active)
    {
        while(!stack.empty()) // a Host_Error jumped out of them
        {
            endScope();
        }
        Node& root = nodes[0];
        root.frameNs = (frameEnded ? frameEnd : now) - frameStart;
        root.frameCalls = 1;
        root.cpuTouched = true;
        cpuTouched.push_back(0);
        const double period = static_cast<double>(now - frameStart) / 1e6;
        const bool hitch = period > hitchMs;
        foldCpu(!hitch);
        GpuSlot& s = slots[slotIndex];
        if(hitch)
        {
            ++intervalHitches;
            s.recs.clear();
        }
        else
        {
            ++intervalFrames;
            periodSum += period;
            periodMax = std::max(periodMax, period);
        }
        s.pending = !s.recs.empty();
        slotIndex = (slotIndex + 1) % gpuSlots;

        // Read back what the GPU finished (oldest first; the ring's next slot is the oldest).
        for(int k = 0; k < gpuSlots; k++)
        {
            if(!resolve(slots[(slotIndex + k) % gpuSlots], false))
            {
                break;
            }
        }
        GpuSlot& next = slots[slotIndex];
        if(next.pending)
        {
            resolve(next, true);
            ++intervalStalls;
        }
        next.used = 0;
        next.recs.clear();
        gpuDepth = 0;
    }

    if(cl.mapname[0])
    {
        mapName = cl.mapname;
    }

    const bool want = vr_profile.value != 0.f;
    if(want && (!active || mapName != captureMap))
    {
        if(active && captureMap != "none" && now - intervalStart > 500'000'000)
        {
            report(now, false); // the last map's
        }
        startCapture(now);
    }
    else if(!want && active)
    {
        if(now - intervalStart > 500'000'000) // not just what followed a vr_profile_dump
        {
            report(now, false);
        }
        resetInterval(now);
        dropGpu();
    }
    else if(active && vr_profile_interval.value > 0.f &&
             static_cast<double>(now - intervalStart) / 1e9 >= vr_profile_interval.value)
    {
        report(now, false);
    }
    active = want;
    gpuOk = GL_QueryCounterFunc && GL_GetQueryObjectui64vFunc && GL_GetQueryObjectivFunc && GL_GenQueriesFunc;
    if(active && vr_profile.value >= 2.f)
    {
        updateOverlay(now);
    }
    frameStart = now;
    frameEnded = false;
}

extern "C" void VR_ProfileFrameEnd()
{
    if(!profile::frameEnded)
    {
        profile::frameEnd = profile::nowNs();
        profile::frameEnded = true;
    }
}

extern "C" void VR_ProfileBegin(const char* name)
{
    profile::begin(name, false);
}

extern "C" void VR_ProfileBeginGPU(const char* name)
{
    profile::begin(name, true);
}

extern "C" void VR_ProfileEnd()
{
    profile::end();
}
