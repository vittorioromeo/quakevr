// vr_flathud.cpp -- flatscreen play's status bar at hudstyle 1 (docs/vr-port/FLATPLAY.md, "The status bar"; his words
// 2026-11-05: "Place the player portrait and health in the center, above the ammo bar. Add current ammo counters to the
// bottom-left and right-edge of the screen, one per hand"). sbar.c draws it with its own pictures and numbers; this file
// says what each hand's counter reads and where bullet time's meter stands (VR_FlatHudHand, VR_FlatHudTime), and
// vr_flathud_report prints both.

#include "vr_api.h"
#include "vr_backend.hpp" // HAND_OFF / HAND_MAIN
#include "vr_bullettime.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_flatplay.hpp"
#include "vr_protocol.hpp"
#include "vr_weapon_ids.h"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "vr_zancle.hpp"

extern "C" cvar_t scr_sbarscale; // gl_screen.c

namespace qvr::flathud
{

namespace
{

// QC's ammo type (AID_*: vr_defs.qc) as the status bar's picture of its kind (-1 none): Rogue's lava nails,
// multi-rockets and plasma as the nails, rockets and cells (vr_gadget.cpp's ammoPicOf).
constexpr int ammoPics[8] = {-1, 0, 1, 2, 3, 1, 2, 3};

[[nodiscard]] int ammoPicOf(int aid)
{
    return aid >= 0 && aid < 8 ? ammoPics[aid] : -1;
}

// A gun whose clip is its only ammo (no reserve type: QC's counter is its clip), so an empty one still reads 0: the
// chainsaw's fuel, the grunt's shotgun, the enforcer's rifle, AD's guns, the flare gun.
[[nodiscard]] bool clipOnly(int wid)
{
    return wid == WID_CHAINSAW || wid == WID_GRUNTGUN || wid == WID_ENFRIFLE || wid == WID_FLAREGUN ||
           (wid >= WID_ADROCKET && wid <= WID_ADCROSSBOW);
}

struct Counter
{
    bool shown{false};
    int count{0};
    int pic{-1};
    int wid{WID_FIST};
};

// What hand `hand`'s counter reads: the server's own counter for that hand (QC weapons.qc updateAmmoCounter: the reserve of
// the gun's ammo, or the clip of a gun whose clip is its only ammo), refreshed on every change of the hand's weapon.
[[nodiscard]] Counter counter(int hand)
{
    using namespace protocol;
    const bool main = hand == HAND_MAIN;
    Counter c;
    c.wid = static_cast<int>(cl.stats[main ? STAT_QVR_WEAPON : STAT_QVR_WEAPON2]);
    if(c.wid == WID_FIST)
    {
        return c;
    }
    const int aid = cl.stats[main ? STAT_QVR_AMMOTYPE : STAT_QVR_AMMO2];
    c.pic = ammoPicOf(aid);
    c.count = cl.stats[main ? STAT_QVR_AMMOCOUNTER : STAT_QVR_AMMOCOUNTER2];
    c.shown = aid != 0 || c.count != 0 || clipOnly(c.wid); // (an axe, a sword, the grapple: nothing to count)
    return c;
}

void report_f()
{
    using namespace protocol;
    for(int hand : {HAND_OFF, HAND_MAIN})
    {
        const bool main = hand == HAND_MAIN;
        const Counter c = counter(hand);
        int clip = 0, size = 0;
        const int clipShown = VR_FlatHudClip(main ? 0 : 1, &clip, &size);
        Con_Printf("flathud: %s hand: weapon %d shown %d count %d pic %d big %d small %d (raw: ammo type %d counter %d clip %d/%d)\n",
            main ? "right" : "left", c.wid, c.shown ? 1 : 0, c.count, c.pic, clipShown ? clip : c.count, clipShown ? c.count : -1,
            cl.stats[main ? STAT_QVR_AMMOTYPE : STAT_QVR_AMMO2], cl.stats[main ? STAT_QVR_AMMOCOUNTER : STAT_QVR_AMMOCOUNTER2],
            cl.stats[main ? STAT_QVR_WEAPONCLIP : STAT_QVR_WEAPONCLIP2],
            cl.stats[main ? STAT_QVR_WEAPONCLIPSIZE : STAT_QVR_WEAPONCLIPSIZE2]);
    }
    float level = 0.f;
    int state = 0;
    const int on = VR_FlatHudTime(&level, &state);
    Con_Printf("flathud: pools shells %d nails %d rockets %d cells %d; STAT_AMMO %d; time bar %d level %.3f state %d; "
               "layout %d (hudstyle %d, vr_flat_hud %g, flatscreen play %d)\n",
        cl.stats[STAT_SHELLS], cl.stats[STAT_NAILS], cl.stats[STAT_ROCKETS], cl.stats[STAT_CELLS], cl.stats[STAT_AMMO], on, level,
        state, VR_FlatHudOn(), hudstyle, vr_flat_hud.value, flatplay::active() ? 1 : 0);
    // The status bar's canvas (gl_draw.c, CANVAS_SBAR2): the screen in pixels and its scale, for a test that finds the
    // layout's pieces in a screenshot.
    const float most = za::min(static_cast<float>(vid.guiwidth) / 400.f, static_cast<float>(vid.guiheight) / 225.f);
    const float s = scr_sbarscale.value < 1.f ? 1.f : scr_sbarscale.value > most ? most : scr_sbarscale.value; // (CLAMP)
    Con_Printf("flathud: canvas %d %d scale %.3f\n", vid.guiwidth, vid.guiheight, s);
}

} // namespace

void init()
{
    Cmd_AddCommand("vr_flathud_report", report_f);
}

} // namespace qvr::flathud

using namespace qvr;

extern "C" int VR_FlatHudOn(void)
{
    return flatplay::active() && hudstyle == HUD_MODERN_CENTERAMMO && vr_flat_hud.value != 0.f ? 1 : 0;
}

extern "C" int VR_FlatHudHand(int left, int* count, int* ammoPic)
{
    const flathud::Counter c = flathud::counter(left ? HAND_OFF : HAND_MAIN);
    *count = c.count;
    *ammoPic = c.pic;
    return c.shown ? 1 : 0;
}

// A flat-screen player's gun with a clip (the server reloads for him: STAT_QVR_RELOADMODE, vr_flat_reload; not a gun whose
// clip is its only ammo, whose counter is its clip already): its rounds and size. The status bars draw the clip in the big
// numbers and the reserve (the hand's counter) in small type beside it (his words 2026-11-08: "the current ammunition in
// the clip in big font, and the total ammunition in storage in small font"). Every hudstyle; flat-screen play only.
extern "C" int VR_FlatHudClip(int left, int* clip, int* clipSize)
{
    using namespace protocol;
    const bool main = left == 0;
    const int wid = static_cast<int>(cl.stats[main ? STAT_QVR_WEAPON : STAT_QVR_WEAPON2]);
    const int size = cl.stats[main ? STAT_QVR_WEAPONCLIPSIZE : STAT_QVR_WEAPONCLIPSIZE2];
    if(!flatplay::active() || cl.stats[STAT_QVR_RELOADMODE] == 0 || wid == WID_FIST || size <= 0 || flathud::clipOnly(wid))
    {
        return 0;
    }
    *clip = cl.stats[main ? STAT_QVR_WEAPONCLIP : STAT_QVR_WEAPONCLIP2];
    *clipSize = size;
    return 1;
}

extern "C" int VR_FlatHudTime(float* level, int* state)
{
    const bullettime::Meter m = bullettime::meter();
    *level = m.level;
    *state = m.active ? 2 : m.cooling ? 3 : m.ready ? 1 : 0;
    return m.enabled ? 1 : 0;
}
