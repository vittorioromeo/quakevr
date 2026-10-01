// vr_profile.cpp -- see vr_profile.hpp.
//
// Each scope is a node of a call tree, keyed by its parent and its name (the eyes' scopes are
// apart, under "eye L" and "eye R"). A frame adds up each node's CPU time and calls; at the frame's
// end they are folded into the interval's sums and maxima. GPU scopes also record a timestamp
// query at each end, into the frame's slot of a ring: a slot is read back once its last query
// is available (a few frames later), so the CPU never waits for the GPU; only when the ring comes
// round to a slot still pending is it read waiting (counted as a stall in the report).

#include "vr_profile.hpp"
#include "vr_profile_systems.hpp"
#include "vr_engine.hpp"
#include "vr_cvars.hpp"
#include "vr_main.hpp"

#include "Zancle/Algorithm/Fill.hpp"
#include "Zancle/Algorithm/Find.hpp"
#include "Zancle/Algorithm/Sort.hpp"
#include "Zancle/Base/GetArraySize.hpp"
#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Base/Strcmp.hpp"
#include "Zancle/Base/UIntPtrT.hpp"
#include "Zancle/Container/AnkerlUnorderedDense.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/String/String.hpp"
#include "vr_zancle.hpp"

#include <stdarg.h>
#include <stdio.h>
#include <time.h>

extern "C"
{
int vr_profile_on = 0;
int vr_profile_fine = 0;
int vr_profile_inqc = 0;
vr_profcounts_t vr_profcounts;
}

namespace qvr::profile
{

bool active = false;

namespace
{

// Frames longer than this (a map loading, a hitch) are counted apart, not in the averages.
constexpr double hitchMs = 250.0;
// GPU frames in flight before a slot is read waiting.
constexpr int gpuSlots = 6;

// The clock in nanoseconds (scopes of well under a microsecond, za::Clock's unit: qza::nowNs, vr_zancle.hpp).
using qza::nowNs;

struct Node
{
    const char* name{nullptr};
    int parent{-1};
    int depth{0};
    za::Vector<int> children;
    int system{0}; // vr_profile_systems.hpp: what its own time is
    int view{0};

    // This frame.
    za::I64 frameNs{0};
    int frameCalls{0};
    bool cpuTouched{false};
    double gpuFrameMs{0.0};
    bool gpuTouched{false};

    // This interval.
    double cpuSum{0.0}; // ms
    double cpuMax{0.0};
    za::I64 calls{0};
    double gpuSum{0.0};
    double gpuMax{0.0};
    bool gpu{false};
};

za::Vector<Node> nodes; // [0]: the frame
za::Vector<int> cpuTouched;
za::Vector<int> gpuTouched;

struct Open
{
    int node;
    za::I64 start;
    int gpuRec; // -1: CPU only
};
za::Vector<Open> stack;

struct GpuRec
{
    int node;
    int begin;
    int end;
    int parent; // the GPU scope it is in (a node), or -1: its time adds to the frame's GPU time
};

struct GpuSlot
{
    za::Vector<GLuint> queries;
    int used{0};
    za::Vector<GpuRec> recs;
    bool pending{false};
};
GpuSlot slots[gpuSlots];
int slotIndex = 0; // this frame's
int gpuDepth = 0;
bool gpuOk = false; // the GL calls there, and vr_profile_gpu (latched at the frame's start)

za::I64 frameStart = 0;
za::I64 frameEnd = 0; // VR_ProfileFrameEnd, when called (else the next frame's start)
bool frameEnded = false;
double periodSum = 0.0; // ms from a frame's start to the next's, idle time included
double periodMax = 0.0;
za::I64 intervalStart = 0;
int intervalFrames = 0;
int intervalGpuFrames = 0;
int intervalHitches = 0;
int intervalStalls = 0;
int intervalNumber = 0;

za::String capturePath; // this capture's file (empty: none written yet)
za::String captureMap;
za::String lastContext;
za::String mapName = "none";

bool recording = false; // the call tree's CSV (vr_profile), apart from the systems' (collecting: `active`)

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

ankerl::unordered_dense::map<const void*, int> phaseOfName; // a name literal's address -> its phase, or -1

struct PhaseOpen
{
    int phase; // -1: not a phase
    za::I64 start;
    int gpuRec; // -1: none
};
za::Vector<PhaseOpen> phaseStack;
za::I64 phaseFrameNs[PhaseCount]{};
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
    int sample{-1};            // its frame's place in the history (frameSample), if kept there,
    za::U64 serial{0};   // and that frame's serial (the place may have been reused since)
};
PhaseGpuSlot phaseSlots[phaseGpuSlots];

// The frames' history (frameSample): a ring, with each place's frame serial.
FrameSample history[frameHistorySize];
za::U64 historySerial[frameHistorySize]{};
za::U64 frameSerial = 0; // the frames recorded so far
int phaseSlot = 0;
bool phaseGpuMade = false;

// In front of the map: the last name seen in each of a few slots (by its address), since every
// scope's begin asks, vr_profile off too.
struct PhaseCacheEntry
{
    const char* name;
    int phase;
};
PhaseCacheEntry phaseCache[256]{};

[[nodiscard]] int phaseOf(const char* name)
{
    PhaseCacheEntry& cached = phaseCache[(reinterpret_cast<za::UIntPtrT>(name) * 0x9E3779B97F4A7C15ull) >> 56];
    if(cached.name == name)
    {
        return cached.phase;
    }
    const auto it = phaseOfName.find(name);
    if(it != phaseOfName.end())
    {
        cached = {name, it->second};
        return it->second;
    }
    int phase = -1;
    for(int i = 0; i < PhaseCount; i++)
    {
        if(ZA_STRCMP(phaseInfo[i].name, name) == 0)
        {
            phase = i;
            break;
        }
    }
    phaseOfName.emplace(name, phase);
    cached = {name, phase};
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
    phaseStack.pushBack(o);
}

void endPhase()
{
    if(phaseStack.empty())
    {
        return;
    }
    const PhaseOpen o = phaseStack.back();
    phaseStack.popBack();
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
    double eyes = 0.0;
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
        const double ms = e > b ? static_cast<double>(e - b) / 1e6 : 0.0;
        phaseSums.gpuMs[r.phase] += ms;
        eyes += r.phase == EyeL || r.phase == EyeR ? ms : 0.0;
    }
    ++phaseSums.gpuFrames;
    if(s.sample >= 0 && historySerial[s.sample] == s.serial)
    {
        history[s.sample].gpuMs = static_cast<float>(eyes);
    }
    s.pending = false;
    return true;
}

// The frame's end for the phases (VR_ProfileFrame): its times into the sums, its GPU slot sent off,
// the finished slots read back.
void endPhaseFrame(za::I64 now, za::I64 start, za::I64 end)
{
    phaseStack.clear(); // a Host_Error jumped out of them, or a dialog's frame inside one
    const double period = start > 0 ? static_cast<double>(now - start) / 1e6 : 0.0;
    const bool keep = start > 0 && period <= hitchMs;
    PhaseGpuSlot& s = phaseSlots[phaseSlot];
    if(keep)
    {
        ++phaseSums.frames;
        phaseSums.periodMs += period;
        phaseSums.periodMaxMs = za::max(phaseSums.periodMaxMs, period);
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
    s.sample = -1;
    if(start > 0)
    {
        const za::I64 waits = phaseFrameNs[XrWait] + phaseFrameNs[XrAcquire] + phaseFrameNs[XrRelease] +
                                   phaseFrameNs[XrSubmit] + phaseFrameNs[Swap];
        const int at = static_cast<int>(frameSerial % frameHistorySize);
        history[at] = {static_cast<double>(start) / 1e9, static_cast<float>(period),
            static_cast<float>(za::max<za::I64>(0, end - start - waits)) / 1e6f, -1.f};
        historySerial[at] = ++frameSerial;
        s.sample = at;
        s.serial = frameSerial;
    }
    za::fill(phaseFrameNs, phaseFrameNs + za::getArraySize(phaseFrameNs), za::I64{0});
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
    // By the name's address first (the same literal: nearly always), then by its text.
    for(int c : nodes[parent].children)
    {
        if(nodes[c].name == name)
        {
            return c;
        }
    }
    for(int c : nodes[parent].children)
    {
        if(ZA_STRCMP(nodes[c].name, name) == 0)
        {
            return c;
        }
    }
    Node n;
    n.name = name;
    n.parent = parent;
    n.depth = nodes[parent].depth + 1;
    systems::classify(name, nodes[parent].system, nodes[parent].view, n.system, n.view);
    nodes.pushBack(n);
    const int index = static_cast<int>(nodes.size()) - 1;
    nodes[parent].children.pushBack(index);
    return index;
}

[[nodiscard]] int query(GpuSlot& s)
{
    if(s.used == static_cast<int>(s.queries.size()))
    {
        const za::SizeT old = s.queries.size();
        s.queries.resize(old + 64);
        GL_GenQueriesFunc(64, s.queries.data() + old);
    }
    GL_QueryCounterFunc(s.queries[s.used], GL_TIMESTAMP);
    return s.used++;
}

void resetInterval(za::I64 now)
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

// This frame's costliest scopes by their own time (less their child scopes'), with their paths: the hitch log's detail.
[[nodiscard]] za::String frameTopScopes()
{
    struct Self
    {
        za::I64 ns;
        int node;
    };
    za::Vector<Self> self;
    for(int i : cpuTouched)
    {
        za::I64 ns = nodes[i].frameNs;
        for(int c : nodes[i].children)
        {
            ns -= nodes[c].frameNs; // (0 for those not run this frame)
        }
        self.pushBack({ns, i});
    }
    za::quickSort(self.begin(), self.end(), [](const Self& a, const Self& b) { return a.ns > b.ns; });
    za::String out;
    for(za::SizeT k = 0; k < self.size() && k < 3; k++)
    {
        za::String p = nodes[self[k].node].name;
        for(int n = nodes[self[k].node].parent; n > 0; n = nodes[n].parent)
        {
            p = za::String{nodes[n].name} + "/" + p;
        }
        char buf[32];
        q_snprintf(buf, sizeof(buf), " %.1f", static_cast<double>(self[k].ns) / 1e6);
        out += (k ? ", " : "") + p + buf;
    }
    return out;
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
            n.cpuMax = za::max(n.cpuMax, ms);
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
        gpuTouched.pushBack(node);
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
        systems::gpu(nodes[r.node].system, nodes[r.node].view, ms);
        if(r.parent < 0)
        {
            touchGpu(0, ms);
        }
        else
        {
            systems::gpu(nodes[r.parent].system, nodes[r.parent].view, -ms);
        }
    }
    systems::gpuFrameDone(s.used);
    for(int i : gpuTouched)
    {
        Node& n = nodes[i];
        n.gpuSum += n.gpuFrameMs;
        n.gpuMax = za::max(n.gpuMax, n.gpuFrameMs);
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
    const double frames = za::max(intervalFrames, 1);
    const double gpuFrames = za::max(intervalGpuFrames, 1);
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
    s.cpuSelf = za::max(0.0, s.cpuAvg - cpuChildren);
    s.gpuSelf = n.gpu ? za::max(0.0, s.gpuAvg - gpuChildren) : 0.0;
    if(i == 0)
    {
        s.calls = 1.0;
    }
    return s;
}

[[nodiscard]] double sumNamed(const char* name)
{
    double sum = 0.0;
    for(za::SizeT i = 1; i < nodes.size(); i++)
    {
        if(ZA_STRCMP(nodes[i].name, name) == 0)
        {
            sum += nodes[i].cpuSum;
        }
    }
    return sum / za::max(intervalFrames, 1);
}

// GPU time of the scopes so named (summed over the tree), per frame.
[[nodiscard]] double gpuNamed(const char* name)
{
    double sum = 0.0;
    for(za::SizeT i = 1; i < nodes.size(); i++)
    {
        if(ZA_STRCMP(nodes[i].name, name) == 0)
        {
            sum += nodes[i].gpuSum;
        }
    }
    return sum / za::max(intervalGpuFrames, 1);
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
    return za::max(0.0, stats(0).cpuAvg - sumNamed("xr wait") - sumNamed("swap"));
}

struct Top
{
    const char* name;
    double ms;
};

// Scopes by self time (the same name summed over the tree: both eyes), costliest first.
[[nodiscard]] za::Vector<Top> top(bool gpu, za::SizeT count)
{
    za::Vector<Top> all;
    for(za::SizeT i = 1; i < nodes.size(); i++)
    {
        const Stats s = stats(static_cast<int>(i));
        const double ms = gpu ? s.gpuSelf : s.cpuSelf;
        if(ms <= 0.0)
        {
            continue;
        }
        auto it = za::findIf(all.begin(), all.end(), [&](const Top& t) { return ZA_STRCMP(t.name, nodes[i].name) == 0; });
        if(it != all.end())
        {
            it->ms += ms;
        }
        else
        {
            all.pushBack({nodes[i].name, ms});
        }
    }
    za::quickSort(all.begin(), all.end(), [](const Top& a, const Top& b) { return a.ms > b.ms; });
    if(all.size() > count)
    {
        all.resize(count);
    }
    return all;
}

[[nodiscard]] za::String path(int i)
{
    za::String p = nodes[i].name;
    for(int n = nodes[i].parent; n >= 0; n = nodes[n].parent)
    {
        p = za::String{nodes[n].name} + "/" + p;
    }
    return p;
}

void appendf(za::String& out, const char* fmt, ...)
{
    char buf[512];
    va_list args;
    va_start(args, fmt);
    q_vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    out += buf;
}

// Map, resolution and the graphics settings: "# key,value" lines.
[[nodiscard]] za::String context()
{
    za::String c;
    appendf(c, "# map,%s\n", captureMap.cStr());
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
        const time_t now = time(nullptr);
        char stamp[64];
        strftime(stamp, sizeof(stamp), "%Y-%m-%d_%H-%M-%S", localtime(&now));
        const za::String dir = za::String{com_gamedir} + "/profile";
        Sys_mkdir(dir.cStr());
        capturePath = dir + "/profile_" + captureMap + "_" + stamp + ".csv";
    }

    FILE* f = fopen(capturePath.cStr(), "a");
    if(!f)
    {
        Con_Warning("vr_profile: can't write %s\n", capturePath.cStr());
        return;
    }

    const za::String ctx = context();
    if(created)
    {
        const time_t now = time(nullptr);
        char when[64];
        strftime(when, sizeof(when), "%Y-%m-%d %H:%M:%S", localtime(&now));
        fprintf(f, "# Quake VR profile (vr_profile), %s\n", when);
        fprintf(f, "# per interval and scope: calls and milliseconds per frame, averaged over its frames; "
                        "max is the worst frame; self excludes the child scopes\n");
        fputs(ctx.cStr(), f);
        fprintf(f, "interval,seconds,frames,gpu_frames,hitches,gpu_stalls,depth,scope,path,calls,cpu_avg_ms,"
                        "cpu_max_ms,cpu_self_ms,gpu_avg_ms,gpu_max_ms,gpu_self_ms\n");
    }
    else if(ctx != lastContext)
    {
        fprintf(f, "# settings changed before interval %d:\n", intervalNumber);
        fputs(ctx.cStr(), f);
    }
    lastContext = ctx;

    // The frame period (a frame's start to the next's, the frame rate cap's idle time included), then
    // the scopes depth first, in the order they were first seen ("frame": _Host_Frame's time; its GPU
    // time is its outermost GPU scopes').
    fprintf(f, "%d,%.2f,%d,%d,%d,%d,0,frame period,frame period,1.00,%.3f,%.3f,,,,\n", intervalNumber, seconds,
        intervalFrames, intervalGpuFrames, intervalHitches, intervalStalls, periodSum / za::max(intervalFrames, 1),
        periodMax);
    za::Vector<int> todo{0};
    while(!todo.empty())
    {
        const int i = todo.back();
        todo.popBack();
        const Node& n = nodes[i];
        for(za::SizeT k = n.children.size(); k-- > 0;) // (the last first)
        {
            todo.pushBack(n.children[k]);
        }
        if(i != 0 && n.calls == 0 && n.gpuSum == 0.0)
        {
            continue;
        }
        const Stats s = stats(i);
        fprintf(f, "%d,%.2f,%d,%d,%d,%d,%d,%s,%s,%.2f,%.3f,%.3f,%.3f,", intervalNumber, seconds, intervalFrames,
            intervalGpuFrames, intervalHitches, intervalStalls, n.depth, n.name, path(i).cStr(), s.calls, s.cpuAvg,
            s.cpuMax, s.cpuSelf);
        if(n.gpu)
        {
            fprintf(f, "%.3f,%.3f,%.3f\n", s.gpuAvg, s.gpuMax, s.gpuSelf);
        }
        else
        {
            fprintf(f, ",,\n");
        }
    }
    fclose(f);
}

[[nodiscard]] za::String topLine(bool gpu, za::SizeT count)
{
    za::String line;
    for(const Top& t : top(gpu, count))
    {
        appendf(line, "%s%s %.2f", line.empty() ? "" : ", ", t.name, t.ms);
    }
    return line;
}

void printTree()
{
    Con_Printf("%-30s %6s %6s %6s %6s\n", "scope (ms/frame)", "cpu", "max", "gpu", "max");
    za::Vector<int> todo{0};
    while(!todo.empty())
    {
        const int i = todo.back();
        todo.popBack();
        const Node& n = nodes[i];
        for(za::SizeT k = n.children.size(); k-- > 0;) // (the last first)
        {
            todo.pushBack(n.children[k]);
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

void report(za::I64 now, bool full)
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
        captureMap.cStr(), intervalFrames, seconds, period, period > 0.0 ? 1000.0 / period : 0.0, frame.cpuAvg, cpuBusy(),
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
    Con_Printf("  top GPU: %s\n  top CPU: %s\n  -> %s\n", topLine(true, 6).cStr(), topLine(false, 6).cStr(),
        capturePath.cStr());
    resetInterval(now);
}

void startCapture(za::I64 now)
{
    capturePath.clear();
    lastContext.clear();
    intervalNumber = 0;
    captureMap = mapName;
    resetInterval(now);
    dropGpu();
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

double displayPeriodMs()
{
    return displayPeriod;
}

bool frameSample(int back, FrameSample& out)
{
    if(back < 0 || back >= frameHistorySize || static_cast<za::U64>(back) >= frameSerial)
    {
        return false;
    }
    out = history[(frameSerial - 1 - static_cast<za::U64>(back)) % frameHistorySize];
    return true;
}

double nowSeconds()
{
    return static_cast<double>(nowNs()) / 1e9;
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
        int gpuParent = -1;
        for(za::SizeT k = stack.size(); k-- > 0;) // (the innermost first)
        {
            const auto it = &stack[k];
            if(it->gpuRec >= 0)
            {
                gpuParent = it->node;
                break;
            }
        }
        o.gpuRec = static_cast<int>(s.recs.size());
        s.recs.pushBack({o.node, query(s), -1, gpuParent});
        ++gpuDepth;
    }
    stack.pushBack(o);
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
    stack.popBack();
    Node& n = nodes[o.node];
    const za::I64 ns = nowNs() - o.start;
    n.frameNs += ns;
    ++n.frameCalls;
    // Its own time to its system; its parent's (whose time holds it) less.
    const Node& p = nodes[n.parent];
    if(n.system != p.system || n.view != p.view)
    {
        systems::cpu(n.system, n.view, ns);
        systems::cpu(p.system, p.view, -ns);
    }
    if(!n.cpuTouched)
    {
        n.cpuTouched = true;
        cpuTouched.pushBack(o.node);
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
    root.system = systems::rootSystem();
    root.view = systems::rootView();
    nodes.pushBack(root);
    Cmd_AddCommand("vr_profile_dump", dump_f);
    systems::init();
}

void overlay()
{
    systems::overlay();
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
    const za::I64 now = nowNs();
    endPhaseFrame(now, frameStart, frameEnded ? frameEnd : now);
    const vr_profcounts_t counts = vr_profcounts; // the frame's (counted whatever vr_profile is)
    vr_profcounts = vr_profcounts_t{};
    vr_profile_inqc = 0; // a Host_Error may have jumped out of a timed PR_ExecuteProgram

    if(cl.mapname[0])
    {
        mapName = cl.mapname;
    }

    if(active)
    {
        while(!stack.empty()) // a Host_Error jumped out of them
        {
            endScope();
        }
        Node& root = nodes[0];
        const za::I64 hostNs = (frameEnded ? frameEnd : now) - frameStart;
        root.frameNs = hostNs;
        root.frameCalls = 1;
        root.cpuTouched = true;
        cpuTouched.pushBack(0);
        const double period = static_cast<double>(now - frameStart) / 1e6;
        const bool hitch = period > hitchMs;
        const double hitchAt = systems::hitchMs();
        const za::String hitchScopes = hitchAt > 0.0 && static_cast<double>(hostNs) / 1e6 > hitchAt ? frameTopScopes() : "";
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
            periodMax = za::max(periodMax, period);
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

        systems::frameEnd(now, now - frameStart, hostNs,
            systems::Counts{counts.traces, counts.hullchecks, counts.drawcalls, counts.aliasdrawn}, hitchMs,
            mapName.cStr(), hitchScopes);
    }

    // Collecting: for the call tree's CSV (vr_profile), the panel or the systems' CSV. The call tree's capture starts
    // with vr_profile, and anew at each map (a file each).
    const bool record = vr_profile.value != 0.f;
    const bool want = record || vr_profile_overlay.value != 0.f || vr_profile_csv.value != 0.f;
    const bool longEnough = now - intervalStart > 500'000'000; // not just what followed a vr_profile_dump
    if(recording && (!record || !want || mapName != captureMap))
    {
        if(captureMap != "none" && longEnough)
        {
            report(now, false); // the last map's, or the capture's end
        }
        recording = false;
    }
    else if(recording && vr_profile_interval.value > 0.f &&
             static_cast<double>(now - intervalStart) / 1e9 >= vr_profile_interval.value)
    {
        report(now, false);
    }
    if(want != active)
    {
        if(want)
        {
            systems::start(now);
        }
        else
        {
            systems::stop();
        }
        resetInterval(now);
        dropGpu();
    }
    if(record && want && !recording)
    {
        startCapture(now);
        recording = true;
    }
    active = want;
    vr_profile_on = want ? 1 : 0;
    vr_profile_fine = want && vr_profile_detail.value >= 2.f ? 1 : 0;
    // The GPU's times on one frame in vr_profile_gpu: each timer query stalls the GPU's pipeline a little (about 0.12 ms
    // of the mock's eyes a frame for all the scopes' queries), so by default one frame in 4 is timed, and averaged.
    static unsigned gpuFrameCount = 0;
    const int gpuEvery = static_cast<int>(vr_profile_gpu.value);
    gpuOk = GL_QueryCounterFunc && GL_GetQueryObjectui64vFunc && GL_GetQueryObjectivFunc && GL_GenQueriesFunc &&
            gpuEvery >= 1 && gpuFrameCount++ % static_cast<unsigned>(gpuEvery) == 0;
    if(active)
    {
        systems::profilerTime(nowNs() - now); // this frame's share of the profiler's own work (its files, its panel)
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
