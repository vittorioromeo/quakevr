// vr_comfortfade.cpp -- see vr_comfortfade.hpp.
//
// The bonus colour shift (CSHIFT_BONUS) set to black each frame of the fade, its share the time left over the whole
// (fully black at first, gone at the end), in real time; the engine's own decay of it in between is overridden the next frame.

#include "vr_comfortfade.hpp"
#include "vr_engine.hpp"
#include "vr_cvars.hpp"

#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/MinMax.hpp"

namespace qvr::comfortfade
{
namespace
{

double fadeStart{0.0}; // realtime it began (the eyes: wall-clock time) (black),
double fadeEnd{0.0};   // ... and when it is gone

void fade_f()
{
    const float seconds = Cmd_Argc() > 1 ? static_cast<float>(Q_atof(Cmd_Argv(1))) : vr_comfort_teleport_fade.value;
    start(seconds);
}

void info_f()
{
    Con_Printf("comfortfade: real time %.2f fade %.2f..%.2f bonus %.0f%% (%d %d %d) blend alpha %.2f\n", realtime, fadeStart,
        fadeEnd, cl.cshifts[CSHIFT_BONUS].percent, cl.cshifts[CSHIFT_BONUS].destcolor[0],
        cl.cshifts[CSHIFT_BONUS].destcolor[1], cl.cshifts[CSHIFT_BONUS].destcolor[2], v_blend[3]);
}

} // namespace

void start(const float seconds)
{
    if(seconds <= 0.f)
    {
        return;
    }
    fadeStart = realtime;
    fadeEnd = realtime + za::min(seconds, 5.f);
    Con_DPrintf("comfortfade: from black over %.2f s\n", fadeEnd - fadeStart);
}

void frame()
{
    if(fadeEnd <= 0.0)
    {
        return;
    }
    cshift_t& shift = cl.cshifts[CSHIFT_BONUS];
    if(realtime >= fadeEnd || realtime < fadeStart) // (over; or the clock went back)
    {
        shift.percent = 0.f; // (the black's last share gone at once, not left to the bonus flash's slow decay)
        clear();
        return;
    }
    const float k = static_cast<float>(za::clamp((fadeEnd - realtime) / za::max(0.01, fadeEnd - fadeStart), 0.0, 1.0));
    shift.destcolor[0] = 0;
    shift.destcolor[1] = 0;
    shift.destcolor[2] = 0;
    shift.percent = 255.f * k; // (its own: a bonus flash meanwhile is lost in the black)
}

void clear()
{
    fadeStart = 0.0;
    fadeEnd = 0.0;
}

void registerCommands()
{
    Cmd_AddCommand("vr_comfort_fade", fade_f);
    Cmd_AddCommand("vr_comfort_fade_info", info_f);
}

} // namespace qvr::comfortfade
