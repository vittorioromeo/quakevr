// vr_lightstyle.cpp -- lightstyles in VR: the flicker comfort (vr_lightstyle_comfort: an animated style's swing around
// its average softened, its steps blended) and vr_lightstyle_rate, a probe of how fast each style steps and changes.
// The styles run on the client's clock at 10 steps a second (R_AnimateLight: cl.time * 10), whatever the frame rate.

#include "vr_lightstyle.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_main.hpp"

#include <math.h>

namespace qvr::lightstyle
{

namespace
{

// vr_lightstyle_rate's probe: per style, from its first frame.
struct Track
{
    float last{-1.f};
    float lo{1e9f}, hi{-1e9f};
    float maxJump{0.f};   // the largest change from one frame to the next ('a'..'z' levels)
    double travel{0.0};   // the sum of the changes' sizes
    int changes{0};       // frames its level changed
};

struct Probe
{
    bool on{false};
    double seconds{0.0};
    double t0{0.0}, tLast{0.0};
    double r0{0.0};       // realtime at the start
    int frames{0};
    int steps{0};         // the 10 Hz step index advanced
    int lastStep{0};
    int only{-1};         // one style (-1: every animated one)
    Track tracks[MAX_LIGHTSTYLES];
};
Probe probe;

[[nodiscard]] bool animated(int style)
{
    return style >= 0 && style < MAX_LIGHTSTYLES && cl_lightstyle[style].length > 1;
}

void report()
{
    const double span = probe.tLast - probe.t0;
    const double wall = realtime - probe.r0;
    Con_Printf("LSRATE|%d frames in %.2f s of client time (%.1f frames a second of it)|%.1f steps/s|%.2f s wall\n",
        probe.frames, span, span > 0.0 ? probe.frames / span : 0.0, span > 0.0 ? probe.steps / span : 0.0, wall);
    for(int j = 0; j < MAX_LIGHTSTYLES; j++)
    {
        const Track& t = probe.tracks[j];
        if(!animated(j) || t.last < 0.f || (probe.only >= 0 && j != probe.only))
        {
            continue;
        }
        Con_Printf("LSRATE|style %d|\"%.24s\"|swing %.2f..%.2f|%.1f changes/s|travel %.1f/s|max jump %.2f a frame\n", j,
            cl_lightstyle[j].map, t.lo, t.hi, span > 0.0 ? t.changes / span : 0.0, span > 0.0 ? t.travel / span : 0.0,
            t.maxJump);
    }
    Con_Printf("LSRATE|done\n");
}

void rate_f()
{
    probe = Probe{};
    probe.seconds = Cmd_Argc() >= 2 ? Q_atof(Cmd_Argv(1)) : 5.0;
    probe.only = Cmd_Argc() >= 3 ? Q_atoi(Cmd_Argv(2)) : -1;
    probe.on = probe.seconds > 0.0;
    probe.t0 = probe.tLast = cl.time;
    probe.r0 = realtime;
    probe.lastStep = static_cast<int>(floor(cl.time * 10.0));
    Con_Printf("vr_lightstyle_rate: %.1f s of client time, vr_lightstyle_comfort %g (%s)\n", probe.seconds,
        vr_lightstyle_comfort.value, vrActive() ? "in VR: applied" : "not in VR: as authored");
}

} // namespace

void registerCommands()
{
    Cmd_AddCommand("vr_lightstyle_rate", rate_f);
}

} // namespace qvr::lightstyle

// The style's level this frame: in VR with vr_lightstyle_comfort, the step blended into the next one (always: no
// abrupt jump) and its swing around the style's own average scaled down. `level` is the engine's own (as authored).
extern "C" float VR_LightstyleLevel(int style, int step, int next, float frac, float level)
{
    using namespace qvr;
    const float amount = vr_lightstyle_comfort.value;
    if(amount <= 0.f || !lightstyle::animated(style) || !vrActive())
    {
        return level;
    }
    const lightstyle_t& ls = cl_lightstyle[style];
    float total = 0.f;
    for(int i = 0; i < ls.length; i++)
    {
        total += static_cast<float>(ls.map[i] - 'a');
    }
    const float average = total / static_cast<float>(ls.length);
    const float blended = static_cast<float>(step) + static_cast<float>(next - step) * frac;
    const float keep = 1.f - (amount < 1.f ? amount : 1.f);
    return average + (blended - average) * keep;
}

extern "C" void VR_LightstyleFrame(void)
{
    using namespace qvr::lightstyle;
    if(!probe.on)
    {
        return;
    }
    probe.frames++;
    probe.tLast = cl.time;
    const int step = static_cast<int>(floor(cl.time * 10.0));
    probe.steps += step != probe.lastStep ? 1 : 0;
    probe.lastStep = step;
    for(int j = 0; j < MAX_LIGHTSTYLES; j++)
    {
        if(!animated(j))
        {
            continue;
        }
        Track& t = probe.tracks[j];
        const float v = r_lightbuffer.lightstyles[j] * (256.f / 22.f);
        if(t.last >= 0.f)
        {
            const float d = fabsf(v - t.last);
            t.changes += d > 1e-4f ? 1 : 0;
            t.travel += d;
            t.maxJump = d > t.maxJump ? d : t.maxJump;
        }
        t.last = v;
        t.lo = v < t.lo ? v : t.lo;
        t.hi = v > t.hi ? v : t.hi;
    }
    if(cl.time - probe.t0 >= probe.seconds)
    {
        probe.on = false;
        report();
    }
}
