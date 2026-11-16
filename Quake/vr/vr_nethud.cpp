// vr_nethud.cpp -- the net HUD (MP_PLAN.md, B13): while joined to a remote server, `vr_net_hud` 1 shows at the top
// right (under the clock and the frame rate) the ping, the loss and the prediction's corrections; 2 a second line with
// the bytes each way and the reliable resends (Debug > Reports > Network: Net HUD).
//
// - Ping: the round trip of his moves' acknowledgements (vr_netmove.cpp: the way there, the server's frame that ran the
//   move, the datagram back), smoothed; to a server without Quake VR's moves, the reliable chunks' (net_dgrm.c).
// - Loss: the unreliable datagrams whose sequence numbers were skipped (lost, or late past a newer one) of all expected
//   in the last window (a second of game time; qsocket_t's counts).
// - Fix: his own walk's predicted place corrected by the server's by half a unit or more (vr_predict.cpp), a second;
//   their mean length in units (vr_net_hud_report).
// `vr_net_hud_report` prints the values shown ("mp_nethud:"), whether or not it is drawn.

#include "vr_nethud.hpp"

#include "vr_api.h"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_netmove.hpp"
#include "vr_predict.hpp"

#include "Zancle/Base/IntTypes.hpp"

namespace qvr::nethud
{

namespace
{

constexpr double windowSeconds = 1.0;
constexpr int lineLength = 48;

struct Sample
{
    double time{0.0};
    unsigned int counts[5]{}; // NET_QvrLinkCounts's: read, skipped, resent, bytes in, bytes out
    za::U64 corrN{0};
    double corrSum{0.0};
};

struct State
{
    struct qsocket_s* sock{nullptr}; // the connection the samples are of
    Sample last;                     // the window's start
    bool haveWindow{false};          // the values below are of a whole window
    int pingMs{0}, pingLastMs{0};
    float lossPct{0.f}, fixRate{0.f}, fixMean{0.f}, inKBs{0.f}, outKBs{0.f}, resentRate{0.f};
    bool predicts{false};
    int reportAfter{0}; // vr_net_hud_report <n>: the windows left before it prints
    char lines[2][lineLength]{};
};
State state;

void printReport();

[[nodiscard]] bool remote()
{
    return cls.state == ca_connected && cls.netcon && !sv.active && !cls.demoplayback;
}

void take(Sample& s)
{
    double reliableRtt = 0.0;
    NET_QvrLinkCounts(cls.netcon, s.counts, &reliableRtt);
    predict::correctionTotals(s.corrN, s.corrSum);
    s.time = realtime;
}

void update()
{
    if(!remote())
    {
        state.sock = nullptr;
        state.haveWindow = false;
        return;
    }
    if(state.sock != cls.netcon || realtime < state.last.time)
    {
        state.sock = cls.netcon;
        state.haveWindow = false;
        take(state.last);
    }

    // (the ping at every frame: the newest smoothed)
    double newest = 0.0;
    double rtt = netmove::clientRoundTrip(&newest);
    state.predicts = rtt > 0.0;
    if(rtt <= 0.0)
    {
        unsigned int counts[5];
        NET_QvrLinkCounts(cls.netcon, counts, &rtt);
        newest = rtt;
    }
    state.pingMs = static_cast<int>(rtt * 1000.0 + 0.5);
    state.pingLastMs = static_cast<int>(newest * 1000.0 + 0.5);

    if(realtime - state.last.time >= windowSeconds)
    {
        Sample now;
        take(now);
        const double dt = now.time - state.last.time;
        const unsigned int read = now.counts[0] - state.last.counts[0];
        const unsigned int skipped = now.counts[1] - state.last.counts[1];
        const za::U64 fixes = now.corrN - state.last.corrN;
        state.lossPct = read + skipped > 0 ? 100.f * static_cast<float>(skipped) / static_cast<float>(read + skipped) : 0.f;
        state.fixRate = static_cast<float>(static_cast<double>(fixes) / dt);
        state.fixMean = fixes > 0 ? static_cast<float>((now.corrSum - state.last.corrSum) / static_cast<double>(fixes)) : 0.f;
        state.inKBs = static_cast<float>((now.counts[3] - state.last.counts[3]) / dt / 1024.0);
        state.outKBs = static_cast<float>((now.counts[4] - state.last.counts[4]) / dt / 1024.0);
        state.resentRate = static_cast<float>((now.counts[2] - state.last.counts[2]) / dt);
        state.last = now;
        state.haveWindow = true;
    }

    q_snprintf(state.lines[0], lineLength, "ping %3d  loss %4.1f%%  fix %4.1f/s", state.pingMs, state.lossPct,
        state.fixRate);
    q_snprintf(state.lines[1], lineLength, "in %5.1f out %4.1f KB/s  re %3.1f/s", state.inKBs, state.outKBs,
        state.resentRate);
    if(state.reportAfter > 0 && state.last.time == realtime && --state.reportAfter == 0)
    {
        printReport(); // (the window just ended)
    }
}

// vr_net_hud_report [<n>]: the values now; with n, after n whole windows from now (the harness's: its wait is on the
// line). (A window ends in the 2D pass's update, every frame the screen is drawn.)
void report_f()
{
    if(Cmd_Argc() > 1 && Q_atoi(Cmd_Argv(1)) > 0 && remote())
    {
        state.sock = cls.netcon;
        take(state.last);
        state.haveWindow = false;
        state.reportAfter = Q_atoi(Cmd_Argv(1));
        return;
    }
    update();
    printReport();
}

void printReport()
{
    Con_Printf("mp_nethud: on=%d remote=%d window=%d ping=%d ping_last=%d predicts=%d loss=%.2f fix_rate=%.2f "
               "fix_mean=%.3f in=%.2f out=%.2f resent=%.2f line0=\"%s\" line1=\"%s\"\n",
        static_cast<int>(vr_net_hud.value), remote() ? 1 : 0, state.haveWindow ? 1 : 0, state.pingMs, state.pingLastMs,
        state.predicts ? 1 : 0, state.lossPct, state.fixRate, state.fixMean, state.inKBs, state.outKBs, state.resentRate,
        state.lines[0], state.lines[1]);
}

} // namespace

void init()
{
    Cmd_AddCommand("vr_net_hud_report", report_f);
}

} // namespace qvr::nethud

using namespace qvr;

extern "C" const char* VR_NetHudLine(int line)
{
    if(line == 0)
    {
        nethud::update();
    }
    const int lines = vr_net_hud.value >= 2.f ? 2 : vr_net_hud.value > 0.f ? 1 : 0;
    if(line < 0 || line >= lines || !nethud::state.sock)
    {
        return nullptr;
    }
    return nethud::state.lines[line];
}
