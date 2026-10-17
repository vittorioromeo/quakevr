// vr_servertick.cpp -- the audit for a server tick on a worker thread (docs/vr-port/SERVER_THREAD.md, ROUND22.md).
//
// A listen server's tick (Host_ServerFrame: QuakeC, SV_Physics, Box3D) could run on a worker while the main thread
// draws the previous snapshot, if nothing on either side touched the other's state while they overlap. This file
// finds where they do, in a real game, before any thread is involved:
//
//   vr_server_thread_check 1   every access to the server's state from the main thread outside the tick (the server's
//                              VM switched in, a trace or link in its world: pr_edict.c, world.c), and every access to
//                              the client's from inside the tick (a sound started, a particle, a cvar set, a command
//                              queued, a console line), by the frame's phase and the caller (a stack hash, described
//                              once). Only the phases a threaded tick would overlap (screen, particles) are races.
//   vr_server_thread_check 2   the same, and the frame in the threaded order on the main thread: the screen (and the
//                              particles) first, then the tick, then the client's read. The server's results
//                              (vr_physics_hash, vr_bench_statehash) must not change: a change is state the drawing
//                              hands to the server outside the messages (the same order a worker would need).
//   vr_server_thread_report [reset]   the sites found (console), most frequent first.
//
// Off (0, the default) the choke points test one int.

#include "vr_engine.hpp"
#include "vr_cvars.hpp"

#include "Zancle/Algorithm/StableSort.hpp"
#include "Zancle/Container/Vector.hpp"

#include <stdint.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

extern "C" int vr_servertick_checking = 0; // vr_server_thread_check's value (vr_api.h's VR_SERVER_ACCESS tests it)

namespace qvr::servertick
{

namespace
{

enum Kind
{
    ServerFromMain, // the server's state from the main thread, outside the tick
    ClientFromTick, // the client's state from inside the tick
};

struct Site
{
    unsigned hash{0};
    Kind kind{ServerFromMain};
    const char* what{""};
    const char* phase{""};
    int count{0};
    char callers[320]{};
};

struct State
{
    bool inTick{false};
    bool deferred{false}; // check 2: this frame's tick runs after the screen
    const char* phase{"other"};
    za::Vector<Site> sites;
    int offMain{0}; // client accesses inside the tick from another thread (not recorded)
    int ticks{0};
};

State st;

constexpr int maxSites = 512;

unsigned stackHash()
{
#ifdef _WIN32
    void* pcs[10];
    const USHORT n = CaptureStackBackTrace(3, 10, pcs, nullptr);
    unsigned h = 2166136261u;
    for(USHORT i = 0; i < n; i++)
    {
        const auto a = reinterpret_cast<uintptr_t>(pcs[i]);
        h = (h ^ static_cast<unsigned>(a & 0xffffffffu)) * 16777619u;
    }
    return h;
#else
    return 0;
#endif
}

void note(Kind kind, const char* what)
{
    const unsigned h = stackHash() ^ (static_cast<unsigned>(kind) * 0x9e3779b9u);
    for(Site& s : st.sites)
    {
        if(s.hash == h && s.what == what && s.phase == st.phase)
        {
            s.count++;
            return;
        }
    }
    if(static_cast<int>(st.sites.size()) >= maxSites)
    {
        return;
    }
    Site s;
    s.hash = h;
    s.kind = kind;
    s.what = what;
    s.phase = st.phase;
    s.count = 1;
    if(!VR_DescribeCallers(s.callers, sizeof(s.callers), 2, 6))
    {
        q_strlcpy(s.callers, "(no stack)", sizeof(s.callers));
    }
    st.sites.pushBack(s);
}

void onCheck(cvar_t* var)
{
    vr_servertick_checking = static_cast<int>(var->value);
}

void report_f()
{
    if(Cmd_Argc() > 1 && !q_strcasecmp(Cmd_Argv(1), "reset"))
    {
        st.sites.clear();
        st.offMain = 0;
        st.ticks = 0;
        Con_Printf("vr_server_thread_report: cleared\n");
        return;
    }
    za::Vector<Site>& v = st.sites;
    za::stableSort(v.begin(), v.end(), [](const Site& a, const Site& b) { return a.count > b.count; });
    int races = 0;
    for(const Site& s : v)
    {
        races += s.kind == ClientFromTick || !strcmp(s.phase, "screen") || !strcmp(s.phase, "particles");
    }
    Con_Printf("vr_server_thread_report: %d ticks, %d sites (%d would race a threaded tick), %d off-main client accesses "
               "in the tick\n",
        st.ticks, static_cast<int>(v.size()), races, st.offMain);
    for(const Site& s : v)
    {
        const bool race = s.kind == ClientFromTick || !strcmp(s.phase, "screen") || !strcmp(s.phase, "particles");
        Con_Printf("%s %-6d %-13s %-10s %s\n", race ? "RACE" : "ok  ", s.count,
            s.kind == ServerFromMain ? "server state" : "client state", s.phase, s.what);
        Con_Printf("       %s\n", s.callers);
    }
}

} // namespace

void registerCommands()
{
    Cvar_SetCallback(&vr_server_thread_check, onCheck);
    vr_servertick_checking = static_cast<int>(vr_server_thread_check.value);
    Cmd_AddCommand("vr_server_thread_report", report_f);
}

} // namespace qvr::servertick

using namespace qvr::servertick;

extern "C" void VR_ServerTickPhase(const char* phase)
{
    st.phase = phase;
}

extern "C" void VR_ServerTickBegin(void)
{
    st.inTick = true;
    st.ticks++;
}

extern "C" void VR_ServerTickEnd(void)
{
    st.inTick = false;
}

extern "C" void VR_ServerAccess(const char* what)
{
    if(!st.inTick && VR_OnMainThread())
    {
        note(ServerFromMain, what);
    }
}

extern "C" void VR_ClientAccess(const char* what)
{
    if(!st.inTick)
    {
        return;
    }
    if(!VR_OnMainThread())
    {
        st.offMain++;
        return;
    }
    note(ClientFromTick, what);
}

extern "C" int VR_ServerTickDefer(int ticks)
{
    st.deferred = vr_servertick_checking == 2 && ticks == 1 && sv.active && cls.state == ca_connected && cls.signon == SIGNONS;
    return st.deferred;
}
