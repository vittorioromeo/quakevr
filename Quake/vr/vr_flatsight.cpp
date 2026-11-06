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
// The ironsight. Only for a gun with a sight line (sightalign::sightLine: a rear and a front point on its model, the same
// ones Align Sights to My Aim reads; the melee weapons have none) held in one hand alone. The hand is solved from them: the
// sight line, as the hand holds it (noteDrawn: measured off the weapon as drawn), turned onto the crosshair's ray and slid
// along it until the rear sight stands vr_flat_ads_relief from the eye. So both sights lie on the ray whatever the
// weapon, the field of view or the eye's nudges, with no number per weapon. flatplay::drawnOffset does the same solve last,
// from the camera the frame is drawn with (drawnSight): the looks are spent out over the travel, and nothing else moves
// the drawn gun off it. The game's hand stands at the same place, its muzzle on the drawn gun's (centreOnSight), turned to
// shoot at what the crosshair is on (the aim, below): the bore lies under the sights, so the two turns differ a little.
// vr_flat_ads_screen_x / _y move the whole sight picture on the screen: the ray itself turns (rayDir), so the sights stay
// on it and the crosshair and the shots go with them. The field of view narrows with the travel (vr_flat_aim_fov, through
// VR_FlatAdsFov in gl_screen.c); the engine's own IN_FovScale keeps the crosshair's speed on the screen, and the HUD does
// not scale with the FOV.
//
// The walls. A held weapon goes back along the way into the body until it is out of them (pushBack: handpose::gunBack, the
// headset's stop's points and traces): towards its shoulder, along the ray in the ironsight, whose sights stay on it. It
// is the game's pose that moves: the gun aims from where it is pushed to, and the shots leave from there. Models stop the
// drawn gun as they do in a headset (vr_modelcollide.cpp, run by the view whatever the mode).
//
// The aim. A gun (one with a sight line, in either hand, ironsight or not) points from its muzzle at what the ray through
// the middle of the screen hits (aimAtPoint): traced every pose, as the shot's line is (the local server's: vr_trace.hpp),
// far along the ray if nothing is there. QC's shots leave along the shot's angles on the line through the muzzle
// (VR_ShotBackPoint, from behind it: what the muzzle stands in is hit too), so they go through the crosshair's point
// wherever the muzzle stands: offsets, the eye's nudges, near targets. A point nearer than aimMinAhead past the muzzle
// (a wall in his face, the gun inside a monster's box) is not pointed at: no gun turned back on itself. The muzzle is
// carried with the hand's turn, not only its move (muzzleNow, notePlaced): the turn moves it, and the shot
// must leave from where the aim was measured. flatplay::drawnOffset eases the drawn gun onto this turn.
//
// None of it runs outside flatscreen play: every entry point says so, and a headset run is untouched.

#include "vr_flatsight.hpp"

#include "vr_api.h"
#include "vr_backend.hpp" // HAND_OFF / HAND_MAIN / HAND_COUNT
#include "vr_cvars.hpp"
#include "vr_avatar.hpp"
#include "vr_client.hpp"
#include "vr_engine.hpp"
#include "vr_flatplay.hpp"
#include "vr_geom.hpp"
#include "vr_handpose.hpp"
#include "vr_hands.hpp"
#include "vr_sightalign.hpp"
#include "vr_trace.hpp"
#include "vr_twohand.hpp"
#include "vr_view.hpp"
#include "vr_weapons.hpp"

#include "Zancle/Math/Acos.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Exp.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sqrt.hpp"
#include "Zancle/Math/Tan.hpp"
#include "vr_zancle.hpp"

#include <glm/gtc/quaternion.hpp>

extern "C" qboolean keydown[MAX_KEYS]; // keys.c (the game's own key state, as flatplay reads E and V from it)

namespace qvr::flatsight
{

namespace
{

// The state for the session (no function-local statics: CODE_STYLE.md).
struct Sight
{
    bool ads{false};            // the ironsight is asked for (the right mouse button with one weapon held, vr_flat_aim)
    bool keyWas{false};         // that button, as the ironsight's, last frame: a press edge
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
    // The stance (vr_flat_stance; stanceAndLook): a long gun with a two-handed hotspot turns the torso, and the free hand's
    // drawn place and turn are the hotspot's (a look: its game hand stays where the pose has it).
    bool stanceWanted{false};   // this frame's gun is such a gun
    bool stanceGun{false};      // ... and its hand comes in for it: not on a real grip, whose pose stays the game's own
    float stanceT{0.f};         // how far the torso has turned
    float stanceSign{-1.f};     // which way (the free hand's shoulder forward); kept for the way back
    bool stancePistol{false};   // a pistol's stance, not a long gun's (vr_flat_stance_pistol); kept for the way back
    int stanceHand{-1};         // the hand holding the gun it is for, the same
    bool lookWanted{false};     // the free hand is going to the hotspot
    bool lookFast{false};       // ... or was taken off it by a punch or E: off as fast as a grip's
    bool lookFar{false};        // ... its arm can't reach it, in this pose
    float lookT{0.f};           // how far it has gone
    float lookTarget{1.f};      // how far it goes, its arm's need allowing (hands on the body)
    float lookSlide{0.f};       // how far down the fore-end from the hotspot it holds, where the arm can't take the hotspot (units)
    int lookHand{-1};           // the hand that is, or was last, on it
    glm::mat3 lookView{1.f};    // the view's frame (forward, right, up) and pitch as the stance was last decided: elbowPole's
    float lookPitch{0.f};
    int spot[2]{-1, -1};        // each hand's weapon's two-handed hotspot (view::twoHandSpot), as the hand held it when last drawn
    glm::vec3 spotLocal[2]{glm::vec3{0.f}, glm::vec3{0.f}}; // ... in that hand's frame
    float armHip{0.f};          // the share of its arm the free hand uses on it: the gun where the hipfire pose has it
    float armSight{0.f};        // ... and on the sight's place
    glm::vec3 drawnLocal{0.f};  // the free hand as drawn last frame, in the held hand's frame (forward, right, up), and how it
    glm::vec3 drawnTurn{0.f};   // is turned there: what the status prints against the hotspot
    bool drawnKnown{false};
    glm::vec3 spotHandOff{0.f}; // the free hand fully on the hotspot, against the hotspot (its own offsets: noteDrawn), and for which
    const qmodel_t* spotHandModel{nullptr}; // gun
    glm::vec3 shift{0.f};       // how far the holding hand moved onto the sight's place (world, units)
    float offBefore{0.f};       // its muzzle off the crosshair's ray before the move, and after it (units)
    float offAfter{0.f};
    float offAsked{0.f};        // ... which the sight's place asks to be this (the bore under the sights, the travel's share)
    glm::vec3 muzzleLocal[2]{glm::vec3{0.f}, glm::vec3{0.f}}; // each muzzle in its hand's frame (forward, right, up), as
    bool muzzleKnown[2]{false, false};                        // the view last placed it (notePlaced)
    bool aimBlocked[2]{false, false};                         // something lies between the muzzle and what the crosshair is on (aimAtPoint)
    float sightAngle{0.f};      // the same offset as an angle, at the muzzle's own distance (degrees)

    // The sight line as each hand held it when last drawn, in that hand's frame (noteDrawn): what the ironsight is solved
    // from. For this model only: another weapon has no sight known until it is drawn.
    glm::vec3 sightRear[2]{glm::vec3{0.f}, glm::vec3{0.f}};
    glm::vec3 sightFront[2]{glm::vec3{0.f}, glm::vec3{0.f}};
    const qmodel_t* sightModel[2]{nullptr, nullptr};
    glm::vec3 stock[2]{glm::vec3{0.f}, glm::vec3{0.f}}; // the weapon's rear off its hand, in the view's frame (stockOff)
    const qmodel_t* stockModel[2]{nullptr, nullptr};
    float relief{0.f};         // this pose's eye relief: vr_flat_ads_relief, as far as the arm allows (reliefFor)

    float backNeed[2]{0.f, 0.f}; // how far each hand's weapon must go back to be out of the walls, this pose's measure
    float back[2]{0.f, 0.f};     // and how far it is back, eased once a frame (pushBack)
    glm::vec3 backWay[2]{glm::vec3{0.f}, glm::vec3{0.f}}; // the way it goes, in the view's frame
    float tiltNeed[2]{0.f, 0.f}; // how far each drawn weapon's muzzle must tilt up to be out of what the way back can't clear
    float tilt[2]{0.f, 0.f};     // (degrees, this pose's measure) and how far it is tilted up, as drawn (wallTilt)
    bool tiltWanted[2]{false, false}; // the wall needs the tilted pose this frame (tiltNeedFor)
    float tiltOn[2]{0.f, 0.f};    // how far into the tilted pose, 0..1 over vr_flat_wall_tilt_time (eased: smooth01)
    float tiltPitch[2]{0.f, 0.f}; // the tilted pose's pitch, following tiltNeed (held as it eases out)
    float tiltShare[2]{0.f, 0.f}; // the share of the yaw and roll (the sighted share: tiltNeedFor's sightFade)
    float tiltAngle[2]{0.f, 0.f}; // the whole drawn turn (degrees: wallTilt, wallBlocksFire)
    float tiltDepth[2]{0.f, 0.f}; // how deep in the walls it stood before the tilt, and after it (units; the status prints them)
    float tiltLeft[2]{0.f, 0.f};
    glm::vec3 tiltPivot[2]{glm::vec3{0.f}, glm::vec3{0.f}}; // what it turns round, off the hand in the view's frame

    float gripOff{0.f};         // the grip's own measures, printed by the status: the free hand off the hotspot, less
    float gripDot{0.f};         // its bias, and the hands' line against the held hand's forward (vr_2h_angle_threshold)
    float handGap{0.f};         // the hands' distance, and the muzzle's off the held hand (twohand's beforeMuzzle)
    float muzzleGap{0.f};
    float gripReach{0.f};       // where the weapon's grip is from the hand holding it, in that hand's own frame
    float gripFwd{0.f};         // (read before this file moves the free hand onto it: the flat pose's own reach)
    float gripRight{0.f};
    float gripUp{0.f};

    float fovNarrow{0.f};       // the field of view the ironsight is narrowing by this frame (gl_screen.c reads it)
    float baseFov{90.f};        // the view's own field of view before the ironsight's narrowing (fov's last call)
    glm::vec3 sightMuzzle{0.f}; // where the sight's place has the muzzle, this pose (world; the travel's share of it)
};

Sight st;

constexpr float gripEase = 9.f;  // how fast an empty hand travels onto a grip, and back off it (1/s)
constexpr float lookEase = 2.5f;      // how fast the free hand travels onto a long gun's hotspot and back off it (1/s): about 0.4 s
constexpr float stanceEase = 2.5f;    // and how fast the torso turns, the same
constexpr float longGunMuzzle = 9.5f; // a gun's muzzle this far from the hand holding it (units) makes it a long gun.
                                      // Measured: revolver 8.4, nail pistol 3.9; nail gun 10.8, super shotgun 11.4, shotgun 14.0
constexpr float lookBack = 0.03f;     // the free hand goes back on the hotspot under lookKeep less this share of the arm
constexpr float aimTimeMin = 0.05f;  // vr_flat_aim_time's shortest travel, and what a smaller nonzero setting reads as
constexpr float aimTimeMax = 1.f;    // and its longest
constexpr float aimFar = 4096.f;     // nothing on the crosshair's ray: the gun points this far along it
constexpr float aimMinAhead = 12.f;  // what the crosshair is on nearer than this past the muzzle, along the ray: not pointed at
constexpr float aimFullAhead = 24.f; // and from this far on the muzzle's own place is the shot's; between, its start eases onto the ray
constexpr float aimBlockedBy = 1.f;  // the muzzle's line stopping this far short of what the crosshair is on: blocked
constexpr int aimSteps = 8;          // the turn moves the muzzle the aim is measured from: each pass closes most of what is left
constexpr float minFov = 20.f;      // the ironsight never takes a view under this field of view (degrees)
constexpr float reliefMin = 3.f;     // vr_flat_ads_relief's range, and the nearest a wall pushes the rear sight to the eye
constexpr float reliefMax = 30.f;
constexpr float handNearEye = 7.f;   // the hand on a gun whose sights stand far ahead of it (the laser cannon's) is no nearer the
                                     // eye than this along the line: the rear sight waits further out, the arm is not crushed
constexpr float lookKeep = 0.98f;    // the free hand goes off the long gun's hotspot past this much of what its arm holds a hand at
constexpr float lookSoftFrom = 0.9f;  // hands on the body: it starts going off here, all of it by lookKeep (the pose keeps the hotspot
                                      // within vr_flat_keep_grip, 0.87, so it holds on at any look: phase 3; it was 0.78)
constexpr float armKeep = 0.97f;     // the sight's hand keeps inside this much of what the arm holds a hand at
constexpr float spotKeepAds = 0.95f; // sighted in a stance, the gun stands where the free arm reaches its hotspot within this much
constexpr float lookSoftFromAds = 0.96f; // and the free hand starts going off it past this much, all of it by its whole reach
constexpr float lookKeepAds = 1.f;
constexpr float spotKeepHip = 0.88f;  // hipfire, the same: the pose keeps the hotspot within vr_flat_keep_grip (0.87), and past this it slides
constexpr float spotReliefMin = 9.5f; // sighted, the hotspot brings the rear sight no nearer the eye than this: the slide does the rest
constexpr float slideKeepAhead = 3.f; // the free hand slid down the fore-end stays this far ahead of the holding hand (units)
constexpr float gripKeep = 0.92f;    // and the other hand's grip inside this much: the arm bent, not straight
constexpr float elbowUpFrom = 15.f;  // elbowPole: the free arm's elbow starts going under the gun from this far up (degrees)
constexpr float elbowUpFull = 60.f;  // and is all the way there by this far up (smooth between)
constexpr float wallElbowFull = 3.f; // elbowPole: the gun hand's elbow is all the way out with its gun this far back off a wall (units)
constexpr float wallElbowDown = 0.5f; // and goes this far down per its way out
constexpr float screenMax = 25.f;    // vr_flat_ads_screen_x / _y's range: percent of the screen's height
constexpr float backIn = 80.f;       // how fast a weapon goes back from a wall it meets (1/s: at once, nearly, as vr_model_collide's)
constexpr float backOut = 12.f;      // and comes out again as the wall clears: no jump
constexpr float wallArmMax = 20.f;   // vr_flat_wall_arm_keep's range (units)
constexpr float wallTiltMax = 85.f;  // vr_flat_wall_tilt's range (degrees)
constexpr float tiltFrom = 0.25f;    // the weapon this deep in a wall at the most the way back takes it (units): its muzzle tilts
constexpr float tiltStep = 10.f;     // the tilt's search: tried in steps this big (degrees), then halved this many times
constexpr int tiltHalvings = 3;
constexpr float tiltTimeMax = 1.5f;  // vr_flat_wall_tilt_time's range (seconds)
constexpr float tiltPitchEase = 4.f; // in the pose, its pitch follows the least that clears at this many over vr_flat_wall_tilt_time (1/s)
constexpr float tiltYawMax = 60.f;   // vr_flat_wall_tilt_yaw's range (degrees)
constexpr float tiltRollMax = 90.f;  // vr_flat_wall_tilt_roll's range (degrees)
constexpr float tiltBlocksFire = 5.f; // a gun turned more than this out of a wall does not fire (vr_flat_wall_block_fire; degrees)
constexpr float tiltShortOfUp = 5.f; // the tilt leaves the muzzle at least this far short of straight up (degrees)
constexpr float sightTiltDownFrom = 0.5f; // sighted, the tilt fades out looking down from 30 degrees (the pitch's sine)...
constexpr float sightTiltDownTo = 0.766f; // ... to none by 50

[[nodiscard]] float ease(float k, float dt)
{
    return dt > 0.f ? 1.f - za::exp(-k * dt) : 1.f;
}

// The view's frame as a matrix: forward, right, up (a point in that frame times this is that point in the world).
[[nodiscard]] glm::mat3 axesOf(const glm::vec3& angles)
{
    glm::vec3 f, r, u;
    hands::angleVectors(angles, f, r, u);
    return glm::mat3{f, r, u};
}

// Where the sight picture stands on the screen (vr_flat_ads_screen_x / _y, percent of the screen's height, x to the right,
// y up), the travel's share of it: 0 0 at the screen's centre.
[[nodiscard]] glm::vec2 screenShift()
{
    const float done = flatplay::active() ? za::clamp(st.adsT, 0.f, 1.f) : 0.f;
    return {za::clamp(vr_flat_ads_screen_x.value, -screenMax, screenMax) * done,
        za::clamp(vr_flat_ads_screen_y.value, -screenMax, screenMax) * done};
}

// That place as a direction in the view's frame: the ray through it, at the field of view being drawn. Forward at 0 0.
[[nodiscard]] glm::vec3 sightDir()
{
    const glm::vec2 shift = screenShift();
    if(shift == glm::vec2{0.f})
    {
        return {1.f, 0.f, 0.f};
    }
    const float t = std::tan(glm::radians(za::clamp(r_refdef.fov_y, 1.f, 170.f) * 0.5f));
    return glm::normalize(glm::vec3{1.f, shift.x * 0.02f * t, shift.y * 0.02f * t});
}

// Whether `h`'s weapon has a sight line known as the hand holds it.
[[nodiscard]] bool sightKnown(int h)
{
    const qmodel_t* m = weapons::heldModel(h);
    return m && st.sightModel[h] == m;
}

// The hand's place and turn that put the sight line on the ray, the rear sight `relief` from the eye, in the view's frame.
struct Sighted
{
    glm::vec3 hand{0.f};
    glm::mat3 turn{1.f};
};
[[nodiscard]] Sighted sighted(int h, float relief)
{
    const glm::vec3 d = sightDir();
    const glm::vec3 line = glm::normalize(st.sightFront[h] - st.sightRear[h]);
    const glm::vec3 axis = glm::cross(line, d);
    Sighted out; // (the least turn taking the line onto the ray: the gun stays upright)
    out.turn = glm::mat3_cast(glm::normalize(glm::quat{1.f + glm::dot(line, d), axis.x, axis.y, axis.z}));
    out.hand = d * relief - out.turn * st.sightRear[h];
    return out;
}

// The eye relief for this pose, the hand's shoulder being `shoulder` in the view's frame: vr_flat_ads_relief, further out
// where the sights stand far ahead of the hand, nearer where the arm cannot hold the hand that far out (a look straight up).
[[nodiscard]] float reliefScale();
[[nodiscard]] float reliefFor(int h, const glm::vec3& shoulder, const glm::vec3& helper, float span)
{
    const Sighted at0 = sighted(h, 0.f);
    const glm::vec3 d = sightDir();
    float relief = za::clamp(za::clamp(vr_flat_ads_relief.value, reliefMin, reliefMax) * reliefScale(), reliefMin, reliefMax);
    relief = za::max(relief, za::min(handNearEye, relief - reliefMin) - glm::dot(at0.hand, d)); // (a relief asked at its nearest is that near)
    const glm::vec3 c = at0.hand - shoulder;
    const float along = -glm::dot(c, d);
    const float across2 = glm::dot(c, c) - along * along;
    const float keep = span * armKeep;
    float most = along + (keep * keep > across2 ? za::sqrt(keep * keep - across2) : 0.f);
    // The other hand's grip stays in its arm's reach too: the one it holds, or the stance's hotspot it is drawn on (what the torso's
    // turn and lean leave, the gun comes nearer the eye for: the sights stay on the ray).
    const bool onGrip = st.pinned && h == st.holding;
    const bool onSpot = !onGrip && st.stanceGun && h == st.stanceHand && st.spot[h] >= 0 && flatplay::onBody();
    if(onGrip || onSpot)
    {
        const glm::vec3 local = onGrip ? glm::vec3{st.gripFwd, st.gripRight, st.gripUp} : st.spotLocal[h];
        const glm::vec3 g = at0.hand + at0.turn * local - helper;
        const float gAlong = -glm::dot(g, d);
        const float gAcross2 = glm::dot(g, g) - gAlong * gAlong;
        const float gKeep = span * (onGrip ? gripKeep : spotKeepAds);
        const float spotMost = gAlong + (gKeep * gKeep > gAcross2 ? za::sqrt(gKeep * gKeep - gAcross2) : 0.f);
        // (The hotspot brings the gun no nearer than a wall would: a look straight up with a long gun can't have both, and the
        // hand eases off there.)
        most = za::min(most, onGrip ? spotMost : za::max(spotMost, spotReliefMin));
    }
    relief = za::min(relief, most);
    return za::max(reliefMin, relief);
}

// vr_flat_aim_time: how long the ironsight's whole travel takes -- onto the crosshair, and back off it -- in seconds
// (0.05..1.00; 0: at once). How far that travel has gone is st.adsT, which is also what the field of view is narrowed
// by (fovNarrowNow), so the view closes and opens with the weapon rather than at the press.
[[nodiscard]] float aimSeconds()
{
    const float t = vr_flat_aim_time.value;
    return t <= 0.f ? 0.f : za::clamp(t, aimTimeMin, aimTimeMax);
}

// The muzzle the hand's shots go from this frame: the one the view placed last frame, carried with the hand since, its
// move and its turn. keepMuzzle hands it to the shot (hands::handMuzzle carries only the move, vr_client.cpp).
[[nodiscard]] glm::vec3 muzzleNow(const hands::State& s, int h)
{
    if(!s.muzzleValid[h] || !st.muzzleKnown[h])
    {
        return s.pos[h] + hands::forward(s.rot[h]) * 8.f;
    }
    glm::vec3 fwd, right, up;
    hands::angleVectors(s.rot[h], fwd, right, up);
    const glm::vec3& l = st.muzzleLocal[h];
    return s.pos[h] + fwd * l.x + right * l.y + up * l.z;
}

// A line from `a` towards `b`, as a shot's goes (QC FireBulletsImpl: monsters' models, gibs and heads; him and what he owns
// left out): where it first stops. `b` when it stops nowhere, or with no world to trace (not hosting).
[[nodiscard]] glm::vec3 lineStop(const glm::vec3& a, const glm::vec3& b)
{
    const auto tr = worldtrace::move(a, glm::vec3{0.f}, glm::vec3{0.f}, b, MOVE_NORMAL | MOVE_HITGIBS | MOVE_HITMODEL);
    return tr ? worldtrace::endPos(*tr) : b;
}

// What the crosshair is on: the ray through the middle of the screen, stopped.
[[nodiscard]] glm::vec3 crosshairPoint(const hands::State& s, const glm::vec3& aim)
{
    glm::vec3 org, dir;
    flatplay::crosshairRay(s, aim, org, dir);
    return lineStop(org, org + dir * aimFar);
}

// Where the shot from a muzzle at `m` starts. Its own place, with what the crosshair is on far enough past it. Nearer (a wall in his
// face, the floor at his feet, a monster the gun is inside, the muzzle past the surface), or with something in the way (a ledge the
// ray clears and the muzzle's line doesn't), it eases onto the ray, level with the muzzle, nearer than aimMinAhead all the way.
// Only the shot's start: nothing drawn moves. So the shot lands on the crosshair's point at any range, and the gun never has to
// turn hard to get there.
[[nodiscard]] glm::vec3 shotFrom(const glm::vec3& org, const glm::vec3& dir, const glm::vec3& point, const glm::vec3& m, bool blocked)
{
    const float along = glm::dot(m - org, dir);
    const float u = za::clamp((glm::dot(point - org, dir) - along - aimMinAhead) / (aimFullAhead - aimMinAhead), 0.f, 1.f);
    const float onRay = blocked ? 1.f : 1.f - u * u * (3.f - 2.f * u);
    return m + (org + dir * za::max(along, 0.f) - m) * onRay;
}

// The way the gun points, its shot starting at `from`: at the point, or along the ray where that start is on the ray.
[[nodiscard]] glm::vec3 aimWay(const glm::vec3& dir, const glm::vec3& point, const glm::vec3& from, bool blocked)
{
    return blocked || glm::dot(point - from, dir) < aimMinAhead ? dir : glm::normalize(point - from);
}

// A gun that aims at that point: a hand holding a weapon with a sight line, whose muzzle the view has placed.
[[nodiscard]] bool aimsAtPoint(const hands::State& s, int h)
{
    return flatplay::heldIn(h) >= 0 && s.muzzleValid[h] && st.muzzleKnown[h] && sightalign::alignable(h);
}

// The muzzle the shot is handed (shotFrom).
[[nodiscard]] glm::vec3 shotMuzzle(const hands::State& s, int h, const glm::vec3& aim, const glm::vec3& point)
{
    const glm::vec3 m = muzzleNow(s, h);
    if(vr_flat_aim_point.value == 0.f || !aimsAtPoint(s, h))
    {
        return m;
    }
    glm::vec3 org, dir;
    flatplay::crosshairRay(s, aim, org, dir);
    return shotFrom(org, dir, point, m, st.aimBlocked[h]);
}

void keepMuzzle(hands::State& s, int h, const glm::vec3& aim, const glm::vec3& point)
{
    if(s.muzzleValid[h] && st.muzzleKnown[h])
    {
        s.muzzle[h] = shotMuzzle(s, h, aim, point) - (s.pos[h] - s.placedFrom[h]);
    }
}

// A hand with nothing in it: no weapon but the fist, no gun, prop or foe it carries.
[[nodiscard]] bool emptyHand(int h)
{
    return (!weapons::heldModel(h) || weapons::heldSlot(h) == weapons::fistSlot()) && !twohand::carrying(h);
}

// Each gun pointed from its muzzle at what the crosshair is on (the comment at the top). The sight's muzzle stays where
// centreOnCrosshair put it, by the same travel; a muzzle not on a sight goes where the hand's turn takes it.
void aimAtPoint(hands::State& s, const glm::vec3& aim)
{
    if(vr_flat_aim_point.value == 0.f)
    {
        return;
    }
    glm::vec3 org, dir, viewFwd, viewRight, viewUp;
    flatplay::crosshairRay(s, aim, org, dir);
    hands::angleVectors(aim, viewFwd, viewRight, viewUp);
    const glm::vec3 point = crosshairPoint(s, aim);
    for(int h = 0; h < HAND_COUNT; h++)
    {
        if(!aimsAtPoint(s, h))
        {
            continue;
        }
        // In the sight the muzzle stays on the drawn gun's (st.sightMuzzle, the same travel); else it goes where the hand's
        // turn takes it.
        const bool onSight = h == st.holding && st.adsT > 0.f;
        const glm::vec3 place = onSight ? st.sightMuzzle : muzzleNow(s, h);
        st.aimBlocked[h] = glm::distance(lineStop(place, point), point) > aimBlockedBy;
        for(int i = 0; i < aimSteps; i++)
        {
            const glm::vec3 m = muzzleNow(s, h);
            const glm::vec3 want = aimWay(dir, point, shotFrom(org, dir, point, m, st.aimBlocked[h]), st.aimBlocked[h]);
            const glm::vec3 side = glm::cross(want, viewUp);
            const glm::vec3 wantUp = glm::length(side) > 1e-4f ? glm::normalize(glm::cross(side, want)) : viewUp;
            s.rot[h] = flatplay::aimRotAt(hands::anglesFromVectors(want, wantUp), h);
            s.rot[h].z += vr_fakevr_handroll.value;
            if(onSight && i + 1 < aimSteps)
            {
                const glm::vec3 back = (place - muzzleNow(s, h)) * st.adsT;
                s.pos[h] += back;
                if(st.pinned)
                {
                    s.pos[st.helping] += back;
                }
            }
        }
    }
}

// How far a point stands off a ray: across it, and as an angle at the point's own distance along it.
void offTheRay(const glm::vec3& org, const glm::vec3& dir, const glm::vec3& p, float& units, float& degrees)
{
    const glm::vec3 d = p - org;
    const float len = glm::length(d);
    units = len > 1e-6f ? glm::length(d - dir * glm::dot(d, dir)) : 0.f;
    // (From the across and along parts: acos of a cosine near 1 reads only in float's steps, 0.0198 and 0.028 degrees the first.)
    degrees = len > 1.f ? glm::degrees(za::atan2(units, glm::dot(d, dir))) : 0.f;
}

// The angle between two unit ways, the same way (flataim's, the sweep's shot_deg).
[[nodiscard]] float degreesApart(const glm::vec3& a, const glm::vec3& b)
{
    return glm::degrees(za::atan2(glm::length(glm::cross(a, b)), glm::dot(a, b)));
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

// The weapon's way back from a wall, a unit vector in the view's frame: towards the hand's shoulder in hipfire, along the
// ray in the sight, so the sights stay on it.
[[nodiscard]] glm::vec3 backWay(int h, const glm::vec3& handInView, const glm::vec3& shoulderInView)
{
    const glm::vec3 home = shoulderInView - handInView;
    const float len = glm::length(home);
    const glm::vec3 hip = len > 1e-3f ? home / len : glm::vec3{-1.f, 0.f, 0.f};
    const float sight = h == st.holding && sightKnown(h) ? za::clamp(st.adsT, 0.f, 1.f) : 0.f;
    const glm::vec3 way = hip * (1.f - sight) - sightDir() * sight;
    const float wayLen = glm::length(way);
    return wayLen > 1e-3f ? way / wayLen : -sightDir();
}

// The tilted pose's turn in the world about the view's axes (`axes`: forward, right, up): `pitch` up about its right, then
// vr_flat_wall_tilt_yaw (muzzle inwards) and vr_flat_wall_tilt_roll (top inwards) times `share`, mirrored for the left hand
// (the off hand). The roll is about the untilted gun's way (the view's forward), before the pitch and the yaw.
[[nodiscard]] glm::mat3 tiltTurn(int h, const glm::mat3& axes, float pitch, float share)
{
    const float side = h == HAND_OFF ? -1.f : 1.f; // (inwards: to the left for the right hand)
    const float yaw = za::clamp(vr_flat_wall_tilt_yaw.value, -tiltYawMax, tiltYawMax) * share * side;
    const float roll = za::clamp(vr_flat_wall_tilt_roll.value, -tiltRollMax, tiltRollMax) * share * side;
    // (About the view's up, a positive angle turns the muzzle left; about its forward, the top to the right.)
    const glm::quat q = glm::angleAxis(glm::radians(yaw), axes[2]) * glm::angleAxis(glm::radians(pitch), axes[1]) *
                        glm::angleAxis(glm::radians(-roll), axes[0]);
    return glm::mat3_cast(q);
}

// What the way back can't clear (it stops with the arm folded, the stock at the chest, the rear sight vr_flat_ads_wall_keep
// from the eye), the drawn muzzle tilts up out of (vr_flat_wall_tilt): the least tilt about the view's right, round the
// hand, that leaves the weapon out of the walls where the way back leaves it, or all of the setting. The drawn gun only
// (flatplay::drawnOffset): the game's hand keeps the crosshair's turn, so the shots still go through the crosshair.
void tiltNeedFor(const hands::State& s, int h, const glm::mat3& view, const glm::vec3& wayWorld)
{
    st.tiltNeed[h] = st.tiltDepth[h] = st.tiltLeft[h] = 0.f;
    st.tiltWanted[h] = false;
    // (No further than the muzzle pointing straight up, less tiltShortOfUp: past it the gun would lean back over him.)
    const glm::mat3 hand = axesOf(s.rot[h]);
    const float offUp = glm::degrees(za::acos(za::clamp(hand[0].z, -1.f, 1.f))); // the muzzle's way off straight up
    // (Sighted and looking down past sightTiltDownFrom, none by sightTiltDownTo: tilted there the gun's rear and the hands went
    // into the chest, 3.6 deep at 55 down; the gun's end stays hidden in the wall as it was.)
    const float sight = h == st.holding && sightKnown(h) ? za::clamp(st.adsT, 0.f, 1.f) : 0.f;
    const float down = -view[0].z; // the sine of the look's pitch down
    // (And sighted, vr_flat_wall_tilt_sighted of it at most: tilted, the sights leave the ray.)
    const float downFade =
        1.f - geom::smooth01(za::clamp((down - sightTiltDownFrom) / (sightTiltDownTo - sightTiltDownFrom), 0.f, 1.f));
    const float sightFade = 1.f - sight * (1.f - za::clamp(vr_flat_wall_tilt_sighted.value, 0.f, 1.f) * downFade);
    const float most = za::min(za::clamp(vr_flat_wall_tilt.value, 0.f, wallTiltMax) * sightFade, offUp - tiltShortOfUp);
    if(most <= 0.f)
    {
        return;
    }
    const glm::vec3 at = s.pos[h] + wayWorld * st.backNeed[h];
    const glm::vec3 muzzle = muzzleNow(s, h) - s.pos[h];
    const float before = handpose::gunDepth(h, s.head, at, s.rot[h], muzzle);
    st.tiltDepth[h] = st.tiltLeft[h] = before;
    // It turns round a point between the hand and its shoulder (vr_flat_wall_tilt_pivot): at the shoulder the whole arm
    // swings the gun up and back, and the hand keeps its reach.
    const glm::vec3 pivot = (flatplay::shoulderOf(s, h) - at) * za::clamp(vr_flat_wall_tilt_pivot.value, 0.f, 1.f);
    st.tiltPivot[h] = glm::transpose(view) * pivot;
    if(before <= tiltFrom)
    {
        return;
    }
    st.tiltWanted[h] = true;
    st.tiltShare[h] = sightFade;
    const auto depthAt = [&](float deg) {
        const glm::mat3 r = tiltTurn(h, view, deg, sightFade);
        const glm::mat3 turned = r * hand;
        return handpose::gunDepth(h, s.head, at + pivot - r * pivot, hands::anglesFromVectors(turned[0], turned[2]), r * muzzle);
    };
    // Fixed (vr_flat_wall_tilt_fixed): all of it, however little would clear.
    if(vr_flat_wall_tilt_fixed.value != 0.f)
    {
        st.tiltNeed[h] = most;
        st.tiltLeft[h] = depthAt(most);
        return;
    }
    // The least: in steps up to the most, then halved between the last step in and the first out.
    float lo = 0.f, hi = most, left = before;
    bool out = false;
    for(float a = tiltStep;; a += tiltStep)
    {
        const float t = za::min(a, most);
        const float d = depthAt(t);
        left = d;
        if(d <= tiltFrom)
        {
            hi = t;
            out = true;
            break;
        }
        lo = t;
        if(t >= most)
        {
            break;
        }
    }
    for(int i = 0; out && i < tiltHalvings; i++)
    {
        const float mid = 0.5f * (lo + hi);
        const float d = depthAt(mid);
        if(d <= tiltFrom)
        {
            hi = mid;
            left = d;
        }
        else
        {
            lo = mid;
        }
    }
    st.tiltNeed[h] = out ? hi : most;
    st.tiltLeft[h] = left;
}

// The held weapons against the walls: how far each must go back (handpose::gunBack) at the place this pose made for it,
// and the hand moved back by what has been eased so far (frame). Not a hand throwing a punch: a blow is the game's, and a
// blade reaches what it reaches (vr_melee_world_pass_*, in a headset).
void pushBack(hands::State& s, const glm::vec3& aim)
{
    glm::vec3 org, dir;
    flatplay::crosshairRay(s, aim, org, dir);
    const glm::mat3 view = axesOf(aim);
    for(int h = 0; h < HAND_COUNT; h++)
    {
        st.backNeed[h] = 0.f;
        st.tiltNeed[h] = 0.f;
        st.tiltWanted[h] = false;
        if(flatplay::heldIn(h) < 0 || !s.muzzleValid[h] || !st.muzzleKnown[h] || vr_flat_gun_collide.value == 0.f)
        {
            continue;
        }
        const glm::vec3 hand = glm::transpose(view) * (s.pos[h] - org);
        const glm::vec3 shoulder = glm::transpose(view) * (flatplay::shoulderOf(s, h) - org);
        const glm::vec3 way = backWay(h, hand, shoulder);
        st.backWay[h] = way;

        // As far as the arm folds in hipfire, as far as the rear sight comes to the eye in the sight.
        const float sight = h == st.holding && sightKnown(h) ? za::clamp(st.adsT, 0.f, 1.f) : 0.f;
        const float hipMost = za::max(0.f, glm::distance(hand, shoulder) - za::clamp(vr_flat_wall_arm_keep.value, 0.f, wallArmMax));
        const float sightMost = za::max(0.f, st.relief - za::clamp(vr_flat_ads_wall_keep.value, reliefMin, reliefMax));
        // (Never so far its stock goes into the chest: crouched at a wall it did, 2.3 deep.)
        const float most = flatplay::stockBackMost(s, h, glm::normalize(view * way), hipMost + (sightMost - hipMost) * sight);
        if(!flatplay::punching(h))
        {
            st.backNeed[h] = handpose::gunBack(h, s.head, s.pos[h], s.rot[h], muzzleNow(s, h) - s.pos[h], view * way, most);
            tiltNeedFor(s, h, view, view * way);
        }
        const glm::vec3 moved = view * way * za::min(st.back[h], most);
        s.pos[h] += moved;
        if(st.adsT > 0.f && h == st.holding)
        {
            st.sightMuzzle += moved;
        }
        if(st.pinned && h == st.holding)
        {
            s.grip2H[h] += moved;
            s.pos[st.helping] += moved;
        }
    }
}

// How much of the field of view the ironsight is taking this frame: vr_flat_aim_fov's degrees times how far the weapon
// has travelled onto the crosshair (st.adsT, over vr_flat_aim_time) -- so the view closes over those same seconds and
// opens back off them, whatever the toggle says meanwhile, and nothing is taken outside flatscreen play or once the
// travel is back to nothing.
// vr_flat_aim_fov's degrees are counted at vr_flat_aim_fov_ref: the ironsight's field of view for a view of `base` is the same
// magnification of it (the tangents of the half-angles in the same ratio), so a change of fov keeps the zoom.
[[nodiscard]] float adsFovFor(float base)
{
    const float ref = za::clamp(vr_flat_aim_fov_ref.value, 60.f, 130.f);
    const float at = za::max(minFov, ref - za::clamp(vr_flat_aim_fov.value, 0.f, 75.f));
    const float zoom = std::tan(glm::radians(ref * 0.5f)) / std::tan(glm::radians(at * 0.5f));
    return za::max(minFov, 2.f * glm::degrees(std::atan(std::tan(glm::radians(za::clamp(base, 1.f, 170.f) * 0.5f)) / zoom)));
}

[[nodiscard]] float fovNarrowNow()
{
    const float done = za::clamp(st.adsT, 0.f, 1.f);
    if(!flatplay::active() || done <= 0.f)
    {
        return 0.f;
    }
    return za::max(0.f, st.baseFov - adsFovFor(st.baseFov)) * done;
}

// vr_flat_ads_relief is the relief at vr_flat_aim_fov_ref with no model fov: the sight picture's size on the screen goes as
// 1 / (relief * tan(half the field of view the gun is drawn at)), so the relief is scaled to keep that size for this view's
// sighted field of view, or vr_flat_model_fov's where that draws the player's own models.
[[nodiscard]] float reliefScale()
{
    const float ref = za::clamp(vr_flat_aim_fov_ref.value, 60.f, 130.f);
    const float refDrawn = za::max(minFov, ref - za::clamp(vr_flat_aim_fov.value, 0.f, 75.f));
    const float drawn = vr_flat_model_fov.value > 0.f ? za::clamp(vr_flat_model_fov.value, 10.f, 170.f) : adsFovFor(st.baseFov);
    return std::tan(glm::radians(refDrawn * 0.5f)) / std::tan(glm::radians(drawn * 0.5f));
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

// The right mouse button (+offhandattack, client::offhandButton) with one weapon held, whichever hand holds it
// (flatplay::routeFire; his words 2026-11-05: "right click holds ADS ... No need for dedicated ADS button"): vr_flat_aim_mode
// 0 the ironsight while it is held, 1 a press turns it on and the next off. Edges only, so `vr_flat_aim` said on the
// console stays until the button is used. A weapon in each hand, or none, and the button fires the left hand: an
// ironsight the button was holding goes off (the ironsight is refused there anyway: "nothing held one-handed").
void pollKey()
{
    const bool aims = flatplay::routeFire(false, false).rmbAims;
    const bool down = aims && key_dest == key_game && client::offhandButton();
    if(!aims)
    {
        if(st.keyWas && vr_flat_aim_mode.value == 0.f)
        {
            setAds(false, "right mouse button (no lone weapon)");
        }
    }
    else if(vr_flat_aim_mode.value != 0.f)
    {
        if(down && !st.keyWas)
        {
            setAds(!st.ads, "right mouse button (toggle)");
        }
    }
    else if(down != st.keyWas)
    {
        setAds(down, "right mouse button (held)");
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
    // A gun held in two hands comes in to where the other arm reaches its grip: across the chest, and nearer.
    const glm::vec3 home = flatplay::shoulderOf(s, st.helping);
    const glm::vec3 far = s.grip2H[st.holding] + (s.pos[st.holding] - s.placedFrom[st.holding]) - home;
    const float past = glm::length(far) - flatplay::armReach() * gripKeep;
    if(past > 0.f)
    {
        s.pos[st.holding] -= far * (past / glm::length(far) * st.gripT);
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

// The held weapon brought to the sight's place, over the travel: the hand where the solve has it (sighted), in the frame
// of the crosshair's ray, and the muzzle the shots go from where that gun's muzzle is.
void centreOnSight(hands::State& s, const glm::vec3& aim)
{
    st.shift = glm::vec3{0.f};
    st.offBefore = st.offAfter = st.offAsked = st.sightAngle = 0.f;
    if(st.holding < 0)
    {
        return;
    }

    // Measured whatever the ironsight is doing: where the muzzle this frame's pose leaves the crosshair's ray stands,
    // printed as the pair before -> after.
    glm::vec3 org, dir;
    flatplay::crosshairRay(s, aim, org, dir);
    const glm::vec3 m = muzzleNow(s, st.holding);
    offTheRay(org, dir, m, st.offBefore, st.sightAngle);
    st.offAfter = st.offBefore;
    st.sightMuzzle = m;
    if(!sightKnown(st.holding))
    {
        st.relief = 0.f;
        return;
    }
    const glm::mat3 view = axesOf(aim);
    const glm::vec3 helper = st.helping >= 0 ? glm::transpose(view) * (flatplay::shoulderOf(s, st.helping) - org) : glm::vec3{0.f};
    st.relief = reliefFor(st.holding, glm::transpose(view) * (flatplay::shoulderOf(s, st.holding) - org), helper, flatplay::armReach());
    if(st.adsT <= 0.f)
    {
        return;
    }

    const Sighted at = sighted(st.holding, st.relief);
    const glm::vec3 muzzle = org + view * (at.hand + at.turn * st.muzzleLocal[st.holding]);
    st.shift = (org + view * at.hand - s.pos[st.holding]) * st.adsT;
    s.pos[st.holding] += st.shift;
    if(st.pinned)
    {
        // The grip the other hand is holding travels with the hand it holds on to: their distance apart is unchanged.
        s.grip2H[st.holding] += st.shift;
        s.pos[st.helping] += st.shift;
    }
    st.sightMuzzle = m + (muzzle - m) * st.adsT;
    float deg = 0.f;
    offTheRay(org, dir, muzzle, st.offAsked, deg);
    st.offAsked *= st.adsT;
    offTheRay(org, dir, st.sightMuzzle, st.offAfter, st.sightAngle);
}

// The share of its arm the free hand `helper` uses on the world point `at`: from the shoulder the body poses that arm on.
[[nodiscard]] float armShare(const hands::State& s, int helper, const glm::vec3& at)
{
    // (Hands on the body: from the rest shoulder, which the free hand's own place doesn't move. The posed one swings to its hand, so
    // the need read lower with the hand on the hotspot than off it, and the hand snapped between the two.)
    avatar::Skeleton k;
    const glm::vec3 root = !flatplay::onBody() && avatar::skeleton(k) ? k.shoulder[helper] : flatplay::shoulderOf(s, helper);
    return glm::distance(root, at) / za::max(flatplay::armReach(), 1.f);
}

// The stance. A long gun whose hotspot the view found (view::twoHandSpot: the weapon's own, from Weapon Offsets) turns the
// torso, and with no real grip to take (carry) the free hand is drawn on that hotspot, with the pose a helping hand has there:
// the view does that with this share (lookShare), and nothing here decides a place or a turn for it. Its game hand stays in
// the hanging pose, so no hold, weight, recoil or punch knows of it, and nothing moves the gun for it: an arm that can't reach
// goes back to hanging, eased.
void stanceAndLook(const hands::State& s, const glm::vec3& aim, const glm::vec3& reach, bool carry, bool freshFrame)
{
    if(st.lookHand >= 0 && flatplay::heldIn(st.lookHand) >= 0)
    {
        st.lookT = 0.f; // it took a weapon of its own: that pose is the shots'
        st.lookHand = -1;
    }
    // A gun held with the other hand free and a hotspot for it: a long gun's stance, or a pistol's (both hands in front of him). Not
    // dual wielding (the other hand holds), not a melee weapon (no sight line: aimsAtPoint), not a gun with no hotspot: one hand.
    const bool twoHandGun = st.holding >= 0 && st.helping >= 0 && aimsAtPoint(s, st.holding) && emptyHand(st.helping) &&
                            st.spot[st.holding] >= 0;
    const bool pistol = twoHandGun && glm::length(st.muzzleLocal[st.holding]) < longGunMuzzle;
    const bool longGun = twoHandGun && (!pistol || vr_flat_stance_pistol.value != 0.f);
    st.armHip = st.armSight = 0.f;
    float slide = 0.f;
    float slideHip = 0.f;
    // Where the arm can't take the hotspot at `at` (world; `along`: the gun's forward), how far down the fore-end it takes it within
    // `keep` of its reach: |v - f d| = keep, the nearer root; never nearer the holding hand than slideKeepAhead.
    const auto slideFor = [&](const glm::vec3& at, const glm::vec3& along, float keepShare) {
        const glm::vec3 v = at - flatplay::shoulderOf(s, st.helping);
        const float keep = keepShare * za::max(flatplay::armReach(), 1.f);
        const float vf = glm::dot(v, along);
        const float disc = vf * vf - glm::dot(v, v) + keep * keep;
        return za::clamp(disc > 0.f ? vf - za::sqrt(disc) : vf, 0.f, za::max(0.f, st.spotLocal[st.holding].x - slideKeepAhead));
    };
    if(longGun)
    {
        const glm::vec3& local = st.spotLocal[st.holding];
        const glm::mat3 held = axesOf(s.rot[st.holding]);
        const glm::vec3 hipAt = s.pos[st.holding] - st.shift + held * local;
        st.armHip = armShare(s, st.helping, hipAt);
        // (Hipfire too: crouched and looking up, the pose can't bring the hotspot itself into reach without the stock in his chest.)
        if(flatplay::onBody() && st.armHip > spotKeepHip)
        {
            slideHip = slideFor(hipAt, held[0], spotKeepHip);
            st.armHip = armShare(s, st.helping, hipAt - held[0] * slideHip);
        }
        if(sightKnown(st.holding))
        {
            glm::vec3 org, dir;
            flatplay::crosshairRay(s, aim, org, dir);
            const Sighted at = sighted(st.holding, st.relief);
            const glm::mat3 view = axesOf(aim);
            const glm::vec3 spotAt = org + view * (at.hand + at.turn * local);
            st.armSight = armShare(s, st.helping, spotAt);
            // Sighted, where the relief can't come nearer (reliefFor's floor): the free hand down the fore-end, along the gun, to where
            // its arm takes it within spotKeepAds. |v - f d| = keep, the nearer root; never nearer the holding hand than slideKeepAhead.
            if(flatplay::onBody() && st.armSight > spotKeepAds)
            {
                const glm::vec3 f = view * (at.turn * glm::vec3{1.f, 0.f, 0.f});
                slide = slideFor(spotAt, f, spotKeepAds);
                st.armSight = armShare(s, st.helping, spotAt - f * slide);
            }
        }
    }
    if(freshFrame)
    {
        const float sighted = za::clamp(st.adsT, 0.f, 1.f);
        st.lookSlide = slideHip + (slide - slideHip) * sighted;
    }
    if(!freshFrame)
    {
        return;
    }
    st.lookView = axesOf(aim);
    st.lookPitch = aim.x;
    const bool on = vr_flat_stance.value != 0.f;
    st.stanceWanted = on && longGun;
    st.stanceGun = st.stanceWanted && !carry;
    if(st.stanceWanted)
    {
        st.stanceHand = st.holding;
        st.stancePistol = pistol;
        st.stanceSign = st.helping == HAND_OFF ? -1.f : 1.f; // the yaw grows to his left: the left shoulder comes forward by a turn to the right
    }
    // What decides it is what the arm needs where the gun stands, or is going: the sight's place while it is asked for.
    const float need = st.adsWanted ? st.armSight : st.armHip;
    st.lookFar = need > lookKeep || (st.lookFar && need > lookKeep - lookBack);
    // Hands on the body: the hand goes off the hotspot as the need grows past lookSoftFrom, all of it by lookKeep, and comes back the
    // same way: a look that takes the hotspot out of reach takes the hand off it over some degrees, not in a frame.
    // (Sighted, the gun stands where the arm reaches it nearly straight, along the fore-end: reliefFor's spotKeepAds.)
    const float softFrom = st.adsWanted ? lookSoftFromAds : lookSoftFrom;
    const float softTo = st.adsWanted ? lookKeepAds : lookKeep;
    const float soft = za::clamp((need - softFrom) / (softTo - softFrom), 0.f, 1.f);
    st.lookTarget = flatplay::onBody() ? 1.f - soft * soft * (3.f - 2.f * soft) : 1.f;
    if(flatplay::onBody())
    {
        st.lookFar = soft >= 1.f;
    }
    const bool reaching = st.helping == HAND_OFF && glm::length(reach) > 0.f; // E: a force grab reaches out with it
    st.lookFast = longGun && (reaching || flatplay::punching(st.helping));
    st.lookWanted = st.stanceWanted && !carry && !st.lookFar && !st.lookFast;
    if(st.lookWanted)
    {
        st.lookHand = st.helping;
    }
}

// vr_flat_recoil_probe: the most a shot's recoil moved the drawn weapons since the last print, before and after the
// ironsight's share (keepRecoil), and the share the last frame kept. The test fires hipfire, then sighted, and compares.
struct RecoilProbe
{
    float rawPos{0.f}, rawAngles{0.f}, drawnPos{0.f}, drawnAngles{0.f}, share{1.f};
};
RecoilProbe recoilProbe;

void recoilProbe_f()
{
    const RecoilProbe& p = recoilProbe;
    Con_Printf("flatrecoil: raw %.4f u %.4f deg drawn %.4f u %.4f deg share %.3f (vr_flat_ads_recoil %g, ads %.3f)\n", p.rawPos,
        p.rawAngles, p.drawnPos, p.drawnAngles, p.share, vr_flat_ads_recoil.value, za::clamp(st.adsT, 0.f, 1.f));
    recoilProbe = RecoilProbe{};
}

} // namespace

static void check_f();

void init()
{
    Cmd_AddCommand("vr_flat_recoil_probe", recoilProbe_f);
    Cmd_AddCommand("vr_flat_aim", aim_f);
    Cmd_AddCommand("vr_flat_sight_check", check_f);
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
        st.lookT = st.stanceT = 0.f;
        st.lookWanted = st.lookFar = st.lookFast = st.stanceWanted = st.stanceGun = st.drawnKnown = false;
        st.lookHand = st.stanceHand = -1;
        st.spot[0] = st.spot[1] = -1;
        st.edgeFrame = -1;
        for(int h = 0; h < HAND_COUNT; h++)
        {
            st.back[h] = st.backNeed[h] = 0.f;
            st.tilt[h] = st.tiltNeed[h] = st.tiltOn[h] = st.tiltPitch[h] = st.tiltShare[h] = st.tiltAngle[h] = 0.f;
            st.tiltWanted[h] = false;
            st.sightModel[h] = nullptr;
        }
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
    {
        const float to = st.lookWanted ? st.lookTarget : 0.f;
        const float step = ease(st.lookFast ? gripEase : lookEase, dt);
        st.lookT = za::clamp(st.lookT + za::clamp(to - st.lookT, -step, step), 0.f, 1.f);
    }
    st.stanceT = za::clamp(st.stanceT + (st.stanceWanted ? 1.f : -1.f) * ease(stanceEase, dt), 0.f, 1.f);
    // The ironsight's travel is spent at a steady rate, so it is over in the cvar's own seconds; 0 asks for the whole
    // of it this frame. The weapon's shift onto the crosshair (apply) and the field of view go by the same fraction.
    const float aim = aimSeconds();
    st.adsT = aim <= 0.f ? (st.adsWanted ? 1.f : 0.f)
                         : za::clamp(st.adsT + (st.adsWanted ? 1.f : -1.f) * dt / aim, 0.f, 1.f);
    // A wall met: back at once, nearly; cleared: out again over a moment, whichever way the view or the player moved.
    for(int h = 0; h < HAND_COUNT; h++)
    {
        st.back[h] += (st.backNeed[h] - st.back[h]) * ease(st.backNeed[h] > st.back[h] ? backIn : backOut, dt);
        // The tilted pose eases in, and back out off the wall, over vr_flat_wall_tilt_time (0: at once). In it, its pitch
        // follows the least that clears at tiltPitchEase over that time (it went straight after it before, 4.7 degrees of
        // gun per degree of look going down into a wall); easing out, it keeps the pitch it had.
        const float tiltTime = za::clamp(vr_flat_wall_tilt_time.value, 0.f, tiltTimeMax);
        if(st.tiltWanted[h])
        {
            st.tiltPitch[h] = st.tiltOn[h] <= 0.f || tiltTime <= 0.f
                                  ? st.tiltNeed[h]
                                  : st.tiltPitch[h] + (st.tiltNeed[h] - st.tiltPitch[h]) * ease(tiltPitchEase / tiltTime, dt);
        }
        const float onStep = tiltTime > 0.f ? dt / tiltTime : 1.f;
        st.tiltOn[h] = za::clamp(st.tiltOn[h] + (st.tiltWanted[h] ? onStep : -onStep), 0.f, 1.f);
        const float on = geom::smooth01(st.tiltOn[h]);
        st.tilt[h] = on * st.tiltPitch[h];
        const glm::mat3 r = tiltTurn(h, glm::mat3{1.f}, st.tilt[h], on * st.tiltShare[h]);
        st.tiltAngle[h] = glm::degrees(za::acos(za::clamp(0.5f * (r[0][0] + r[1][1] + r[2][2] - 1.f), -1.f, 1.f)));
    }

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
    // A punch of the free hand takes it off the grip, as fast as it came (gripEase: done before the throw), and it goes back
    // when the punch is over; twohand lets go of the hold for as long (flatplay::punching there).
    st.gripWanted = carryToGrip && !flatplay::punching(st.helping);
    pinToGrip(s, reach, carryToGrip);
    centreOnSight(s, aim);
    pushBack(s, aim);

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

    // The muzzle may have moved through the frame's other poses' turns: this is the pose's, handed on to the shot.
    aimAtPoint(s, aim);
    const glm::vec3 shotPoint = crosshairPoint(s, aim);
    for(int h = 0; h < HAND_COUNT; h++)
    {
        keepMuzzle(s, h, aim, shotPoint);
    }

    // Last, on the gun as this pose leaves it: the stance, and the free hand on the gun's hotspot, measure and move nothing above.
    stanceAndLook(s, aim, reach, carryToGrip, freshFrame);
}

void notePlaced(const hands::State& s)
{
    if(!flatplay::active())
    {
        return;
    }
    for(int h = 0; h < HAND_COUNT; h++)
    {
        st.muzzleKnown[h] = s.muzzleValid[h];
        if(s.muzzleValid[h])
        {
            glm::vec3 fwd, right, up;
            hands::angleVectors(s.visualRot[h], fwd, right, up);
            const glm::vec3 d = s.muzzle[h] - s.pos[h];
            st.muzzleLocal[h] = {glm::dot(d, fwd), glm::dot(d, right), glm::dot(d, up)};
        }
    }
}

void noteDrawn(const hands::State& s)
{
    if(!flatplay::active())
    {
        return;
    }
    for(int h = 0; h < HAND_COUNT; h++)
    {
        glm::vec3 at{0.f};
        st.spot[h] = view::twoHandSpot(s, h, at);
        st.spotLocal[h] = glm::transpose(axesOf(s.visualRot[h])) * (at - s.pos[h]);
        view::WeaponFrame wf;
        st.sightModel[h] = nullptr;
        st.stockModel[h] = nullptr;
        if(!view::weaponFrame(s, h, wf) || !wf.model)
        {
            continue;
        }
        {
            const float cy = 0.5f * (wf.model->mins[1] + wf.model->maxs[1]), cz = 0.5f * (wf.model->mins[2] + wf.model->maxs[2]);
            const glm::vec3 rear{wf.modelToWorld * glm::vec4{wf.model->mins[0], cy, cz, 1.f}};
            // (Without the drawn tilt at a wall: the pose's own gun, which the way back measures from.)
            const glm::mat3 viewAxes = axesOf(geom::vec(cl.viewangles));
            const glm::mat3 untilt = glm::transpose(wallTurn(h, viewAxes));
            st.stock[h] = glm::transpose(viewAxes) * (untilt * (rear - s.pos[h]));
            st.stockModel[h] = wf.model;
        }
        const sightalign::SightLine line = sightalign::sightLine(wf.model);
        if(!line.valid)
        {
            continue;
        }
        const glm::mat3 toHand = glm::transpose(axesOf(s.visualRot[h]));
        st.sightRear[h] = toHand * (glm::vec3{wf.modelToWorld * glm::vec4{line.rear, 1.f}} - s.pos[h]);
        st.sightFront[h] = toHand * (glm::vec3{wf.modelToWorld * glm::vec4{line.front, 1.f}} - s.pos[h]);
        st.sightModel[h] = wf.model;
    }
    // The free hand as it was last drawn, against the gun: the status's measure.
    glm::vec3 drawnPos, drawnRot;
    st.drawnKnown = st.holding >= 0 && st.helping >= 0 && view::drawnHandPose(st.helping, drawnPos, drawnRot);
    if(st.drawnKnown)
    {
        const glm::mat3 toHand = glm::transpose(axesOf(s.visualRot[st.holding]));
        const glm::mat3 turn = toHand * axesOf(drawnRot);
        st.drawnLocal = toHand * (drawnPos - s.pos[st.holding]);
        st.drawnTurn = hands::anglesFromVectors(turn[0], turn[2]);
        // Where the whole look draws it against the hotspot: the hotspot's own hand offsets (Weapon Offsets' hsN_v*, its style; a
        // pistol's cup has the hand 4.9 off the point), learned while it is fully on. The sweep measures from there.
        const qmodel_t* m = weapons::heldModel(st.holding);
        if(st.spot[st.holding] >= 0 && lookShare(st.helping) >= 0.999f && lookSlide(st.helping) <= 0.f)
        {
            st.spotHandOff = st.drawnLocal - st.spotLocal[st.holding];
            st.spotHandModel = m;
        }
    }
}

float drawnSight(int h, glm::vec3& hand, glm::mat3& turn)
{
    if(!flatplay::active() || h != st.holding || st.adsT <= 0.f || !sightKnown(h))
    {
        return 0.f;
    }
    const Sighted at = sighted(h, st.relief);
    turn = at.turn;
    hand = at.hand + st.backWay[h] * st.back[h];
    return za::clamp(st.adsT, 0.f, 1.f);
}

float crosshairFade()
{
    return flatplay::active() ? 1.f - geom::smooth01(za::clamp(st.adsT, 0.f, 1.f)) : 1.f;
}

void keepRecoil(glm::vec3& pos, glm::vec3& angles)
{
    if(!flatplay::active())
    {
        return;
    }
    const float share =
        glm::mix(1.f, za::clamp(vr_flat_ads_recoil.value, 0.f, 1.f), geom::smooth01(za::clamp(st.adsT, 0.f, 1.f)));
    RecoilProbe& p = recoilProbe;
    p.rawPos = glm::max(p.rawPos, glm::length(pos));
    p.rawAngles = glm::max(p.rawAngles, glm::length(angles));
    pos *= share;
    angles *= share;
    p.drawnPos = glm::max(p.drawnPos, glm::length(pos));
    p.drawnAngles = glm::max(p.drawnAngles, glm::length(angles));
    p.share = share;
}

glm::vec3 rayDir(const glm::vec3& aim)
{
    return axesOf(aim) * sightDir();
}

glm::vec2 screenPlace()
{
    return screenShift();
}

float wallTilt(int h, glm::vec3* pivot)
{
    const bool on = flatplay::active() && h >= 0 && h < HAND_COUNT;
    if(pivot)
    {
        *pivot = on ? st.tiltPivot[h] : glm::vec3{0.f};
    }
    return on ? st.tiltAngle[h] : 0.f;
}

glm::mat3 wallTurn(int h, const glm::mat3& axes)
{
    if(!flatplay::active() || h < 0 || h >= HAND_COUNT || st.tiltAngle[h] == 0.f)
    {
        return glm::mat3{1.f};
    }
    return tiltTurn(h, axes, st.tilt[h], geom::smooth01(st.tiltOn[h]) * st.tiltShare[h]);
}

bool wallBlocksFire(int h)
{
    return vr_flat_wall_block_fire.value != 0.f && wallTilt(h) > tiltBlocksFire;
}

float drawnShare(int h)
{
    return flatplay::active() && h == st.holding && sightKnown(h) ? za::clamp(st.adsT, 0.f, 1.f) : 0.f;
}

float lookShare(int h)
{
    return flatplay::active() && h == st.lookHand ? geom::smooth01(st.lookT) : 0.f;
}

float lookSlide(int h)
{
    return flatplay::active() && h == st.lookHand ? st.lookSlide : 0.f;
}

float elbowPole(int h, glm::vec3& way)
{
    if(!flatplay::active() || !flatplay::onBody())
    {
        return 0.f;
    }
    // The gun's own hand, its gun pushed back by a wall into the turned chest: its elbow out at its side and a little down, so the
    // forearm comes across in front of the chest, not into it (vr_flat_wall_elbow).
    if(h == st.stanceHand && h != st.lookHand)
    {
        const float wall = za::clamp(vr_flat_wall_elbow.value, 0.f, 1.f) * geom::smooth01(za::clamp(st.back[h] / wallElbowFull, 0.f, 1.f));
        if(wall <= 0.f)
        {
            return 0.f;
        }
        way = glm::normalize(st.lookView[1] * (h == HAND_OFF ? -1.f : 1.f) - st.lookView[2] * wallElbowDown);
        return wall;
    }
    if(h != st.lookHand)
    {
        return 0.f;
    }
    // (Quake's pitch: negative is up.)
    const float up = geom::smooth01(za::clamp((-st.lookPitch - elbowUpFrom) / (elbowUpFull - elbowUpFrom), 0.f, 1.f));
    const float share = za::clamp(vr_flat_up_elbow.value, 0.f, 1.f) * up * lookShare(h);
    if(share <= 0.f)
    {
        return 0.f;
    }
    // Down under the gun (the view's down: the gun is on the sight line) and out at the arm's own side (the off hand: the left).
    const float out = za::clamp(vr_flat_up_elbow_out.value, 0.f, 2.f);
    const glm::vec3 side = st.lookView[1] * (h == HAND_OFF ? -1.f : 1.f);
    way = glm::normalize(-st.lookView[2] + side * out);
    return share;
}

float stanceYaw()
{
    const float turn = za::clamp(st.stancePistol ? vr_flat_pistol_turn.value : vr_flat_stance_turn.value, 0.f, 45.f);
    const float sighted = za::clamp(vr_flat_ads_turn.value, 0.f, 30.f) * geom::smooth01(za::clamp(st.adsT, 0.f, 1.f));
    return st.stanceSign * (turn + sighted) * geom::smooth01(st.stanceT);
}

bool stancePistol()
{
    return st.stancePistol;
}

float stanceLean()
{
    if(!flatplay::active() || !st.stanceGun)
    {
        return 0.f;
    }
    return za::clamp(vr_flat_ads_lean.value, 0.f, 30.f) * geom::smooth01(za::clamp(st.adsT, 0.f, 1.f)) * geom::smooth01(st.stanceT);
}

float stanceSpot(int h, glm::vec3& local)
{
    if(h != st.stanceHand || !st.stanceGun || st.spot[h] < 0)
    {
        return 0.f;
    }
    local = st.spotLocal[h];
    return stanceShare(h);
}

bool stockOff(int h, glm::vec3& local)
{
    if(h < 0 || h >= HAND_COUNT || !st.stockModel[h] || st.stockModel[h] != weapons::heldModel(h))
    {
        return false;
    }
    local = st.stock[h];
    return true;
}

float stanceShare(int h)
{
    return h == st.stanceHand && st.stanceGun ? geom::smooth01(st.stanceT) : 0.f;
}

float stanceLift(int h)
{
    return flatplay::active() && h == st.stanceHand && st.stanceWanted ? geom::smooth01(st.stanceT) : 0.f;
}

bool adsHelper(int h)
{
    return h == st.helping && st.gripWanted;
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
    st.baseFov = basefov;
    const float narrow = fovNarrowNow();
    return narrow > 0.f ? za::max(minFov, basefov - narrow) : basefov;
}

void statusLine()
{
    Con_Printf("  ironsight: %s (vr_flat_aim, or the right mouse button with one weapon held, vr_flat_aim_mode %g; vr_flat_aim_fov %g off the field of view, "
               "taken over %g s of travel (vr_flat_aim_time); %d toggles this session; solved from the weapon's sight line: "
               "the rear sight vr_flat_ads_relief %g from the eye, the picture at vr_flat_ads_screen_x %g _y %g percent)%s\n",
        st.ads ? "asked for" : "off", vr_flat_aim_mode.value, vr_flat_aim_fov.value, vr_flat_aim_time.value, st.toggles,
        vr_flat_ads_relief.value, vr_flat_ads_screen_x.value, vr_flat_ads_screen_y.value,
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
        if(st.spot[st.holding] >= 0)
        {
            // The stance, and the free hand on the weapon's own hotspot (stanceAndLook: no grip).
            const glm::vec3& l = st.spotLocal[st.holding];
            const glm::vec3 d = st.drawnKnown ? st.drawnLocal - l : glm::vec3{0.f};
            Con_Printf("  stance: %s, torso %.1f deg, free hand %s, share %.2f (gun %d far %d fast %d grip wanted %d, %.2f)\n", st.stanceWanted ? "on" : "off", stanceYaw(),
                st.lookWanted ? "going on it" : twohand::helping(st.helping) ? "holding its grip" : "hanging", lookShare(st.helping),
                st.stanceGun ? 1 : 0, st.lookFar ? 1 : 0, st.lookFast ? 1 : 0, st.gripWanted ? 1 : 0, st.gripT);
            Con_Printf("  stance arm: hipfire %.2f sight %.2f of %.1f, keeps %.2f%s\n", st.armHip, st.armSight, flatplay::armReach(),
                gripKeep, st.lookFar ? ", out of reach" : "");
            Con_Printf("  stance spot: hs%d fwd %.2f right %.2f up %.2f relief %.1f\n", st.spot[st.holding] + 1, l.x, l.y, l.z, st.relief);
            Con_Printf("  stance hand: off %.2f fwd %.2f right %.2f up %.2f turned %.1f %.1f %.1f\n", glm::length(d), st.drawnLocal.x,
                st.drawnLocal.y, st.drawnLocal.z, st.drawnTurn.x, st.drawnTurn.y, st.drawnTurn.z);
        }
        else
        {
            Con_Printf("  stance: no two-handed hotspot on this gun\n");
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
    const glm::vec3 viewAim = geom::vec(cl.viewangles);
    flatplay::crosshairRay(s, viewAim, org, dir);
    float units = 0.f;
    float degrees = 0.f;
    offTheRay(org, dir, muzzleNow(s, h), units, degrees);

    // Each gun's aim, the shot's way: what the crosshair is on, and what the line the shot leaves on stops at (from behind
    // the muzzle, as QC's VR_ShotBackPoint: what the muzzle stands in is hit too).
    const glm::vec3 point = crosshairPoint(s, viewAim);
    for(int g = 0; g < HAND_COUNT; g++)
    {
        if(!aimsAtPoint(s, g))
        {
            continue;
        }
        const glm::vec3 m = muzzleNow(s, g);
        const glm::vec3 shot = hands::forward(weapons::shotAngles(s.rot[g], flatplay::heldIn(g), g == HAND_OFF));
        const glm::vec3 from = shotFrom(org, dir, point, m, st.aimBlocked[g]);
        const glm::vec3 stop = lineStop(from - shot * glm::distance(m, s.pos[g]), from + shot * aimFar);
        const glm::vec3 toPoint = aimWay(dir, point, from, st.aimBlocked[g]);
        Con_Printf("flataim: %s off its point %.4f deg away %.1f apart %.2f at %.2f %.2f %.2f\n", g == HAND_MAIN ? "main" : "off",
            degreesApart(shot, toPoint), glm::distance(org, point),
            glm::distance(stop, point), point.x, point.y, point.z);
    }

    if(st.holding < 0)
    {
        Con_Printf("flatsight: t %.4f ads %s fov %.2f x %.2f hand main muzzle off the crosshair %.2f units (%.2f deg): "
                   "nothing held one-handed\n",
            vr_gametime, st.ads ? "asked for" : "off", r_refdef.fov_x, r_refdef.fov_y, units, degrees);
        return;
    }
    // (The wall's turn as drawn, in the view's frame: the turned muzzle's way up and to the right, its top's lean right
    // about the muzzle's own way: towards the right that stays level with the view.)
    const glm::mat3 viewAxes = axesOf(viewAim);
    const glm::mat3 turn = wallTurn(st.holding, viewAxes);
    const glm::vec3 turnedFwd = turn * viewAxes[0];
    const float turnUp = glm::dot(turnedFwd, viewAxes[2]);
    const float turnSide = glm::dot(turnedFwd, viewAxes[1]);
    const glm::vec3 levelRight = glm::cross(turnedFwd, viewAxes[2]);
    const float levelLen = glm::length(levelRight);
    const float turnCant = levelLen > 1e-3f ? glm::dot(turn * viewAxes[2], levelRight / levelLen) : 0.f;
    Con_Printf("flatsight: t %.4f ads %s t %.2f fov %.2f x %.2f hand %s grip %s held %d helpKind %d transition %.2f "
               "muzzle off the crosshair %.2f -> %.2f units (%.2f deg) hand moved %.2f fov take %g relief %.2f back %.2f asked %.2f "
               "tilt %.1f need %.1f depth %.2f left %.2f\nflatwallturn: t %.4f on %.3f angle %.1f up %.3f side %.3f cant %.3f block %d\n",
        vr_gametime, adsApplies() ? "on" : (st.ads ? "no sights" : "off"), st.adsT, r_refdef.fov_x, r_refdef.fov_y,
        st.holding == HAND_MAIN ? "main" : "off", st.pinned ? "hotspot" : "none", twohand::helping(st.helping) ? 1 : 0,
        twohand::helpKind(st.helping), twohand::transition(st.holding), st.offBefore, units, degrees,
        glm::length(st.shift), vr_flat_aim_fov.value, st.relief, st.back[st.holding], st.offAsked, st.tilt[st.holding],
        st.tiltNeed[st.holding], st.tiltDepth[st.holding], st.tiltLeft[st.holding], vr_gametime, geom::smooth01(st.tiltOn[st.holding]),
        st.tiltAngle[st.holding], turnUp, turnSide, turnCant, wallBlocksFire(st.holding) ? 1 : 0);
}

// vr_flat_sight_check [main|off]: the sight line as the weapon was last drawn, against the view it was drawn with: how far
// each sight stands off the ray through where the crosshair is, far along it too. Measured off the entity, not the solve.
static void check_f()
{
    const int h = Cmd_Argc() >= 2 && !q_strcasecmp(Cmd_Argv(1), "off") ? HAND_OFF : HAND_MAIN;
    const hands::State& s = hands::current();
    view::WeaponFrame wf;
    if(!flatplay::active() || !view::weaponFrame(s, h, wf))
    {
        Con_Printf("flatsightcheck: no drawn weapon (flatscreen play only)\n");
        return;
    }
    const sightalign::SightLine line = sightalign::sightLine(wf.model);
    {
        // Where the weapon is drawn in the camera's frame (forward, right, up; and its pitch on the screen, degrees, up
        // positive): the hand, the muzzle, the sights. What a gun drawn lower or higher than the others is read from.
        const glm::vec3 c = geom::vec(r_refdef.vieworg);
        const glm::mat3 ax = axesOf(geom::vec(r_refdef.viewangles));
        const auto inView = [&](const glm::vec3& p) { return glm::transpose(ax) * (p - c); };
        const auto upDeg = [](const glm::vec3& v) { return glm::degrees(std::atan2(v.z, glm::max(v.x, 0.01f))); };
        const glm::vec3 hand = inView(s.pos[h]);
        const glm::vec3 muz = s.muzzleValid[h] ? inView(s.muzzle[h]) : hand;
        const glm::vec3 rear = line.valid ? inView(glm::vec3{wf.modelToWorld * glm::vec4{line.rear, 1.f}}) : hand;
        const glm::vec3 front = line.valid ? inView(glm::vec3{wf.modelToWorld * glm::vec4{line.front, 1.f}}) : muz;
        Con_Printf("flatgunview: %s t %.2f hand %.2f %.2f %.2f (%.2f deg) muzzle %.2f %.2f %.2f (%.2f deg) rear %.2f %.2f %.2f "
                   "(%.2f deg) front %.2f %.2f %.2f (%.2f deg)\n",
            wf.model->name, st.adsT, hand.x, hand.y, hand.z, upDeg(hand), muz.x, muz.y, muz.z, upDeg(muz), rear.x, rear.y, rear.z,
            upDeg(rear), front.x, front.y, front.z, upDeg(front));
        Con_Printf("flatcrosshair: %.3f\n", crosshairFade());
    }
    if(!line.valid)
    {
        Con_Printf("flatsightcheck: %s no sight line\n", wf.model->name);
        return;
    }
    const glm::vec3 cam = geom::vec(r_refdef.vieworg);
    const glm::vec3 dir = axesOf(geom::vec(r_refdef.viewangles)) * sightDir();
    const glm::vec3 rear{wf.modelToWorld * glm::vec4{line.rear, 1.f}};
    const glm::vec3 front{wf.modelToWorld * glm::vec4{line.front, 1.f}};
    const glm::vec3 lineDir = glm::normalize(front - rear);
    float offRear, offFront, deg;
    offTheRay(cam, dir, rear, offRear, deg);
    offTheRay(cam, dir, front, offFront, deg);
    // Far along: where the sight line stands 4096 past its rear sight, against the same ray, as an angle.
    float offFar, degFar;
    offTheRay(cam, dir, rear + lineDir * aimFar, offFar, degFar);
    const glm::vec3 toShoulder = flatplay::shoulderOf(s, h) - s.pos[h];
    Con_Printf("flatsightcheck: %s t %.2f rear off %.4f front off %.4f far off %.3f (%.4f deg)\n", wf.model->name, st.adsT, offRear,
        offFront, offFar, degFar);
    Con_Printf("flatsightpose: relief %.2f length %.2f rear in hand %.2f %.2f %.2f hand to shoulder %.1f arm %.1f "
               "fov %.2f of %.2f relief scale %.4f\n",
        glm::dot(rear - cam, dir), glm::distance(rear, front), st.sightRear[h].x, st.sightRear[h].y, st.sightRear[h].z,
        glm::length(toShoulder), flatplay::armReach(), r_refdef.basefov, st.baseFov, reliefScale());
}

SweepNumbers sweepNumbers(const hands::State& s)
{
    SweepNumbers n;
    n.holding = st.holding;
    n.relief = st.relief;
    n.adsT = st.adsT;
    if(st.holding >= 0 && st.helping >= 0 && st.spot[st.holding] >= 0)
    {
        n.hasSpot = true;
        n.lookShare = lookShare(st.helping);
        n.lookFar = st.lookFar;
        n.armHip = st.armHip;
        n.armSight = st.armSight;
        const glm::vec3 own = st.spotHandModel == weapons::heldModel(st.holding) ? st.spotHandOff : glm::vec3{0.f};
        // (Off the place it holds: the hotspot, or down the fore-end from it by the slide.)
        const glm::vec3 slid{lookSlide(st.helping), 0.f, 0.f};
        n.spotOff = st.drawnKnown ? glm::distance(st.drawnLocal, st.spotLocal[st.holding] + own - slid) : -1.f;
        n.spotSlide = lookSlide(st.helping);
    }
    const int g = st.holding >= 0 ? st.holding : HAND_MAIN;
    if(aimsAtPoint(s, g))
    {
        const glm::vec3 viewAim = geom::vec(cl.viewangles);
        glm::vec3 org, dir;
        flatplay::crosshairRay(s, viewAim, org, dir);
        const glm::vec3 point = crosshairPoint(s, viewAim);
        const glm::vec3 m = muzzleNow(s, g);
        const glm::vec3 shot = hands::forward(weapons::shotAngles(s.rot[g], flatplay::heldIn(g), g == HAND_OFF));
        const glm::vec3 from = shotMuzzle(s, g, viewAim, point); // (what the shot is handed: keepMuzzle)
        const glm::vec3 stop = lineStop(from - shot * glm::distance(m, s.pos[g]), from + shot * aimFar);
        const glm::vec3 toPoint = aimWay(dir, point, from, st.aimBlocked[g]);
        n.shotDeg = degreesApart(shot, toPoint);
        n.shotApart = glm::distance(stop, point);
        n.shotPoint = point;
        n.shotStop = stop;
    }
    view::WeaponFrame wf;
    if(view::weaponFrame(s, g, wf))
    {
        const sightalign::SightLine line = sightalign::sightLine(wf.model);
        if(line.valid)
        {
            const glm::vec3 cam = geom::vec(r_refdef.vieworg);
            const glm::vec3 dir = axesOf(geom::vec(r_refdef.viewangles)) * sightDir();
            float deg = 0.f;
            offTheRay(cam, dir, glm::vec3{wf.modelToWorld * glm::vec4{line.rear, 1.f}}, n.rearOff, deg);
            offTheRay(cam, dir, glm::vec3{wf.modelToWorld * glm::vec4{line.front, 1.f}}, n.frontOff, deg);
        }
    }
    return n;
}

} // namespace qvr::flatsight

// gl_screen.c (VR_FlatAdsFov, vr_api.h): the flatscreen FOV, narrowed while the ironsight is on. Off, or outside the
// mode, it is the FOV asked for -- unchanged for a headset run and for plain flatscreen play.
extern "C" float VR_FlatAdsFov(float basefov)
{
    return qvr::flatsight::fov(basefov);
}

// gl_screen.c (SCR_DrawCrosshair, vr_api.h): where the crosshair stands off the screen's centre, as fractions of the screen's
// height, x to the right and y down: the ironsight's picture moved (vr_flat_ads_screen_x / _y). 0 0 outside the mode.
extern "C" float VR_FlatCrosshairAlpha(void)
{
    return qvr::flatsight::crosshairFade();
}

extern "C" void VR_FlatCrosshairShift(float* x, float* y)
{
    const glm::vec2 shift = qvr::flatsight::screenPlace();
    *x = shift.x * 0.01f;
    *y = -shift.y * 0.01f;
}
