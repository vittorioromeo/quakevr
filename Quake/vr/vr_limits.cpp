// vr_limits.cpp -- the engine's and the port's hardcoded limits: how close the game is to each (the vr_limits command,
// Debug > Reports > Limits), and a loud warning, once a session, for the ones whose overflow used to be silent.
//
// ROUND21.md, "Hardcoded limits audit": each limit, its value, the usage measured and what happens past it. The
// limits that grew with the port (the cvars, the command buffer, a command's length, the zone) have no fixed size any
// more or a large one; the ones listed here still have one (most are the network protocol's or the renderer's).

#include "vr_limits.hpp"

#include "vr_cvars.hpp"
#include "vr_decals.hpp"
#include "vr_engine.hpp"
#include "vr_mem.hpp"
#include "vr_particles.hpp"
#include "vr_props.hpp"
#include "vr_protocol.hpp"
#include "vr_weapons.hpp"

#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Container/Array.hpp"


namespace qvr::limits
{

namespace
{

struct Hit
{
    const char* name;
    const char* what;
    int count;
};

za::Array<Hit, QVR_LIMIT_COUNT> hits{{
    {"MAX_TEMP_ENTITIES", "temporary entities (beams' segments, torch flames) not drawn", 0},
    {"MAX_DLIGHTS", "dynamic lights taking over the first one", 0},
    {"datagram", "frames whose farther entities were not sent (Packet overflow)", 0},
}};

// One row: its usage against its maximum; a usage at 80% or more of the maximum is highlighted.
void row(const char* name, int used, int max, const char* note)
{
    const bool high = max > 0 && used * 5 >= max * 4;
    Con_Printf("%s%-22s %9d / %-9d %3d%%  %s\n", high ? "\x02" : "", name, used, max, max > 0 ? used * 100 / max : 0, note);
}

void rowPeak(const char* name, int used, int peak, int max, const char* note)
{
    const bool high = max > 0 && peak * 5 >= max * 4;
    Con_Printf("%s%-22s %9d / %-9d %3d%%  peak %d. %s\n", high ? "\x02" : "", name, used, max, max > 0 ? peak * 100 / max : 0, peak,
        note);
}

void rowInfo(const char* name, const char* text)
{
    Con_Printf("%-22s %s\n", name, text);
}

int precacheCount(const char* const* list, int max)
{
    int n = 1; // 0 is none
    while(n < max && list[n])
    {
        n++;
    }
    return n;
}

int staticSoundChannels()
{
    return total_channels > MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS ? total_channels - MAX_DYNAMIC_CHANNELS - NUM_AMBIENTS : 0; // 0 without sound
}

} // namespace

void hit(int limit)
{
    if(limit < 0 || limit >= QVR_LIMIT_COUNT)
    {
        return;
    }
    Hit& h = hits[static_cast<za::SizeT>(limit)];
    if(h.count++ == 0)
    {
        Con_Printf("\x02" "Limit reached: %s: %s (vr_limits)\n", h.name, h.what);
    }
}

double lastTimeCommand = 0.0; // "vr_limits time"'s last (Sys_DoubleTime; 0: none)

void command_f()
{
    // "vr_limits time": the time since the last one (a stress test's timing: vr_limits time; exec big.cfg; vr_limits time).
    if(Cmd_Argc() > 1 && !strcmp(Cmd_Argv(1), "time"))
    {
        double& last = lastTimeCommand;
        const double now = Sys_DoubleTime();
        Con_Printf("vr_limits time: %.1f ms since the last\n", last > 0.0 ? (now - last) * 1000.0 : 0.0);
        last = now;
        return;
    }
    // "vr_limits stress <n>": n more cvars (qvr_stress_00000...), not saved: the stress test of the cvar count (ROUND21.md,
    // "Hardcoded limits audit": a config setting 50,000 of them).
    if(Cmd_Argc() > 2 && !strcmp(Cmd_Argv(1), "stress"))
    {
        const int n = Q_atoi(Cmd_Argv(2));
        const double start = Sys_DoubleTime();
        for(int i = 0; i < n; i++)
        {
            Cvar_Create(va("qvr_stress_%05d", i), "0");
        }
        Con_Printf("vr_limits stress: %d cvars in %.1f ms (%d in all)\n", n, (Sys_DoubleTime() - start) * 1000.0, Cvar_Count());
        return;
    }
    // "vr_limits cvarlen <name>": a cvar value's length (Con_Printf shows at most 4095 characters of it).
    if(Cmd_Argc() > 2 && !strcmp(Cmd_Argv(1), "cvarlen"))
    {
        const cvar_t* var = Cvar_FindVar(Cmd_Argv(2));
        Con_Printf("vr_limits cvarlen: %s: %d characters, ends \"%s\"\n", Cmd_Argv(2), var ? static_cast<int>(strlen(var->string)) : -1,
            var ? var->string + (strlen(var->string) > 8 ? strlen(var->string) - 8 : 0) : "");
        return;
    }

    Con_Printf("Limits: used / maximum, %% of the maximum (the peak's, when there is one). Highlighted: 80%% or more.\n");

    Con_Printf("\x02" "Console and settings\n");
    row("cvars", Cvar_Count(), 0, "no maximum (was 16384); a hash map");
    row("commands", Cmd_CommandCount(), 0, "no maximum");
    row("aliases", Cmd_AliasCount(), 0, "no maximum");
    rowPeak("command buffer, bytes", 0, cmd_limits.cbuf_peak, 0, "grows (was 256 KiB, then 4 MiB)");
    rowPeak("command line, chars", 0, cmd_limits.longest_line, 0, "any length (was 1023, cut silently)");
    rowPeak("argument, chars", 0, cmd_limits.longest_token, 0, "any length (was 1023, cut silently)");
    rowPeak("arguments", 0, cmd_limits.max_argc, 1024, "was 80 (the rest dropped silently); now warned");

    Con_Printf("\x02" "Memory\n");
    {
        int used = 0;
        int peak = 0;
        int size = 0;
        Z_Usage(&used, &peak, &size);
        rowPeak("zone, KiB", used / 1024, peak / 1024, size / 1024, "-zone <KiB>; full: Sys_Error (was 4 MiB)");
        int segments = 0;
        int maxSegments = 0;
        Hunk_Usage(&used, &peak, &size, &segments, &maxSegments);
        rowPeak("hunk, MiB", used >> 20, peak >> 20, size >> 20, "grows by segments (-heapsize <KiB> the first)");
        row("hunk segments", segments, maxSegments, "each twice the last; past the last: Sys_Error");
        const mem::Totals held = mem::totals();
        rowInfo("VR scratch, KiB", va("%9d in %d sets (given back at each map; vr_memstats: the largest)",
                                       static_cast<int>(held.scratchBytes / 1024), held.scratchSets));
        rowInfo("VR caches, KiB", va("%9d in %d sets (emptied on their events)", static_cast<int>(held.cacheBytes / 1024),
                                      held.cacheSets));
    }

    Con_Printf("\x02" "Resources (for the whole session: never freed between maps)\n");
    row("models known", Mod_KnownCount(), Mod_KnownMax(), "full: Sys_Error (freed only by a game change)");
    row("sounds known", S_KnownSfxCount(), S_KnownSfxMax(), "full: Sys_Error (never freed)");
    row("textures", TexMgr_Count(), TexMgr_Max(), "full: Sys_Error");
    row("menu pictures", menu_numcachepics, Draw_CachedPicsMax(), "full: Sys_Error");

    Con_Printf("\x02" "The map\n");
    if(sv.active)
    {
        const int free = sv.qcvm.max_edicts - sv.qcvm.num_edicts;
        rowPeak("edicts", sv.qcvm.num_edicts, dev_peakstats.edicts, sv.qcvm.max_edicts,
            va("max_edicts; %d never used (debris keeps 2048 free); full: Host_Error", free));
        row("models precached", precacheCount(sv.model_precache, MAX_MODELS), MAX_MODELS, "full: Host_Error (protocol 999: 16 bits)");
        row("sounds precached", precacheCount(sv.sound_precache, MAX_SOUNDS), MAX_SOUNDS, "full: Host_Error");
        int styles = 0;
        for(const char* s : sv.lightstyles)
        {
            styles += s && s[0];
        }
        row("light styles", styles, MAX_LIGHTSTYLES, "the protocol's; past it: ignored");
        row("signon buffers", sv.num_signon_buffers, MAX_SIGNON_BUFFERS, "31500 bytes each; full: Host_Error");
        rowPeak("datagram, bytes", dev_stats.packetsize, dev_peakstats.packetsize, MAX_DATAGRAM,
            "the local client's; remote: 1400 (the entities past it wait a frame)");
    }
    else
    {
        rowInfo("server", "not running (the server's limits are shown during a game)");
    }
    row("static entities", cl.num_statics, MAX_STATIC_ENTITIES, "full: Host_Error");
    rowPeak("entities drawn", cl_numvisedicts, dev_peakstats.visedicts, MAX_VISEDICTS, "the rest not drawn");
    rowPeak("temp entities", num_temp_entities, dev_peakstats.tempents, MAX_TEMP_ENTITIES, "the rest not drawn (warned)");
    rowPeak("beams", dev_stats.beams, dev_peakstats.beams, MAX_BEAMS, "full: \"Beam list overflow!\", dropped");
    rowPeak("dynamic lights", dev_stats.dlights, dev_peakstats.dlights, MAX_DLIGHTS,
        "the renderer's (one bit each); full: the first taken over (warned)");
    row("static sounds", staticSoundChannels(), MAX_CHANNELS - MAX_DYNAMIC_CHANNELS - NUM_AMBIENTS, "full: dropped, printed");
    row("stats", protocol::STAT_QVR_END, MAX_CL_STATS, "fixed at build (STAT_QVR_*)");
    if(qcvm && qcvm->progs)
    {
        row("QC known strings", qcvm->numknownstrings, qcvm->maxknownstrings, "grows");
        row("QC fields (words)", qcvm->progs->entityfields, 0, "no maximum; each edict this many words");
        row("QC globals (words)", qcvm->progs->numglobals, 65535, "progs version 6: 16-bit offsets");
    }

    Con_Printf("\x02" "Quake VR\n");
    row("prop slots", props::slotsUsed(), props::numSlots, "full: a new prop keeps its defaults (printed)");
    row("weapon slots", weapons::slotsUsed(), weapons::numSlots, "a weapon without one uses the defaults");
    row("decals", decals::liveCount(), static_cast<int>(vr_decal_max.value), "vr_decal_max; the oldest go first");
    row("particles", particles::liveCount(), particles::capacity(), "the rest not spawned");

    Con_Printf("\x02" "Overflows this session (were silent)\n");
    for(const Hit& h : hits)
    {
        Con_Printf("%s%-22s %9d  %s\n", h.count ? "\x02" : "", h.name, h.count, h.what);
    }
}

} // namespace qvr::limits

extern "C" void VR_LimitHit(int limit)
{
    qvr::limits::hit(limit);
}
