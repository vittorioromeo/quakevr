// vr_flatsight.cpp -- the flatscreen player's ironsight, and his free hand on a grip (Vittorio's flatscreen list,
// items 3 and 4). Read vr_flatsight.hpp for what the two parts do; here is how they are measured.
//
// Item 3 (the grip). The view already offers the held weapon's two-handed grip point every frame it draws a weapon:
// hands::State::grip2H[holding], the hotspot it chose for the helping hand, placed with the muzzles (vr_view.cpp's
// placeHotspots, vr_view_weapon.cpp's chooseGrip). A placed point is only good for the frame it was placed in, so it is
// carried to this frame's pose the way a muzzle is -- from hands::State::placedFrom, where the hand was when the view
// placed it (the same carry hands::handMuzzle uses for the shot). The empty hand is eased onto it, starting from where
// flatplay put it (its support pose), and vr_twohand.cpp's apply runs over the pose: standing on the hotspot it takes
// hold by its own tolerances (foregripTake 5.5 units to take, foregripKeep 20 times the hotspot's stickiness to keep),
// its helpKind is 1, the game code gets the two-handed aiming bits, and the weapon's weight, recoil and spread become
// the two-handed ones. E's reach is added on top of the pin, staying inside the keep. Only a weapon whose model has
// such a hotspot is held this way: without one the hand keeps flatplay's pose, and a weapon whose Two-Handed setting is
// "Not Allowed" is refused by vr_twohand itself. Dual wielding never enters here: both hands hold, so there is no empty
// hand to put anywhere, and their pose is today's.
//
// A flatscreen hand is always gripping (vr_client.cpp's gripping), so there is no press for a hotspot to be taken on:
// the frame this pose brings the empty hand onto the grip is stood in for that press by twohand::armFlatGrip, armed
// once per arrival and let go the frame it leaves. Everything else -- goodDistance, goodDot, the stickiness, the
// weapon's own setting -- is vr_twohand's, unchanged.
//
// What vr_twohand turns of a hand's angles is put back afterwards -- in a headset the gun's own turn *is* the aim, but
// on the flat screen the crosshair is, and the shots must go along it (vr_flatplay.cpp's own rule). What is kept is the
// grip: the help, its transition, and everything the game code reads off them.
//
// Item 4 (the ironsight). Only for a gun with a sight line (sightalign::alignable: a ranged weapon; the melee weapons
// have none) held in one hand alone. The weapon's model, its muzzle and its shot all follow the holding hand, and the
// shot's direction is already the crosshair's (aimRotAtCrosshair solves the hand's turn out of the weapon's Shot Pitch
// and Yaw), so what is left is where the shot comes from: the muzzle stands beside the crosshair's ray -- the sights
// are off the middle of the screen, and a shot passes beside what the crosshair is on. The holding hand moves by that
// offset across the ray (nothing along it), eased in over a tenth of a second and measured afresh from the crosshair
// every frame, so a turn keeps the sights on it. It is the game's pose that moves, not flatplay::drawnOffset: the view
// puts the muzzle back on the steady hand (vr_view.cpp), so a looks-only move would move the drawn gun and leave the
// shot where it was. The field of view narrows with it (vr_flat_aim_fov, through VR_FlatAdsFov in gl_screen.c); the
// engine's own IN_FovScale keeps the crosshair's speed on the screen, and the HUD does not scale with the FOV.
//
// None of it runs outside flatscreen play: every entry point says so, and a headset run is untouched.

#include "vr_flatsight.hpp"

#include "vr_api.h"
#include "vr_backend.hpp" // HAND_OFF / HAND_MAIN / HAND_COUNT
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_flatplay.hpp"
#include "vr_geom.hpp"
#include "vr_hands.hpp"
#include "vr_sightalign.hpp"
#include "vr_twohand.hpp"
#include "vr_weapons.hpp"

#include "Zancle/Math/Acos.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/Exp.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "vr_zancle.hpp"

extern "C" qboolean keydown[MAX_KEYS]; // keys.c (the game's own key state, as flatplay reads E and V from it)

namespace qvr::flatsight
{

namespace
{

// The state for the session (no function-local statics: CODE_STYLE.md).
struct Sight
{
    bool ads{false};            // the ironsight is asked for (the mouse wheel's button, vr_flat_aim)
    bool keyWas{false};         // K_MOUSE3 last frame: a press edge
    double since{-1e9};         // when it was last toggled
    int toggles{0};

    double lastTime{-1e9};      // the eases below step once a frame (the pose is made several times a frame)
    bool gripWanted{false};     // what apply found last frame: a grip to hold, a weapon to centre
    bool adsWanted{false};
    float gripT{0.f};           // how far the empty hand has gone onto the grip (0: flatplay's own support pose)
    float adsT{0.f};            // how far the held weapon has come onto the crosshair

    int holding{-1};            // this frame: the hand that holds a weapon while the other is empty (-1: neither)
    int helping{-1};            // ... and the empty hand that holds on to it
    bool pinned{false};         // it was put on the weapon's hotspot (the weapon has one)
    int edgeFrame{-1};          // the frame the arrival at a grip was decided in (the pose is made twice a frame)
    bool armed{false};          // ... and the press it stands for, kept for both poses of that frame
    bool atGrip{false};         // last frame: the empty hand stood on a hotspot (its own take distance) -- the edge
    int lastHolding{-1};        // last frame's pair: a weapon changing disarms the hand that was reaching
    int lastHelping{-1};
    glm::vec3 shift{0.f};       // item 4: how far the holding hand moved onto the crosshair's ray (world, units)
    float offBefore{0.f};       // its muzzle off that ray before the move, and after it (units)
    float offAfter{0.f};
    float sightAngle{0.f};      // the same offset as an angle, at the muzzle's own distance (degrees)

    float fovNarrow{0.f};       // the field of view the ironsight is narrowing by this frame (gl_screen.c reads it)
};

Sight st;

constexpr float gripEase = 9.f;  // how fast an empty hand travels onto a grip, and back off it (1/s)
constexpr float adsEase = 12.f;  // how fast a held weapon comes onto the crosshair, and back off it (1/s)
constexpr float camNear = 24.f;  // flatplay::camNear: the camera the flat hands are kept on (drawnOffset)

[[nodiscard]] float ease(float k, float dt)
{
    return dt > 0.f ? 1.f - za::exp(-k * dt) : 1.f;
}

// The weapon `h` holds, as its settings' slot; -1 for an empty hand (flatplay's heldIn, the same rule).
[[nodiscard]] int heldIn(int h)
{
    const int slot = weapons::heldSlot(h);
    return slot >= 0 && slot != weapons::fistSlot() ? slot : -1;
}

// The muzzle the hand's shots go from this frame: the one the view placed last frame, carried with the hand since
// (hands::handMuzzle does exactly this for the shot itself, vr_client.cpp).
[[nodiscard]] glm::vec3 muzzleNow(const hands::State& s, int h)
{
    if(!s.muzzleValid[h])
    {
        return s.pos[h] + hands::forward(s.rot[h]) * 8.f;
    }
    return s.muzzle[h] + (s.pos[h] - s.placedFrom[h]);
}

// The crosshair: the centre of the screen as the player sees it (the camera flatplay keeps the drawn hands on). A
// camera that is not on the player's own head (a chase cam) is stood in for by the pose's head along the aim.
void crosshairRay(const hands::State& s, const glm::vec3& aim, glm::vec3& org, glm::vec3& dir)
{
    const glm::vec3 cam{r_refdef.vieworg[0], r_refdef.vieworg[1], r_refdef.vieworg[2]};
    if(!chase_active.value && glm::length(cam - s.head) < camNear)
    {
        org = cam;
        dir = hands::forward(glm::vec3{r_refdef.viewangles[0], r_refdef.viewangles[1], r_refdef.viewangles[2]});
        return;
    }
    org = s.head;
    dir = hands::forward(aim);
}

// How far a point stands off a ray: across it, and as an angle at the point's own distance along it.
void offTheRay(const glm::vec3& org, const glm::vec3& dir, const glm::vec3& p, float& units, float& degrees)
{
    const glm::vec3 d = p - org;
    const float len = glm::length(d);
    units = len > 1e-6f ? glm::length(d - dir * glm::dot(d, dir)) : 0.f;
    degrees = len > 1.f ? glm::degrees(za::acos(za::clamp(glm::dot(d, dir) / len, -1.f, 1.f))) : 0.f;
}

// Item 3's case: one hand holds a weapon and the other is empty.
bool oneHanded(int& holding, int& helping)
{
    const bool mainHolds = heldIn(HAND_MAIN) >= 0;
    const bool offHolds = heldIn(HAND_OFF) >= 0;
    if(mainHolds && !offHolds)
    {
        holding = HAND_MAIN;
        helping = HAND_OFF;
        return true;
    }
    if(offHolds && !mainHolds)
    {
        holding = HAND_OFF;
        helping = HAND_MAIN;
        return true;
    }
    holding = helping = -1;
    return false;
}

// Item 3's press. A flatscreen hand is always gripping (vr_client.cpp's gripping), so the pose putting the empty hand
// onto a grip has no press to be taken on; this is the frame that stands for it, measured the way vr_twohand measures
// the take: the hotspot's own point, less its bias, within its take distance, and the hands' line within
// vr_2h_angle_threshold of the held weapon's own. A cup (grip2HCup) and a palm grip (grip2HPalm) are held by twohand's
// own rules rather than by an arrival here, so those keep flatplay's support pose as it is today.
bool arrivedAtGrip(const hands::State& s)
{
    if(st.helping < 0 || !s.grip2HValid[st.holding] || s.grip2HCup[st.holding] || s.grip2HPalm[st.holding])
    {
        return false;
    }
    if(glm::distance(s.pos[st.helping], s.grip2H[st.holding]) - s.grip2HBias[st.holding] >= twohand::gripTakeUnits)
    {
        return false;
    }
    glm::vec3 fwd, right, up;
    hands::angleVectors(s.rot[st.holding], fwd, right, up);
    const glm::vec3 line = s.pos[st.helping] - s.pos[st.holding];
    return glm::length(line) > 1e-4f &&
           glm::dot(glm::normalize(line), fwd) > za::cos(glm::radians(vr_2h_angle_threshold.value));
}

// The ironsight asked for, and holding what it applies to: a ranged weapon, in one hand alone. In flatscreen play
// only -- a headset run never has it, whatever the flatscreen player left behind when a headset took the game back.
bool adsApplies()
{
    return flatplay::active() && st.ads && st.holding >= 0 && sightalign::alignable(st.holding);
}

void setAds(bool on, const char* by)
{
    if(on == st.ads)
    {
        return;
    }
    st.ads = on;
    st.since = vr_gametime;
    st.toggles++;
    if(vr_flatplay_debug.value)
    {
        Con_Printf("flatsight: ironsight %s (%s)%s\n", on ? "on" : "off", by,
            st.ads && st.holding >= 0 && !sightalign::alignable(st.holding)
                ? " -- the weapon held has no sights to align"
                : "");
    }
}

// The mouse wheel's button, K_MOUSE3 (keys.h): a press edge read from the keyboard state, like flatplay's E and V, so
// nothing is rebound and no config changes. The wheel itself (K_MWHEELUP, K_MWHEELDOWN) still cycles the weapons: this
// is its button, pressed.
void pollKey()
{
    const bool down = key_dest == key_game && keydown[K_MOUSE3] != 0;
    if(down && !st.keyWas)
    {
        setAds(!st.ads, "mouse wheel button");
    }
    st.keyWas = down;
}

// vr_flat_aim [0|1|toggle]: the same from the console (and from a -Script), flatscreen only.
void aim_f()
{
    if(!flatplay::active())
    {
        Con_Printf("Flatscreen aim: flatscreen play only (start the game with -flatscreenmode).\n");
        return;
    }
    const char* arg = Cmd_Argc() > 1 ? Cmd_Argv(1) : "toggle";
    if(q_strcasecmp(arg, "toggle") == 0)
    {
        setAds(!st.ads, "vr_flat_aim");
    }
    else if(q_strcasecmp(arg, "1") == 0 || q_strcasecmp(arg, "on") == 0)
    {
        setAds(true, "vr_flat_aim");
    }
    else if(q_strcasecmp(arg, "0") == 0 || q_strcasecmp(arg, "off") == 0)
    {
        setAds(false, "vr_flat_aim");
    }
    else
    {
        Con_Warning("Flatscreen aim: use vr_flat_aim 0, 1 or toggle.\n");
    }
}

// Item 3: the empty hand onto the held weapon's two-handed grip hotspot, eased from where flatplay put it.
void pinToGrip(hands::State& s, const glm::vec3& reach)
{
    st.pinned = false;
    if(st.holding < 0 || st.gripT <= 0.f)
    {
        return;
    }
    // The hotspot as the view placed it last frame, carried to this frame's pose with the holding hand.
    const glm::vec3 grip = s.grip2H[st.holding] + (s.pos[st.holding] - s.placedFrom[st.holding]);
    // A cup (a two-handed pistol grip) is taken by the palm: move the hand so that its palm lands there.
    const glm::vec3 to = s.grip2HPalm[st.holding]
                             ? grip + (s.pos[st.helping] - hands::palmPoint(s, st.helping))
                             : grip;
    const glm::vec3 out = st.helping == HAND_OFF ? reach * st.gripT : glm::vec3{0.f}; // E still reaches out with it
    s.pos[st.helping] += (to - s.pos[st.helping]) * st.gripT + out;
    // vr_twohand measures the take and the keep against the point this pose reaches; the view places it again with the
    // weapon it draws (with this frame's drawn hand), which is the same point moved the same way.
    s.grip2H[st.holding] = grip;
    st.pinned = true;
}

// Item 4: the held weapon brought onto the crosshair's ray, by the muzzle its shots go from.
void centreOnCrosshair(hands::State& s, const glm::vec3& aim)
{
    st.shift = glm::vec3{0.f};
    st.offBefore = st.offAfter = st.sightAngle = 0.f;
    if(st.adsT <= 0.f || st.holding < 0)
    {
        return;
    }
    glm::vec3 org, dir;
    crosshairRay(s, aim, org, dir);
    const glm::vec3 m = muzzleNow(s, st.holding);
    offTheRay(org, dir, m, st.offBefore, st.sightAngle);

    const glm::vec3 lat = m - org - dir * glm::dot(m - org, dir); // off the ray: across it, nothing along it
    st.shift = -lat * st.adsT;
    if(glm::dot(st.shift, st.shift) < 1e-8f)
    {
        st.offAfter = 0.f;
        return;
    }
    s.pos[st.holding] += st.shift;
    if(st.pinned)
    {
        // The grip the other hand is holding travels with the hand it holds on to: their distance apart is unchanged.
        s.grip2H[st.holding] += st.shift;
        s.pos[st.helping] += st.shift;
    }
    st.offAfter = glm::length(lat + st.shift);
}

} // namespace

void init()
{
    Cmd_AddCommand("vr_flat_aim", aim_f);
}

void frame()
{
    if(!flatplay::active())
    {
        st.ads = st.keyWas = false;
        st.gripWanted = st.adsWanted = false;
        st.gripT = st.adsT = 0.f;
        st.lastTime = -1e9;
        if(st.fovNarrow != 0.f)
        {
            st.fovNarrow = 0.f; // a headset took the game back with the ironsight on: the view is the player's own again
            vid.recalc_refdef = 1;
        }
        return;
    }

    // One frame's step (this runs once a frame, before the hands are posed; the eases are the state's, the pose reads
    // them).
    const double t = vr_gametime;
    const float dt = st.lastTime > -1e8 ? za::clamp(static_cast<float>(t - st.lastTime), 0.f, 0.1f) : 0.f;
    st.lastTime = t;
    st.gripT = za::clamp(st.gripT + (st.gripWanted ? 1.f : -1.f) * ease(gripEase, dt), 0.f, 1.f);
    st.adsT = za::clamp(st.adsT + (st.adsWanted ? 1.f : -1.f) * ease(adsEase, dt), 0.f, 1.f);

    pollKey();

    // The field of view is chosen when the engine recalculates its view (gl_screen.c's SCR_CalcRefdef through
    // VR_FlatAdsFov), and that happens on demand: ask for it the frame the ironsight's share of it changes -- its
    // toggle, a weapon with no sights taken up while it is on, or a vr_flat_aim_fov moved from the console or the menu.
    const float narrow = adsApplies() ? za::clamp(vr_flat_aim_fov.value, 0.f, 60.f) : 0.f;
    if(narrow != st.fovNarrow)
    {
        st.fovNarrow = narrow;
        vid.recalc_refdef = 1;
    }
}

void apply(hands::State& s, const glm::vec3& aim, const glm::vec3& reach)
{
    if(!flatplay::active() || !s.valid)
    {
        st.holding = st.helping = -1;
        st.pinned = false;
        st.gripWanted = st.adsWanted = false;
        if(st.lastHelping >= 0)
        {
            twohand::armFlatGrip(st.lastHelping, false); // his hands are reaching nothing any more
        }
        st.atGrip = false;
        st.armed = false;
        st.edgeFrame = -1;
        st.lastHolding = st.lastHelping = -1;
        return;
    }

    st.holding = st.helping = -1;
    oneHanded(st.holding, st.helping);
    st.gripWanted = st.holding >= 0 && s.grip2HValid[st.holding];
    st.adsWanted = adsApplies();
    // The pose is made twice a frame in flatscreen play (vr_client.cpp's buildMove: the hands made again with this
    // frame's mouse turn), so the arrival at a grip, and the press it stands for, is decided once a frame and the same
    // for both poses.
    const bool freshFrame = st.edgeFrame != host_framecount;
    if(freshFrame)
    {
        st.edgeFrame = host_framecount;
        if(st.holding != st.lastHolding || st.helping != st.lastHelping)
        {
            // Another weapon, or a hand that stopped helping: the one that was reaching is off the grip, and the next
            // arrival arms afresh -- the way letting go of the grip button and pressing it again does.
            if(st.lastHelping >= 0)
            {
                twohand::armFlatGrip(st.lastHelping, false);
            }
            st.atGrip = false;
            st.armed = false;
        }
    }

    // flatplay made these turns of the hands for the crosshair (aimRotAtCrosshair): kept against what the grip below
    // turns of them.
    const glm::vec3 crosshair[2]{s.rot[HAND_OFF], s.rot[HAND_MAIN]};

    pinToGrip(s, reach);
    centreOnCrosshair(s, aim);

    // Item 3's press, and only its press: true on the frame the empty hand arrives on the hotspot, false the frame it
    // leaves it or the weapon changes. applyHotspots' goodDistance, goodDot, the stickiness and the weapon's own
    // Two-Handed setting decide the hold from here on, exactly as they do for a controller's grip button.
    const bool inside = arrivedAtGrip(s);
    if(freshFrame)
    {
        st.armed = st.helping >= 0 && inside && !st.atGrip;
        st.atGrip = inside;
        st.lastHolding = st.holding;
        st.lastHelping = st.helping;
    }
    if(st.helping >= 0)
    {
        twohand::armFlatGrip(st.helping, st.armed);
    }

    // The two-handed machinery over the pose -- the same vr_twohand.cpp apply a headset run runs, so the hold is real
    // for the game code (its helping flag, its transition, the aiming bits, the weight and recoil that follow them),
    // not a look.
    twohand::apply(s);

    // On the flat screen the crosshair is the aim, whatever the grip made of the held hand's turn (in a headset that
    // turn is the aim, and is kept): put the pose's angles back, and the shots keep going along the crosshair.
    for(int h = 0; h < HAND_COUNT; h++)
    {
        s.rot[h] = crosshair[h];
    }
}

bool aiming()
{
    return adsApplies();
}

float fov(float basefov)
{
    if(!adsApplies())
    {
        return basefov;
    }
    return za::max(20.f, basefov - za::clamp(vr_flat_aim_fov.value, 0.f, 60.f));
}

void statusLine()
{
    Con_Printf("  ironsight: %s (vr_flat_aim, or the mouse wheel's button; vr_flat_aim_fov %g off the field of view; "
               "%d toggles this session)%s\n",
        st.ads ? "asked for" : "off", vr_flat_aim_fov.value, st.toggles,
        st.ads && st.holding >= 0 && !sightalign::alignable(st.holding) ? ", no sights on the weapon held" : "");
    if(st.holding >= 0)
    {
        Con_Printf("  two hands: %s hand holds, the %s hand %s (%s holding: helpKind %d, transition %.2f)\n",
            st.holding == HAND_MAIN ? "main" : "off", st.helping == HAND_MAIN ? "main" : "off",
            st.pinned ? "on its grip hotspot" : "free (no grip hotspot on that weapon)",
            twohand::helping(st.helping) ? "held" : "not held", twohand::helpKind(st.helping),
            twohand::transition(st.holding));
    }
    else
    {
        Con_Printf("  two hands: nothing held in one hand alone (both full, or both free)\n");
    }
}

void print(const hands::State& s)
{
    // Measured afresh here, on the pose the move is about to send: where the muzzle the shots go from stands off the
    // crosshair's ray, and what that is as an angle. st.offBefore is the same measurement before the ironsight moved
    // the hand to it; flatplay::printMove's own line above says which way the shot is going.
    const int h = st.holding >= 0 ? st.holding : HAND_MAIN;
    glm::vec3 org, dir;
    crosshairRay(s, geom::vec(cl.viewangles), org, dir);
    float units = 0.f;
    float degrees = 0.f;
    offTheRay(org, dir, muzzleNow(s, h), units, degrees);

    if(st.holding < 0)
    {
        Con_Printf("flatsight: t %.4f ads %s fov %.2f x %.2f hand main muzzle off the crosshair %.2f units (%.2f deg): "
                   "nothing held one-handed\n",
            vr_gametime, st.ads ? "asked for" : "off", r_refdef.fov_x, r_refdef.fov_y, units, degrees);
        return;
    }
    Con_Printf("flatsight: t %.4f ads %s t %.2f fov %.2f x %.2f hand %s grip %s held %d helpKind %d transition %.2f "
               "muzzle off the crosshair %.2f -> %.2f units (%.2f deg) hand moved %.2f fov take %g\n",
        vr_gametime, adsApplies() ? "on" : (st.ads ? "no sights" : "off"), st.adsT, r_refdef.fov_x, r_refdef.fov_y,
        st.holding == HAND_MAIN ? "main" : "off", st.pinned ? "hotspot" : "none", twohand::helping(st.helping) ? 1 : 0,
        twohand::helpKind(st.helping), twohand::transition(st.holding), st.offBefore, units, degrees,
        glm::length(st.shift), vr_flat_aim_fov.value);
}

} // namespace qvr::flatsight

// gl_screen.c (VR_FlatAdsFov, vr_api.h): the flatscreen FOV, narrowed while the ironsight is on. Off, or outside the
// mode, it is the FOV asked for -- unchanged for a headset run and for plain flatscreen play.
extern "C" int VR_FlatAdsActive(void)
{
    return qvr::flatsight::aiming() ? 1 : 0;
}

extern "C" float VR_FlatAdsFov(float basefov)
{
    return qvr::flatsight::fov(basefov);
}
