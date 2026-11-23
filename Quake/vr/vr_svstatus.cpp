// vr_svstatus.cpp -- a dedicated server's status line (MP_PLAN.md, B17): a server with no screen says in its console
// what it runs and who is in, every `vr_sv_status` seconds of real time (0: never) and at once when a player comes or
// goes:
//   status: e1m1 (coop, skill 1) | 2 of 8 players: A 42 ms, B 85 ms | map 0:12:03, up 1:02:03
// `vr_sv_status_now` prints it at once (a listen host too). `status` gives each player's ping as well (host_cmd.c,
// VR_ClientPingMs). The pings are the server's own measure of each player's moves (sv_user.c's ping_times), as the
// scoreboard's `ping`.

#include "vr_api.h"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "Zancle/Base/SizeT.hpp"

#include <cstdio>

namespace qvr::svstatus
{

namespace
{

struct State
{
    double started{-1.0}; // realtime of the first frame with a map running
    double next{0.0};     // realtime of the next periodic line
    int players{-1};      // the count at the last line (-1: none printed)
};
State state;

constexpr int lineLength = 512;

// h:mm:ss
void clock(char* out, za::SizeT size, double seconds)
{
    const int s = seconds > 0.0 ? static_cast<int>(seconds) : 0;
    q_snprintf(out, size, "%d:%02d:%02d", s / 3600, (s / 60) % 60, s % 60);
}

[[nodiscard]] int activePlayers()
{
    int n = 0;
    for(int i = 0; i < svs.maxclients; i++)
    {
        n += svs.clients[i].active ? 1 : 0;
    }
    return n;
}

void print()
{
    char line[lineLength];
    char mode[64];
    if(deathmatch.value != 0.f)
    {
        q_snprintf(mode, sizeof(mode), "deathmatch %g, fraglimit %g, timelimit %g", static_cast<double>(deathmatch.value),
            static_cast<double>(fraglimit.value), static_cast<double>(timelimit.value));
    }
    else
    {
        q_snprintf(mode, sizeof(mode), "%s, skill %g", coop.value != 0.f ? "coop" : "single player",
            static_cast<double>(skill.value));
    }
    int len = q_snprintf(line, sizeof(line), "status: %s (%s) | %d of %d players", sv.name, mode, activePlayers(),
        svs.maxclients);
    const char* sep = ": ";
    for(int i = 0; i < svs.maxclients && len > 0 && len < lineLength; i++)
    {
        const client_t& c = svs.clients[i];
        if(!c.active)
        {
            continue;
        }
        len += q_snprintf(line + len, sizeof(line) - static_cast<za::SizeT>(len), "%s%s %d ms", sep, c.name,
            VR_ClientPingMs(i));
        sep = ", ";
    }
    char mapTime[32];
    char upTime[32];
    clock(mapTime, sizeof(mapTime), sv.qcvm.time);
    clock(upTime, sizeof(upTime), state.started >= 0.0 ? realtime - state.started : 0.0);
    if(len > 0 && len < lineLength)
    {
        q_snprintf(line + len, sizeof(line) - static_cast<za::SizeT>(len), " | map %s, up %s", mapTime, upTime);
    }
    Con_Printf("%s\n", line);
}

void now_f()
{
    if(!sv.active)
    {
        Con_Printf("vr_sv_status_now: no server running\n");
        return;
    }
    print();
}

} // namespace

} // namespace qvr::svstatus

using namespace qvr;

extern "C" int VR_ClientPingMs(int slot)
{
    if(slot < 0 || slot >= svs.maxclients)
    {
        return 0;
    }
    const client_t& c = svs.clients[slot];
    float total = 0.f;
    for(int j = 0; j < NUM_PING_TIMES; j++)
    {
        total += c.ping_times[j];
    }
    return static_cast<int>(total / NUM_PING_TIMES * 1000.f + 0.5f);
}

extern "C" void VR_ServerStatusInit(void)
{
    Cmd_AddCommand("vr_sv_status_now", svstatus::now_f);
}

extern "C" void VR_ServerStatusFrame(void)
{
    svstatus::State& st = svstatus::state;
    if(cls.state != ca_dedicated || !sv.active)
    {
        st.players = -1;
        return;
    }
    if(st.started < 0.0)
    {
        st.started = realtime;
    }
    const double every = static_cast<double>(vr_sv_status.value);
    const int players = svstatus::activePlayers();
    if(every > 0.0 && (players != st.players || realtime >= st.next))
    {
        svstatus::print();
        st.next = realtime + every;
    }
    st.players = players;
}
