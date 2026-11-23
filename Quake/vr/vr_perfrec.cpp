// vr_perfrec.cpp -- the perf recorder (vr_perfrec.hpp).

#include "vr_perfrec.hpp"
#include "vr_engine.hpp"
#include "vr_cvars.hpp"
#include "vr_dynres.hpp"
#include "vr_lighting.hpp"
#include "vr_alloccount.hpp"
#include "vr_api.h"
#include "vr_tunables_api.hpp"

#include "Zancle/Algorithm/Fill.hpp"
#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Base/Strcmp.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Clamp.hpp"

#include "SDL.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

extern "C"
{
int vr_perf_recording = 0;
}

namespace qvr::perfrec
{

bool on = false;

namespace
{

constexpr const char* kindNames[VR_PERF_KINDS] = {"shader", "shaderbin", "texture", "model", "lightmap", "sound", "map",
    "save", "decal", "nav", "qc", "phys", "work", "mark"};

// The scopes whose time is an event kind's (vr_profile.cpp's begin/end), the outermost of each kind counted.
struct TaggedScope
{
    const char* name;
    int kind;
};
constexpr TaggedScope taggedScopes[] = {{"quakec", VR_PERF_QC}, {"box3d step", VR_PERF_PHYS},
    {"decal baked at once", VR_PERF_DECAL}, {"decals baked", VR_PERF_DECAL}, {"permanent decals bake", VR_PERF_DECAL},
    {"permdecal atlas grown", VR_PERF_DECAL}};

constexpr int detailSize = 256;
constexpr int ringSize = 16; // rows waiting for their GPU times (vr_profile.cpp keeps 6 frames' queries)
constexpr int flushRows = 72; // written out about once a second

struct Row
{
    za::U64 serial{0};
    double startS{0.0};
    float periodMs{0.f};
    float hostMs{0.f};
    float busyMs{0.f};
    float cpuMs[profile::PhaseCount]{};
    float gpuMs[profile::PhaseCount]{};
    float gpuSpanMs{0.f};
    int gpuState{0}; // 0 waiting, 1 read back, 2 none (dropped, or not timed)
    int xrMissed{0};
    float xrPeriodMs{0.f};
    float scale{0.f};
    int drawcalls{0};
    int dlights{0};
    int unlit{0}; // the frame's views whose world was drawn without the lights' loop (vr_gl_world_unlit)
    int split{0}; // ... drawn split by surface, the lit ones alone with the loop (vr_gl_world_unlit 2, C4v)
    za::U64 allocs{0};
    int count[VR_PERF_KINDS]{};
    float ms[VR_PERF_KINDS]{};
    char map[32]{};
    char detail[detailSize]{};
};

// The events of the frame under way (any thread: under the lock).
struct Pending
{
    int count[VR_PERF_KINDS]{};
    double ms[VR_PERF_KINDS]{};
    char detail[detailSize]{};
    int detailLen{0};
    int dropped{0}; // events whose names did not fit
    int xrMissed{0};
    float xrPeriodMs{0.f};
};

struct Recorder
{
    FILE* file{nullptr};
    char path[MAX_OSPATH]{};
    Row ring[ringSize];
    za::U64 nextWrite{0}; // the next serial to write (0: none yet)
    za::U64 newest{0};
    int unflushed{0};
    za::U64 rows{0};
    za::U64 allocs0{0};
    double started{0.0};
    SDL_SpinLock lock{0};
    Pending pending;
    int scopeOpen[VR_PERF_KINDS]{}; // the main thread's tagged scopes open (only the outermost counts)
};
Recorder rec;

void appendDetail(Pending& p, const char* kind, const char* name, double ms)
{
    char item[96];
    const int wanted = q_snprintf(item, sizeof item, "%s%s:%s:%.1f", p.detailLen ? "|" : "", kind, name ? name : "", ms);
    // (q_snprintf returns the length the text would have had: a long name, vr_perf_mark's, is cut at item's end)
    const int n = za::min(wanted, static_cast<int>(sizeof item) - 1);
    if(n <= 0 || p.detailLen + n >= detailSize - 1)
    {
        ++p.dropped;
        return;
    }
    for(int i = 0; i < n; i++) // (the CSV's separators and quotes out of the names)
    {
        const char c = item[i];
        p.detail[p.detailLen++] = c == ',' || c == '"' || c == '\n' || c == '\r' ? ' ' : c;
    }
    p.detail[p.detailLen] = 0;
}

[[nodiscard]] za::U64 allocsNow()
{
    const alloccount::Stats s = alloccount::statsThisThread();
    za::U64 sum = 0;
    for(int k = 0; k < static_cast<int>(sizeof(s.calls) / sizeof(s.calls[0])); k++) // (the allocations: not the frees)
    {
        if(k != static_cast<int>(alloccount::Kind::Delete) && k != static_cast<int>(alloccount::Kind::Free))
        {
            sum += s.calls[k];
        }
    }
    return sum;
}

[[nodiscard]] const char* columnName(const char* phase, char* out, int size)
{
    int i = 0;
    for(; phase[i] && i < size - 1; i++)
    {
        const char c = phase[i];
        out[i] = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ? c : '_';
    }
    out[i] = 0;
    return out;
}

void writeHeader()
{
    FILE* f = rec.file;
    const time_t now = time(nullptr);
    char when[64];
    strftime(when, sizeof when, "%Y-%m-%d %H:%M:%S", localtime(&now));
    fprintf(f, "# Quake VR perf recording (vr_perf_record), %s; GL %s\n", when, gl_renderer ? gl_renderer : "?");
    // (the Quest's: tuning constants mostly, found by their stand-ins: C4q, Cvar_FindVar never found them)
    const char* const quest[] = {"vr_gles_define", "vr_dlight_falloff", "vr_gl_lights_lite", "vr_gl_lights_plain",
        "vr_gl_alias_plain_mask", "vr_gl_world_lite", "vr_xr_foveation", "vr_xr_direct"};
    fprintf(f, "# settings:");
    for(const char* name : profile::settingCvars)
    {
        if(const cvar_t* v = Cvar_FindVar(name))
        {
            fprintf(f, " %s=%s", name, v->string);
        }
    }
    for(const char* name : quest)
    {
        const cvar_t* v = Cvar_FindVar(name);
        if(v || (v = tunableFind(name)) != nullptr)
        {
            fprintf(f, " %s=%s", name, v->string);
        }
    }
    fprintf(f, "\n# times in ms; c_ a phase's CPU, g_ its GPU (empty: 0, or not read back); gpu_span the GPU's first to last "
               "timestamp; xr_missed display periods missed; ev_<kind>_n/_ms the frame's events; events their names\n");
    fprintf(f, "serial,t,map,period,host,busy,xr_period,xr_missed,scale,gpu_span,draws,dlights,unlit,split,allocs");
    char col[48];
    for(int i = 0; i < profile::PhaseCount; i++)
    {
        fprintf(f, ",c_%s", columnName(profile::phaseName(static_cast<profile::Phase>(i)), col, sizeof col));
    }
    for(int i = 0; i < profile::PhaseCount; i++)
    {
        if(profile::phaseGpu(static_cast<profile::Phase>(i)))
        {
            fprintf(f, ",g_%s", columnName(profile::phaseName(static_cast<profile::Phase>(i)), col, sizeof col));
        }
    }
    for(const char* k : kindNames)
    {
        fprintf(f, ",ev_%s_n,ev_%s_ms", k, k);
    }
    fprintf(f, ",events\n");
}

void putMs(FILE* f, float ms)
{
    if(ms > 0.f)
    {
        fprintf(f, ",%.3f", static_cast<double>(ms));
    }
    else
    {
        fputc(',', f);
    }
}

void writeRow(const Row& r)
{
    FILE* f = rec.file;
    fprintf(f, "%llu,%.4f,%s,%.3f,%.3f,%.3f,%.3f,%d,%.3f", static_cast<unsigned long long>(r.serial), r.startS - rec.started,
        r.map, static_cast<double>(r.periodMs), static_cast<double>(r.hostMs), static_cast<double>(r.busyMs),
        static_cast<double>(r.xrPeriodMs), r.xrMissed, static_cast<double>(r.scale));
    if(r.gpuState == 1)
    {
        fprintf(f, ",%.3f", static_cast<double>(r.gpuSpanMs));
    }
    else
    {
        fputc(',', f);
    }
    fprintf(f, ",%d,%d,%d,%d,%llu", r.drawcalls, r.dlights, r.unlit, r.split, static_cast<unsigned long long>(r.allocs));
    for(int i = 0; i < profile::PhaseCount; i++)
    {
        putMs(f, r.cpuMs[i]);
    }
    for(int i = 0; i < profile::PhaseCount; i++)
    {
        if(profile::phaseGpu(static_cast<profile::Phase>(i)))
        {
            putMs(f, r.gpuState == 1 ? r.gpuMs[i] : 0.f);
        }
    }
    for(int k = 0; k < VR_PERF_KINDS; k++)
    {
        if(r.count[k])
        {
            fprintf(f, ",%d,%.3f", r.count[k], static_cast<double>(r.ms[k]));
        }
        else
        {
            fputs(",,", f);
        }
    }
    fprintf(f, ",%s\n", r.detail);
    ++rec.rows;
    if(++rec.unflushed >= flushRows)
    {
        fflush(f);
        rec.unflushed = 0;
    }
}

// Writes the rows in order: each once its GPU times are in (or given up), those up to `forceUpTo` whatever they wait for.
void drain(za::U64 forceUpTo)
{
    while(rec.nextWrite != 0 && rec.nextWrite <= rec.newest)
    {
        Row& r = rec.ring[rec.nextWrite % ringSize];
        if(r.serial != rec.nextWrite)
        {
            ++rec.nextWrite; // (never kept: recording began after it)
            continue;
        }
        if(r.gpuState == 0 && r.serial > forceUpTo && rec.newest - r.serial < ringSize - 2)
        {
            break;
        }
        writeRow(r);
        r.serial = 0;
        ++rec.nextWrite;
    }
}

void start()
{
    char dir[MAX_OSPATH];
#ifdef __ANDROID__
    const char* internal = SDL_AndroidGetInternalStoragePath();
    q_snprintf(dir, sizeof dir, "%s/perf", internal ? internal : com_gamedir);
#else
    q_snprintf(dir, sizeof dir, "%s/profile", com_gamedir);
    Sys_mkdir(dir);
    q_snprintf(dir, sizeof dir, "%s/profile/perf", com_gamedir);
#endif
    Sys_mkdir(dir);
    const time_t now = time(nullptr);
    char stamp[32];
    strftime(stamp, sizeof stamp, "%Y%m%d_%H%M%S", localtime(&now));
    q_snprintf(rec.path, sizeof rec.path, "%s/perf_%s.csv", dir, stamp);
    rec.file = fopen(rec.path, "w");
    if(!rec.file)
    {
        Con_Warning("vr_perf_record: can't write %s\n", rec.path);
        Cvar_SetValueQuick(&vr_perf_record, 0.f);
        return;
    }
    setvbuf(rec.file, nullptr, _IOFBF, 1 << 16);
    writeHeader();
    rec.nextWrite = 0;
    rec.newest = 0;
    rec.rows = 0;
    rec.unflushed = 0;
    rec.started = 0.0;
    rec.allocs0 = allocsNow();
    za::fill(rec.scopeOpen, 0);
    for(Row& r : rec.ring)
    {
        r.serial = 0;
    }
    SDL_AtomicLock(&rec.lock);
    rec.pending = Pending{};
    SDL_AtomicUnlock(&rec.lock);
    on = true;
    vr_perf_recording = 1;
    Con_Printf("vr_perf_record: recording to %s\n", rec.path);
}

void stop()
{
    on = false;
    vr_perf_recording = 0;
    if(!rec.file)
    {
        return;
    }
    drain(~za::U64{0});
    fclose(rec.file);
    rec.file = nullptr;
    Con_Printf("vr_perf_record: %llu frames written to %s\n", static_cast<unsigned long long>(rec.rows), rec.path);
}

} // namespace

int tagOf(const char* name)
{
    for(const TaggedScope& t : taggedScopes)
    {
        if(ZA_STRCMP(t.name, name) == 0)
        {
            return t.kind;
        }
    }
    return -1;
}

bool scopeBegin(int kind)
{
    if(!on || kind < 0 || kind >= VR_PERF_KINDS || rec.scopeOpen[kind])
    {
        return false; // (one of its kind open already: the outermost counts)
    }
    rec.scopeOpen[kind] = 1;
    return true;
}

void scopeEnd(int kind, za::I64 ns)
{
    if(kind >= 0 && kind < VR_PERF_KINDS)
    {
        rec.scopeOpen[kind] = 0;
        VR_PerfEvent(kind, static_cast<double>(ns) / 1e6, nullptr);
    }
}

void frame(za::U64 serial, double startS, double periodMs, double hostMs, double busyMs,
    const za::I64 (&phaseNs)[profile::PhaseCount], int drawcalls, bool gpuPending)
{
    if(on)
    {
        if(rec.started == 0.0)
        {
            rec.started = startS;
        }
        Row& r = rec.ring[serial % ringSize];
        if(r.serial != 0) // (not written yet: the ring's oldest, forced out)
        {
            drain(r.serial);
        }
        za::fill(rec.scopeOpen, 0); // (a Host_Error jumped out of one)
        r = Row{};
        r.serial = serial;
        r.startS = startS;
        r.periodMs = static_cast<float>(periodMs);
        r.hostMs = static_cast<float>(hostMs);
        r.busyMs = static_cast<float>(busyMs);
        for(int i = 0; i < profile::PhaseCount; i++)
        {
            r.cpuMs[i] = static_cast<float>(static_cast<double>(phaseNs[i]) / 1e6);
        }
        r.gpuState = gpuPending ? 0 : 2;
        r.scale = dynres::scale();
        r.drawcalls = drawcalls;
        for(const dlight_t& d : cl_dlights)
        {
            r.dlights += d.die >= cl.time && d.radius > 0.f ? 1 : 0;
        }
        r.unlit = lighting::takeWorldUnlitViews();
        r.split = lighting::takeWorldSplitViews();
        const za::U64 allocs = allocsNow();
        r.allocs = allocs - za::min(allocs, rec.allocs0);
        rec.allocs0 = allocs;
        q_strlcpy(r.map, cl.mapname[0] ? cl.mapname : "-", sizeof r.map);
        SDL_AtomicLock(&rec.lock);
        Pending p = rec.pending;
        rec.pending = Pending{};
        SDL_AtomicUnlock(&rec.lock);
        for(int k = 0; k < VR_PERF_KINDS; k++)
        {
            r.count[k] = p.count[k];
            r.ms[k] = static_cast<float>(p.ms[k]);
        }
        r.xrMissed = p.xrMissed;
        r.xrPeriodMs = p.xrPeriodMs > 0.f ? p.xrPeriodMs : static_cast<float>(profile::displayPeriodMs());
        if(p.dropped)
        {
            q_snprintf(r.detail, sizeof r.detail, "%s|+%d more", p.detail, p.dropped);
        }
        else
        {
            q_strlcpy(r.detail, p.detail, sizeof r.detail);
        }
        if(rec.nextWrite == 0)
        {
            rec.nextWrite = serial;
        }
        rec.newest = za::max(rec.newest, serial);
        drain(0);
    }

    // The setting, latched here: a file begun or ended between frames.
    const bool want = vr_perf_record.value != 0.f && cls.state != ca_dedicated;
    if(want && !on)
    {
        start();
    }
    else if(!want && on)
    {
        stop();
    }
}

void gpu(za::U64 serial, const double* phaseMs, double spanMs)
{
    if(!on || serial == 0)
    {
        return;
    }
    Row& r = rec.ring[serial % ringSize];
    if(r.serial != serial)
    {
        return;
    }
    if(phaseMs)
    {
        for(int i = 0; i < profile::PhaseCount; i++)
        {
            r.gpuMs[i] = static_cast<float>(phaseMs[i]);
        }
        r.gpuSpanMs = static_cast<float>(spanMs);
        r.gpuState = 1;
    }
    else
    {
        r.gpuState = 2;
    }
    drain(0);
}

// vr_test_dlights [<count> [<radius> [<distance>]]]: lasting dynamic lights as big as an explosion's, in a row before
// the eyes, so the cost of big lights is measured on a still scene (a fight's explosions come and go); 0 removes them
// (from agent/quest).
void testDlights_f()
{
    constexpr int key = -0x5E00;
    constexpr int maxLights = 8;
    const int count = Cmd_Argc() > 1 ? za::clamp(Q_atoi(Cmd_Argv(1)), 0, maxLights) : 1;
    const float radius = Cmd_Argc() > 2 ? static_cast<float>(Q_atof(Cmd_Argv(2))) : 350.f;
    const float dist = Cmd_Argc() > 3 ? static_cast<float>(Q_atof(Cmd_Argv(3))) : 160.f;
    if(!cl.worldmodel)
    {
        Con_Printf("vr_test_dlights: no map\n");
        return;
    }
    for(dlight_t& d : cl_dlights) // (the last ones out: only ours, never another light's slot taken to clear it)
    {
        if(d.key <= key && d.key > key - maxLights)
        {
            d.die = 0.f;
            d.radius = 0.f;
        }
    }
    vec3_t fwd, right, up;
    AngleVectors(cl.viewangles, fwd, right, up);
    for(int i = 0; i < count; i++)
    {
        dlight_t* dl = CL_AllocDlight(key - i);
        VectorMA(r_refdef.vieworg, dist, fwd, dl->origin);
        VectorMA(dl->origin, (static_cast<float>(i) - static_cast<float>(count - 1) * 0.5f) * 48.f, right, dl->origin);
        dl->radius = radius;
        dl->die = static_cast<float>(cl.time + 100000.0);
        dl->decay = 0.f;
        VR_TuneDlight(QVR_DLIGHT_EXPLOSION, 0, dl);
    }
    Con_Printf("vr_test_dlights: %d of radius %.0f, %.0f ahead\n", count, static_cast<double>(radius), static_cast<double>(dist));
}

// vr_dlights_list: the live dynamic lights, to tell whose lights a scene pays for (a negative key is the VR code's:
// vr_emissive.cpp's kinds, the hands' screens, the flashlight; 0 an effect's; a positive one an entity's).
void dlightsList_f()
{
    int n = 0;
    for(const dlight_t& d : cl_dlights)
    {
        if(d.die < cl.time || d.radius <= 0.f)
        {
            continue;
        }
        vec3_t to;
        VectorSubtract(d.origin, r_refdef.vieworg, to);
        Con_Printf("vr_dlights_list: key %d radius %.0f distance %.0f colour %.2f %.2f %.2f\n", d.key,
            static_cast<double>(d.radius), static_cast<double>(VectorLength(to)), static_cast<double>(d.color[0]),
            static_cast<double>(d.color[1]), static_cast<double>(d.color[2]));
        n++;
    }
    Con_Printf("vr_dlights_list: %d live\n", n);
}

void init()
{
    Cmd_AddCommand("vr_perf_mark", [] {
        VR_PerfEvent(VR_PERF_MARK, 0.0, Cmd_Argc() > 1 ? Cmd_Args() : "mark");
        if(!on)
        {
            Con_Printf("vr_perf_mark: not recording (vr_perf_record 1)\n");
        }
    });
    Cmd_AddCommand("vr_test_dlights", testDlights_f);
    Cmd_AddCommand("vr_dlights_list", dlightsList_f);
}

void xrFrame(int missed, double periodMs)
{
    if(!on)
    {
        return;
    }
    SDL_AtomicLock(&rec.lock);
    rec.pending.xrMissed += missed;
    rec.pending.xrPeriodMs = static_cast<float>(periodMs);
    SDL_AtomicUnlock(&rec.lock);
}

} // namespace qvr::perfrec

extern "C" void VR_PerfEvent(int kind, double ms, const char* name)
{
    using namespace qvr::perfrec;
    if(!on || kind < 0 || kind >= VR_PERF_KINDS)
    {
        return;
    }
    SDL_AtomicLock(&rec.lock);
    Pending& p = rec.pending;
    ++p.count[kind];
    p.ms[kind] += ms;
    if(kind != VR_PERF_QC && kind != VR_PERF_PHYS && kind != VR_PERF_DECAL)
    {
        appendDetail(p, kindNames[kind], name, ms);
    }
    SDL_AtomicUnlock(&rec.lock);
}
