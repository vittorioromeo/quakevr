// vr_flatsight.cpp -- the flatscreen player's ironsight, and his free hand on a grip. Read vr_flatsight.hpp for what the
// two parts do; here is how they are measured.
//
// The grip. The view already offers the held weapon's two-handed grip point every frame it draws a weapon:
// hands::State::grip2H[holding], the hotspot it chose for the helping hand, placed with the muzzles (vr_view.cpp's
// placeHotspots, vr_view_weapon.cpp's chooseGrip). A placed point is only good for the frame it was placed in, so it is
// carried to this frame's pose the way a muzzle is -- from hands::State::placedFrom, where the hand was when the view
// placed it (the same carry hands::handMuzzle uses for the shot). The empty hand is eased onto it, starting from where
// flatplay put it (its support pose), and vr_twohand.cpp's apply runs over the pose: standing on the hotspot it takes
// hold by its own tolerances (foregripTake 5.5 units to take, foregripKeep 20 times the hotspot's stickiness to keep),
// its helpKind is 1, the game code gets the two-handed aiming bits, and the weapon's weight, recoil and spread become
// the two-handed ones. E's reach is added on top of the pin, staying inside the keep. Only a weapon whose model has
// such a hotspot ahead of the holding hand is held this way (carryToGrip, in apply): any other keeps flatplay's pose,
// and a weapon whose Two-Handed setting is "Not Allowed" is refused by vr_twohand itself. Dual wielding never enters here: both hands hold, so there is no empty
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
// The ironsight. Only for a gun with a sight line (sightalign::alignable: a ranged weapon; the melee weapons
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

#include "Zancle/Math/Acos.hpp"
#include "Zancle/Math/Clamp.hpp"
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
    int toggles{0};

    double lastTime{-1e9};      // the eases below step once a frame (the pose is made several times a frame)
    bool gripWanted{false};     // what apply found last frame: a grip to hold, a weapon to centre
    bool adsWanted{false};
    float gripT{0.f};           // how far the empty hand has gone onto the grip (0: flatplay's own support pose)
    float adsT{0.f};            // how far the held weapon has come onto the crosshair

    int holding{-1};            // this frame: the hand that holds a weapon while the other is empty (-1: neither)
    int helping{-1};            // ... and the empty hand that holds on to it
    bool pinned{false};         // it was put on the weapon's hotspot (the weapon has one)
    int edgeFrame{-1};          // the frame the arrival at a grip was decided in (the pose is made several times a frame)
    bool armed{false};          // ... and the press it stands for, kept for both poses of that frame
    bool atGrip{false};         // last frame: the empty hand stood on a hotspot (its own take distance) -- the edge
    int lastHolding{-1};        // last frame's pair: a weapon changing disarms the hand that was reaching
    int lastHelping{-1};
    glm::vec3 shift{0.f};       // how far the holding hand moved onto the crosshair's ray (world, units)
    glm::vec3 adsOff{0.f};      // and how far vr_flat_ads_fwd / _up / _out moved it before that
    float offBefore{0.f};       // its muzzle off that ray before the move, and after it (units)
    float offAfter{0.f};
    float sightAngle{0.f};      // the same offset as an angle, at the muzzle's own distance (degrees)

    float gripOff{0.f};         // the grip's own measures, printed by the status: the free hand off the hotspot, less
    float gripDot{0.f};         // its bias, and the hands' line against the held hand's forward (vr_2h_angle_threshold)
    float handGap{0.f};         // the hands' distance, and the muzzle's off the held hand (twohand's beforeMuzzle)
    float muzzleGap{0.f};
    float gripReach{0.f};       // where the weapon's grip is from the hand holding it, in that hand's own frame
    float gripFwd{0.f};         // (read before this file moves the free hand onto it: the flat pose's own reach)
    float gripRight{0.f};
    float gripUp{0.f};

    float fovNarrow{0.f};       // the field of view the ironsight is narrowing by this frame (gl_screen.c reads it)
};

Sight st;

constexpr float gripEase = 9.f;  // how fast an empty hand travels onto a grip, and back off it (1/s)
constexpr float aimTimeMin = 0.05f;  // vr_flat_aim_time's shortest travel, and what a smaller nonzero setting reads as
constexpr float aimTimeMax = 1.f;    // and its longest

[[nodiscard]] float ease(float k, float dt)
{
    return dt > 0.f ? 1.f - za::exp(-k * dt) : 1.f;
}

// vr_flat_aim_time: how long the ironsight's whole travel takes -- onto the crosshair, and back off it -- in seconds
// (0.05..1.00; 0: at once). How far that travel has gone is st.adsT, which is also what the field of view is narrowed
// by (fovNarrowNow), so the view closes and opens with the weapon rather than at the press.
[[nodiscard]] float aimSeconds()
{
    const float t = vr_flat_aim_time.value;
    return t <= 0.f ? 0.f : za::clamp(t, aimTimeMin, aimTimeMax);
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

// How far a point stands off a ray: across it, and as an angle at the point's own distance along it.
void offTheRay(const glm::vec3& org, const glm::vec3& dir, const glm::vec3& p, float& units, float& degrees)
{
    const glm::vec3 d = p - org;
    const float len = glm::length(d);
    units = len > 1e-6f ? glm::length(d - dir * glm::dot(d, dir)) : 0.f;
    degrees = len > 1.f ? glm::degrees(za::acos(za::clamp(glm::dot(d, dir) / len, -1.f, 1.f))) : 0.f;
}

// The grip's case: one hand holds a weapon and the other is empty; both -1 otherwise.
void oneHanded(int& holding, int& helping)
{
    const bool mainHolds = flatplay::heldIn(HAND_MAIN) >= 0;
    const bool offHolds = flatplay::heldIn(HAND_OFF) >= 0;
    holding = mainHolds == offHolds ? -1 : mainHolds ? HAND_MAIN : HAND_OFF;
    helping = mainHolds == offHolds ? -1 : mainHolds ? HAND_OFF : HAND_MAIN;
}

// The grip's press. A flatscreen hand is always gripping (vr_client.cpp's gripping), so the pose putting the empty hand
// onto a grip has no press to be taken on; this is the frame that stands for it, measured the way vr_twohand measures
// the take: the hotspot's own point, less its bias, within its take distance, and the line from the held hand to the
// free one past the dot twohand asks for (vr_2h_angle_threshold) against the held hand's forward. A cup (grip2HCup)
// and a palm grip (grip2HPalm) are held by twohand's own rules rather than by an arrival here, so those keep
// flatplay's support pose as it is today.
bool gripArrival(const hands::State& s, float& offGrip, float& dot)
{
    offGrip = dot = 0.f;
    if(st.helping < 0 || st.holding < 0 || !s.grip2HValid[st.holding] || s.grip2HCup[st.holding] || s.grip2HPalm[st.holding])
    {
        return false;
    }
    offGrip = glm::distance(s.pos[st.helping], s.grip2H[st.holding]) - s.grip2HBias[st.holding];
    glm::vec3 fwd, right, up;
    hands::angleVectors(s.rot[st.holding], fwd, right, up);
    const glm::vec3 line = s.pos[st.helping] - s.pos[st.holding];
    // vr_2h_angle_threshold is twohand's dot, not an angle in degrees (vr_twohand.cpp's goodDot): compared as it is there.
    dot = glm::length(line) > 1e-4f ? glm::dot(glm::normalize(line), fwd) : 0.f;
    return offGrip < twohand::gripTakeUnits && dot > vr_2h_angle_threshold.value;
}

// The ironsight asked for, and holding what it applies to: a ranged weapon, in one hand alone. In flatscreen play
// only -- a headset run never has it, whatever the flatscreen player left behind when a headset took the game back.
bool adsApplies()
{
    return flatplay::active() && st.ads && st.holding >= 0 && sightalign::alignable(st.holding);
}

// How much of the field of view the ironsight is taking this frame: vr_flat_aim_fov's degrees times how far the weapon
// has travelled onto the crosshair (st.adsT, over vr_flat_aim_time) -- so the view closes over those same seconds and
// opens back off them, whatever the toggle says meanwhile, and nothing is taken outside flatscreen play or once the
// travel is back to nothing.
[[nodiscard]] float fovNarrowNow()
{
    const float done = za::clamp(st.adsT, 0.f, 1.f);
    if(!flatplay::active() || done <= 0.f)
    {
        return 0.f;
    }
    return za::clamp(vr_flat_aim_fov.value, 0.f, 45.f) * done;
}

void setAds(bool on, const char* by)
{
    if(on == st.ads)
    {
        return;
    }
    st.ads = on;
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

// The empty hand onto the held weapon's two-handed grip hotspot, eased from where flatplay put it. Only for a grip the
// pose can actually bring a hand to (carryToGrip, in apply below).
void pinToGrip(hands::State& s, const glm::vec3& reach, bool carry)
{
    st.pinned = false;
    if(!carry || st.holding < 0 || st.gripT <= 0.f)
    {
        return;
    }
    // The hotspot as the view placed it last frame, carried to this frame's pose with the holding hand.
    const glm::vec3 grip = s.grip2H[st.holding] + (s.pos[st.holding] - s.placedFrom[st.holding]);
    const glm::vec3 out = st.helping == HAND_OFF ? reach * st.gripT : glm::vec3{0.f}; // E still reaches out with it
    s.pos[st.helping] += (grip - s.pos[st.helping]) * st.gripT + out;
    // vr_twohand measures the take and the keep against the point this pose reaches (apply puts the view's own point
    // back after it); the view places it again with the weapon it draws, which is the same point moved the same way.
    s.grip2H[st.holding] = grip;
    st.pinned = true;
}

// The held weapon brought onto the crosshair's ray, by the muzzle its shots go from; and the ironsight's own numbers
// (vr_flat_ads_fwd / vr_flat_ads_up / vr_flat_ads_out), put on the pose before that bringing so the bringing works on the
// muzzle they leave.
void centreOnCrosshair(hands::State& s, const glm::vec3& aim)
{
    st.shift = st.adsOff = glm::vec3{0.f};
    st.offBefore = st.offAfter = st.sightAngle = 0.f;
    if(st.holding < 0)
    {
        return;
    }

    // The ironsight's own numbers first, on the pose the mode made and over the same seconds: forward, up and out at the
    // holding hand's own side (the same side flatplay's vr_flat_gun_out measures out to), in the view's own frame -- the
    // frame the sight picture lives in. It goes on before the muzzle is measured and the weapon brought onto the crosshair
    // because the bringing is what answers the muzzle's distance off that ray: applied after it, the offsets would carry
    // an already-centred weapon aside and the shot, which leaves the muzzle along the crosshair's own direction
    // (aimRotAtCrosshair solves the hand's turn from the crosshair, whatever the hand sits at), would pass a foe standing
    // on the crosshair that far wide. Measured here as st.adsOff and re-measured as offBefore / offAfter.
    glm::vec3 fwd{}, right{}, up{};
    hands::angleVectors(aim, fwd, right, up);
    const float side = st.holding == HAND_MAIN ? 1.f : -1.f;
    st.adsOff = (fwd * flatplay::poseNum(vr_flat_ads_fwd) + right * (side * flatplay::poseNum(vr_flat_ads_out)) +
                    up * flatplay::poseNum(vr_flat_ads_up))
                * st.adsT;
    if(st.pinned)
    {
        // The grip the other hand is holding travels with the hand it holds on to: their distance apart is unchanged.
        s.grip2H[st.holding] += st.adsOff;
        s.pos[st.helping] += st.adsOff;
    }
    s.pos[st.holding] += st.adsOff;

    // Measured whatever the ironsight is doing: where the muzzle this frame's pose leaves the crosshair's ray stands,
    // which the ironsight's own turn is measured against and printed as the pair before -> after.
    glm::vec3 org, dir;
    flatplay::crosshairRay(s, aim, org, dir);
    const glm::vec3 m = muzzleNow(s, st.holding);
    offTheRay(org, dir, m, st.offBefore, st.sightAngle);
    st.offAfter = st.offBefore;
    if(st.adsT <= 0.f)
    {
        return;
    }

    // Brought onto the ray: the shift is across it, nothing along it, spent over the travel (st.adsT) -- the easing of the
    // weapon onto the crosshair, whatever the offsets did to the muzzle on the way. What is left of them at the end of
    // the travel is what lies along the ray: vr_flat_ads_fwd, how far out from him the sight picture sits. The parts
    // across the ray are answered by the centring, which is what keeps the shot on the crosshair; the same seconds, and
    // at 0 0 0 nothing happens at all, which is why the defaults reproduce the muzzle-on-the-ray numbers.
    const glm::vec3 lat = m - org - dir * glm::dot(m - org, dir); // off the ray: across it, nothing along it
    st.shift = -lat * st.adsT;
    if(glm::dot(st.shift, st.shift) < 1e-8f)
    {
        st.offAfter = glm::length(lat);
        return;
    }
    s.pos[st.holding] += st.shift;
    if(st.pinned)
    {
        s.grip2H[st.holding] += st.shift;
        s.pos[st.helping] += st.shift;
    }
    // Re-measured from the muzzle the pose now leaves: the shot goes from there, so this is what the crosshair's own line
    // ends up standing to.
    offTheRay(org, dir, muzzleNow(s, st.holding), st.offAfter, st.sightAngle);
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
        st.holding = st.helping = st.lastHolding = st.lastHelping = -1;
        st.pinned = st.atGrip = st.armed = false;
        st.edgeFrame = -1;
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
    // The ironsight's travel is spent at a steady rate, so it is over in the cvar's own seconds; 0 asks for the whole
    // of it this frame. The weapon's shift onto the crosshair (apply) and the field of view go by the same fraction.
    const float aim = aimSeconds();
    st.adsT = aim <= 0.f ? (st.adsWanted ? 1.f : 0.f)
                         : za::clamp(st.adsT + (st.adsWanted ? 1.f : -1.f) * dt / aim, 0.f, 1.f);

    pollKey();

    // The field of view is chosen when the engine recalculates its view (gl_screen.c's SCR_CalcRefdef through
    // VR_FlatAdsFov), and that happens on demand: ask for it the frame the ironsight's share of it changes, which
    // while it is travelling is every frame -- and a weapon with no sights taken up while it is on, or a
    // vr_flat_aim_fov or vr_flat_aim_time moved from the console or the menu.
    const float narrow = fovNarrowNow();
    if(narrow != st.fovNarrow)
    {
        st.fovNarrow = narrow;
        vid.recalc_refdef = 1;
    }
}

void apply(hands::State& s, const glm::vec3& aim, const glm::vec3& reach)
{
    // Only flatplay::poseHands calls this, so only in the mode. s.valid is not asked for: the flat pose is made in
    // hands::placeFlat, which runs before hands::update raises that flag for the frame (vr_hands.cpp), and what is read
    // here is the pose flatplay just made and the hotspot and muzzle the view placed last frame -- the same thing
    // flatplay's own pose works on.
    oneHanded(st.holding, st.helping);
    st.adsWanted = adsApplies();
    // The pose is made several times a frame in flatscreen play (flatplay::poseHands: at the frame's start, for the move,
    // for the view), so the arrival at a grip, and the press it stands for, is decided once a frame and the same for all
    // of them.
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

    // The hotspot as the view placed it last frame. This pose moves it with the weapon for vr_twohand's measures and puts
    // it back at the end: each of the frame's poses carries it from the view's own point, so the carries do not add up.
    const glm::vec3 placedGrip = st.holding >= 0 ? s.grip2H[st.holding] : glm::vec3{0.f};

    // Where the held weapon's grip stands from the hand that holds it, as the view placed the two together last frame
    // (this frame's pose is not on the crosshair yet: the ironsight's move is still to come), in the held hand's frame.
    st.gripReach = st.gripFwd = st.gripRight = st.gripUp = 0.f;
    if(st.holding >= 0 && s.grip2HValid[st.holding])
    {
        glm::vec3 fwd, right, up;
        hands::angleVectors(s.rot[st.holding], fwd, right, up);
        const glm::vec3 d = placedGrip - s.placedFrom[st.holding];
        st.gripReach = glm::length(d);
        st.gripFwd = glm::dot(d, fwd);
        st.gripRight = glm::dot(d, right);
        st.gripUp = glm::dot(d, up);
    }

    // The free hand is carried to a grip that stands where a second hand can reach it: ahead of the held hand, along the
    // line vr_twohand asks its dot about. A grip that stands on or behind the hand holding the weapon (the shotgun's, the
    // super shotgun's, the nail gun's: measured 5 units off it, 3 to 5 units to the rear) is not a place a second hand
    // can go, and a cup or a palm grip is held by vr_twohand's own rules. Both keep the support pose flatplay gives them
    // today, with the hands where that pose puts them.
    const bool carryToGrip = st.holding >= 0 && st.helping >= 0 && s.grip2HValid[st.holding] && !s.grip2HCup[st.holding] &&
                             !s.grip2HPalm[st.holding] && st.gripFwd > vr_2h_angle_threshold.value * st.gripReach;
    st.gripWanted = carryToGrip;
    pinToGrip(s, reach, carryToGrip);
    centreOnCrosshair(s, aim);

    // The grip's press, and only its press: true on the frame the empty hand arrives on the hotspot, false the frame it
    // leaves it or the weapon changes. applyHotspots' goodDistance, goodDot, the stickiness and the weapon's own
    // Two-Handed setting decide the hold from here on, exactly as they do for a controller's grip button.
    const bool inside = gripArrival(s, st.gripOff, st.gripDot);
    st.handGap = st.muzzleGap = 0.f;
    if(st.holding >= 0 && st.helping >= 0)
    {
        st.handGap = glm::distance(s.pos[st.helping], s.pos[st.holding]);
        st.muzzleGap = s.muzzleValid[st.holding] ? glm::distance(s.pos[st.holding], s.muzzle[st.holding]) : 0.f;
    }
    if(freshFrame)
    {
        // Once per arrival, and again on each frame the pose still holds the hand on a grip nothing has taken: a
        // flatscreen hand is not lifted off a grip it was placed on, so a press missed on the frame the weapon changed
        // would never come again. It stops the frame vr_twohand takes the hold.
        st.armed = st.helping >= 0 && inside && (!st.atGrip || !twohand::helping(st.helping));
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

    if(st.holding >= 0)
    {
        s.grip2H[st.holding] = placedGrip;
    }

    // On the flat screen the crosshair is the aim, whatever the grip made of the held hand's turn (in a headset that
    // turn is the aim, and is kept): put the pose's angles back, and the shots keep going along the crosshair.
    for(int h = 0; h < HAND_COUNT; h++)
    {
        s.rot[h] = crosshair[h];
    }
}

float adsShare()
{
    // How far this frame's travel has gone, whatever the toggle says about the next one: what the weapon's shift onto the
    // crosshair, the field of view's narrowing and flatplay's easing of vr_flat_gun_near into vr_flat_ads_near are all
    // spent over. 0 outside flatscreen play (frame() puts the travel back to nothing there).
    return za::clamp(st.adsT, 0.f, 1.f);
}

float fov(float basefov)
{
    // The eased share, not the toggle (fovNarrowNow): the view closes and opens with the weapon. A base field of view
    // is never taken under 20 degrees, where the engine's own view stops making sense.
    const float narrow = fovNarrowNow();
    return narrow > 0.f ? za::max(20.f, basefov - narrow) : basefov;
}

void statusLine()
{
    Con_Printf("  ironsight: %s (vr_flat_aim, or the mouse wheel's button; vr_flat_aim_fov %g off the field of view, "
               "taken over %g s of travel (vr_flat_aim_time); %d toggles this session; its own offsets on top of the "
               "aligned pose vr_flat_ads_fwd %g vr_flat_ads_up %g vr_flat_ads_out %g, in the view's frame)%s\n",
        st.ads ? "asked for" : "off", vr_flat_aim_fov.value, vr_flat_aim_time.value, st.toggles,
        flatplay::poseNum(vr_flat_ads_fwd), flatplay::poseNum(vr_flat_ads_up), flatplay::poseNum(vr_flat_ads_out),
        st.ads && st.holding >= 0 && !sightalign::alignable(st.holding) ? ", no sights on the weapon held" : "");
    if(st.holding >= 0)
    {
        Con_Printf("  two hands: %s hand holds, the %s hand %s (%s holding: helpKind %d, transition %.2f)\n",
            st.holding == HAND_MAIN ? "main" : "off", st.helping == HAND_MAIN ? "main" : "off",
            st.pinned ? "on its grip hotspot" : "where the mode's pose puts it (no grip there a second hand can take)",
            twohand::helping(st.helping) ? "held" : "not held", twohand::helpKind(st.helping),
            twohand::transition(st.holding));
        if(st.pinned)
        {
            // The measures vr_twohand takes the hold on, read at the frame's pose: the free hand off the hotspot (less
            // its bias, against the take distance an untaken grip asks for), the hands' line against the held hand's
            // forward (against its dot), and the two distances of its muzzle test.
            Con_Printf("  grip: %.2f units off the hotspot (take %.2f), dot %.2f (needs %.2f), hands %.1f apart, "
                       "muzzle %.1f off the held hand\n",
                st.gripOff, twohand::gripTakeUnits, st.gripDot, vr_2h_angle_threshold.value, st.handGap, st.muzzleGap);
        }
        if(st.gripReach > 0.f)
        {
            // Where the held weapon's grip stands from the hand holding it, printed whatever the mode makes of it: the
            // measure the choice above rests on (a grip to the rear of that hand is not a place a second hand can go).
            Con_Printf("  hotspot: %.1f units from the held hand (fwd %.1f, right %.1f, up %.1f of it)\n",
                st.gripReach, st.gripFwd, st.gripRight, st.gripUp);
        }
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
    flatplay::crosshairRay(s, geom::vec(cl.viewangles), org, dir);
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
               "muzzle off the crosshair %.2f -> %.2f units (%.2f deg) hand moved %.2f fov take %g ads %g %g %g\n",
        vr_gametime, adsApplies() ? "on" : (st.ads ? "no sights" : "off"), st.adsT, r_refdef.fov_x, r_refdef.fov_y,
        st.holding == HAND_MAIN ? "main" : "off", st.pinned ? "hotspot" : "none", twohand::helping(st.helping) ? 1 : 0,
        twohand::helpKind(st.helping), twohand::transition(st.holding), st.offBefore, units, degrees,
        glm::length(st.shift), vr_flat_aim_fov.value, flatplay::poseNum(vr_flat_ads_fwd),
        flatplay::poseNum(vr_flat_ads_up), flatplay::poseNum(vr_flat_ads_out));
}

} // namespace qvr::flatsight

// gl_screen.c (VR_FlatAdsFov, vr_api.h): the flatscreen FOV, narrowed while the ironsight is on. Off, or outside the
// mode, it is the FOV asked for -- unchanged for a headset run and for plain flatscreen play.
extern "C" float VR_FlatAdsFov(float basefov)
{
    return qvr::flatsight::fov(basefov);
}
