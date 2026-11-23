// vr_flatswim.cpp -- flatscreen play's swimming as drawn (his words 2026-11-23: under water the hands perform swimming
// strokes based on the direction of movement, their speed following the movement's; any two-handed grip drops to one hand
// so the free hand can swim; firing, melee and reloads take over from the stroke and blend back; docs/vr-port/FLATPLAY.md,
// "Swimming").
//
// A flat-screen player swims as in Quake (his keys, vr_physics_water.cpp's swimmer() answers none for him): this draws it.
// While he swims (Quake's own: the water at his waist off the bottom, or over his head), the hands follow a stroke made up
// of six cycles, each a closed path off its shoulder in the body's frame with the palm turned for it:
//   forward   the breaststroke: out wide and back, palms back, then in under the chin and forward again
//   back      pushing the water ahead: from the chest forward, palms forward
//   sideways  both hands sweeping the water away from the way he goes, palms that way
//   up        pressing the water down, palms down; down: pulling it up, palms up
//   still     treading water: small sweeps in and out at the chest, the palms tilting with the sweep
// The kinds are mixed by the way he moves (his velocity in the view's frame, its forward along the look and its up the
// world's, so a dive is the breaststroke and the jump key the press down), eased (vr_flat_swim_turn_time); his speed
// against vr_flat_swim_full_speed blends treading into them and sets the cycle's rate (vr_flat_swim_tread_rate ..
// vr_flat_swim_rate) and size (vr_flat_swim_size, vr_flat_swim_tread_size).
//
// A free hand takes the stroke's pose (vr_flat_swim_enter_time into it); a hand holding something (a weapon, a prop) only
// moves with the stroke's motion by vr_flat_swim_gun_share, its aim at the crosshair kept (the ironsight's pose comes
// after and aims it). Whatever else a hand does takes over: a shot (the trigger held, then vr_flat_swim_fire_hold
// seconds), a punch, a swing, the shove, the parry, the reload's trip, the force grab, the quick grenade, the hands' swap,
// the ironsight; the stroke eases out over vr_flat_swim_takeover_time and back over vr_flat_swim_return_time. The poses that
// take over come after this one anyway (flatplay::poseHands): the take-over keeps the stroke from fighting them.
//
// The headset's swimming is the tracked hands' (vr_physics_water.cpp): it reads strokes, it makes none, so there is no pose
// to share; what is shared is flatplay's pose framework (the shoulders, the arm's reach, the hand's angles and their
// blend) and the two-handed grip's let-go list (twohand's flatLetsGo, flatsight's gripWanted).
//
// vr_flatswim_report prints the state; vr_flatswim_probe starts a measure of each hand's path and palm (printed by the
// next report): the tests (Misc/quakevr/flatswim_test.sh).

#include "vr_flatswim.hpp"

#include "vr_api.h"
#include "vr_audio.hpp"
#include "vr_avatar.hpp"
#include "vr_backend.hpp" // HAND_OFF / HAND_MAIN
#include "vr_client.hpp"
#include "vr_engine.hpp"
#include "vr_flatfgrab.hpp"
#include "vr_flatgrenade.hpp"
#include "vr_flatplay.hpp"
#include "vr_flatreload.hpp"
#include "vr_flatsight.hpp"
#include "vr_flatswap.hpp"
#include "vr_geom.hpp"
#include "vr_hands.hpp"
#include "vr_handsource.hpp"
#include "vr_particles.hpp"
#include "vr_progs.hpp"
#include "vr_trace.hpp"
#include "vr_tunables.hpp"
#include "vr_twohand.hpp"

#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Acos.hpp"
#include "Zancle/Math/Atan2.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/Exp.hpp"
#include "Zancle/Math/Floor.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sin.hpp"
#include "vr_zancle.hpp"

namespace qvr::flatswim
{

namespace
{

constexpr float twoPi = 6.28318531f;
constexpr float pitchMost = 70.f; // the look's pitch the move is read against, at most (degrees: its forward's cos stays 0.34)
constexpr float strokePitchMost = 45.f; // the stroke's frame follows the look's pitch this far at most (degrees)
constexpr float shapeSpeed = 0.35f; // the stroke all the way out of treading from this share of vr_flat_swim_full_speed (slower: treading mixed in)

enum Kind
{
    Forward,
    Back,
    Side, // signed: + to the right
    Up,
    Down,
    KindCount
};

// A stroke's pose off its shoulder, in shares of the arm's reach, in the body's frame: x ahead, y out (to the hand's own
// side), z up; the palm's way and the fingers' in the same frame. `centre` is the path's middle (the pose without its motion).
struct Pose
{
    glm::vec3 at{0.f};
    glm::vec3 centre{0.f};
    glm::vec3 palm{0.f};
    glm::vec3 fingers{0.f};
};

// Each hand's path and palm over a probe (vr_flatswim_probe .. the next report).
struct Probe
{
    bool on{false};
    int frames{0};
    glm::vec3 lo{1e9f}, hi{-1e9f}; // its place off the shoulder, shares of the reach (x ahead, y out, z up)
    glm::vec3 palm{0.f};           // the palm's way, summed (the same frame)
    float share{0.f};              // the hand's stroke share, summed
    float underLo{1e9f}, underHi{-1e9f}; // where it is seen: degrees under the view's way, from the eye
};

struct SwimState
{
    float in{0.f};                       // the water's share (linear: vr_flat_swim_enter_time)
    float take[2]{1.f, 1.f};             // each hand's, 0 while something else takes it over (linear)
    double firedAt[2]{-1e9, -1e9};       // the trigger last held (vr_gametime)
    float phase{0.f};                    // the cycle, 0..1
    int strokes{0};                      // cycles done
    float move{0.f};                     // the speed against vr_flat_swim_full_speed (eased): the stroke's rate and size
    float shape{0.f};                    // the move's share against treading (eased): full from shapeSpeed of the full speed
    float kind[KindCount]{0.f, 0.f, 0.f, 0.f, 0.f}; // the kinds' mix (eased; Side signed)
    float speed{0.f};                    // units a second, last step
    bool swimming{false};                // last step
    Probe probe[2];
    // The water's sounds and bubbles (vr_flat_swim_sound, vr_flat_swim_bubbles): each stroke's sound as its power starts
    // (strokeStart into the cycle), and each hand's bubbles through its power, their fraction carried to the next frame.
    int sounds{0};                       // stroke sounds played (the report's)
    int lastSound{-1};                   // its kind then: 0 under water, 1 at the surface
    int bubbles{0};                      // bubbles made (the report's)
    float bubbleDue[2]{0.f, 0.f};
    glm::vec3 lastPos[2]{glm::vec3{0.f}, glm::vec3{0.f}};
    double lastPosAt[2]{-1.0, -1.0};     // (cl.time: the hand's speed for its bubbles)
};

SwimState sw;

// The water sounds a stroke plays (the headset's swimming's: vr_physics_water.cpp's; precached by the client, prepare):
// under water a soft or a brisk whoosh (make_sounds.py), at the surface a stroke's recording.
constexpr int underCount = 3, surfaceCount = 4;
constexpr const char* softNames[underCount] = {"vr/swim_soft1.wav", "vr/swim_soft2.wav", "vr/swim_soft3.wav"};
constexpr const char* hardNames[underCount] = {"vr/swim_hard1.wav", "vr/swim_hard2.wav", "vr/swim_hard3.wav"};
constexpr const char* surfaceNames[surfaceCount] = {"vr/stroke1.wav", "vr/stroke2.wav", "vr/stroke3.wav", "vr/stroke4.wav"};
struct StrokeSounds
{
    sfx_t* soft[underCount]{};
    sfx_t* hard[underCount]{};
    sfx_t* surface[surfaceCount]{};
};
StrokeSounds strokeSounds;
constexpr float strokeStart = 0.04f;   // the cycle's share where the power begins to tell: the sound (the power: 0 .. 0.5)
constexpr float bubbleRate = 16.f;     // bubbles a second off a hand through the power, at full speed and vr_flat_swim_bubbles 1

[[nodiscard]] const char* handName(int h)
{
    return h == HAND_MAIN ? "main" : "off";
}

[[nodiscard]] float ease(float k, float dt)
{
    return dt > 0.f ? 1.f - za::exp(-k * dt) : 1.f;
}

[[nodiscard]] bool on()
{
    return flatplay::active() && vr_flat_swim.value != 0.f;
}

// The cycles, at `t` (radians) and `amp` (their motion's size). The power half is sin t > 0 in each.
[[nodiscard]] Pose breaststroke(float t, float amp)
{
    const float c = za::cos(t), s = za::sin(t), k = 0.5f + 0.5f * s;
    Pose p;
    p.centre = {0.42f, 0.f, -0.12f};
    p.at = p.centre + amp * glm::vec3{0.28f * c, 0.26f * s - 0.12f * c, -0.06f * s};
    // (pulling: palms back and out, the fingers down and out, the forearm caught on the water; the recovery: palms in and
    // down, together under the chin, the fingers ahead)
    p.palm = glm::mix(glm::vec3{0.f, -0.8f, -0.6f}, glm::vec3{-0.8f, 0.5f, -0.2f}, k);
    p.fingers = glm::mix(glm::vec3{1.f, 0.1f, 0.f}, glm::vec3{0.45f, 0.35f, -0.8f}, k);
    return p;
}

[[nodiscard]] Pose backstroke(float t, float amp)
{
    const float c = za::cos(t), s = za::sin(t), k = 0.5f + 0.5f * s;
    Pose p;
    p.centre = {0.38f, 0.12f, -0.12f};
    p.at = p.centre + amp * glm::vec3{-0.22f * c, 0.05f * s, -0.08f * s};
    // (pushing ahead: palms forward, the fingers up; back: palms down)
    p.palm = glm::mix(glm::vec3{0.f, -0.3f, -1.f}, glm::vec3{0.95f, 0.f, -0.3f}, k);
    p.fingers = glm::mix(glm::vec3{1.f, 0.f, 0.f}, glm::vec3{0.3f, 0.f, 1.f}, k);
    return p;
}

// `dir` +1: moving to the right (the water swept to the left). `side`: the hand's (+1 the right one), the out axis's.
[[nodiscard]] Pose sidestroke(float t, float amp, float dir, float side)
{
    const float c = za::cos(t), s = za::sin(t), k = 0.5f + 0.5f * s;
    Pose p;
    p.centre = {0.4f, 0.1f, -0.15f};
    // (along the body's right, then turned into the hand's out: times its side)
    p.at = p.centre + amp * glm::vec3{0.04f * s, dir * side * 0.24f * c, -0.08f * s};
    p.palm = glm::mix(glm::vec3{0.f, 0.f, -1.f}, glm::vec3{0.f, -dir * side, -0.2f}, k);
    p.fingers = {1.f, 0.f, 0.f};
    return p;
}

[[nodiscard]] Pose pressDown(float t, float amp)
{
    const float c = za::cos(t), s = za::sin(t), k = 0.5f + 0.5f * s;
    Pose p;
    p.centre = {0.36f, 0.14f, -0.1f};
    p.at = p.centre + amp * glm::vec3{0.f, 0.08f * s, 0.2f * c};
    p.palm = glm::mix(glm::vec3{0.f, -1.f, 0.f}, glm::vec3{-0.2f, 0.f, -1.f}, k);
    p.fingers = {1.f, 0.3f, 0.f};
    return p;
}

[[nodiscard]] Pose pullUp(float t, float amp)
{
    const float c = za::cos(t), s = za::sin(t), k = 0.5f + 0.5f * s;
    Pose p;
    p.centre = {0.36f, 0.14f, -0.2f};
    p.at = p.centre + amp * glm::vec3{0.f, 0.08f * s, -0.2f * c};
    p.palm = glm::mix(glm::vec3{0.f, -1.f, 0.f}, glm::vec3{0.f, 0.f, 1.f}, k);
    p.fingers = {1.f, 0.3f, 0.f};
    return p;
}

[[nodiscard]] Pose tread(float t, float amp)
{
    const float c = za::cos(t), s = za::sin(t);
    Pose p;
    p.centre = {0.42f, 0.12f, -0.12f};
    p.at = p.centre + amp * glm::vec3{0.04f * s, 0.14f * c, 0.02f * za::sin(2.f * t)};
    // (the palm down, tilted to lead the sweep: out while it goes out)
    p.palm = {0.f, -0.5f * s, -1.f};
    p.fingers = {1.f, 0.2f, 0.f};
    return p;
}

void addPose(Pose& sum, const Pose& p, float w)
{
    sum.at += p.at * w;
    sum.centre += p.centre * w;
    sum.palm += p.palm * w;
    sum.fingers += p.fingers * w;
}

// The stroke as it is now for a hand of `side` (+1 the right one).
[[nodiscard]] Pose strokeNow(float side)
{
    const float t = sw.phase * twoPi;
    const float size = za::clamp(vr_flat_swim_size.value, 0.f, 2.f);
    const float amp = size * (0.6f + 0.4f * sw.move);
    Pose moving;
    float total = 0.f;
    const float kinds[KindCount]{sw.kind[Forward], sw.kind[Back], za::abs(sw.kind[Side]), sw.kind[Up], sw.kind[Down]};
    for(float k : kinds)
    {
        total += k;
    }
    Pose sum;
    float m = sw.shape;
    if(total > 0.05f)
    {
        addPose(moving, breaststroke(t, amp), kinds[Forward] / total);
        addPose(moving, backstroke(t, amp), kinds[Back] / total);
        addPose(moving, sidestroke(t, amp, sw.kind[Side] >= 0.f ? 1.f : -1.f, side), kinds[Side] / total);
        addPose(moving, pressDown(t, amp), kinds[Up] / total);
        addPose(moving, pullUp(t, amp), kinds[Down] / total);
        addPose(sum, moving, m);
    }
    else
    {
        m = 0.f;
    }
    addPose(sum, tread(t, za::clamp(vr_flat_swim_tread_size.value, 0.f, 2.f)), 1.f - m);
    return sum;
}

// A stroke's water sound, heard in the head (your own swimming): under water a whoosh, soft or brisk by the speed; at
// the surface a stroke's splash. Louder the faster he swims.
void strokeSound()
{
    const float volume = za::clamp(vr_flat_swim_sound.value, 0.f, 1.f) * (0.35f + 0.65f * sw.move);
    const edict_t* local = progs::localPlayer();
    if(volume <= 0.f || !local)
    {
        return;
    }
    const bool under = static_cast<int>(local->v.waterlevel) >= 3;
    const int k = sw.strokes;
    sfx_t* const sfx = !under            ? strokeSounds.surface[k % surfaceCount]
                       : sw.move >= 0.5f ? strokeSounds.hard[k % underCount]
                                         : strokeSounds.soft[k % underCount];
    audio::playLocal(sfx, volume);
    sw.sounds++;
    sw.lastSound = under ? 0 : 1;
}

// Bubbles off hand `h` at `at` through the stroke's power, under water (vr_flat_swim_bubbles): its share of the rate.
void bubblesOff(int h, const glm::vec3& at, float w)
{
    const double now = cl.time;
    const float dt = sw.lastPosAt[h] >= 0.0 ? static_cast<float>(za::clamp(now - sw.lastPosAt[h], 0.0, 0.1)) : 0.f;
    const glm::vec3 vel = dt > 0.f ? (at - sw.lastPos[h]) / dt : glm::vec3{0.f};
    sw.lastPos[h] = at;
    sw.lastPosAt[h] = now;
    const float amount = za::clamp(vr_flat_swim_bubbles.value, 0.f, 5.f);
    const bool power = sw.phase >= strokeStart && sw.phase < 0.5f;
    if(amount <= 0.f || dt <= 0.f || !power || !sw.swimming ||
        !worldtrace::isLiquid(worldtrace::liquidContents(at, true)))
    {
        sw.bubbleDue[h] = 0.f;
        return;
    }
    sw.bubbleDue[h] += bubbleRate * amount * w * (0.4f + 0.6f * sw.move) * dt;
    const float n = za::floor(sw.bubbleDue[h]);
    if(n >= 1.f)
    {
        sw.bubbleDue[h] -= n;
        // (Carried most of the way with the swimmer, a little of the hand's own sweep on top: left still in the water
        // they were gone behind him in a fraction of a second, never seen.)
        const glm::vec3 body{cl.velocity[0], cl.velocity[1], cl.velocity[2]};
        particles::swimBubbles(at, body * 0.85f + (vel - body) * 0.3f, n);
        sw.bubbles += static_cast<int>(n);
    }
}

// Something else has hand `h` now: the stroke gives way to it.
[[nodiscard]] bool takenOver(int h, bool holds)
{
    if(client::attacking(h))
    {
        sw.firedAt[h] = vr_gametime;
    }
    const bool fired = vr_gametime - sw.firedAt[h] < static_cast<double>(za::clamp(vr_flat_swim_fire_hold.value, 0.f, 3.f));
    return fired || flatplay::punching(h) || flatplay::shoving(h) || flatplay::parryShare(h) > 0.f ||
           flatplay::melee2HShare(h) > 0.f || flatreload::busy(h) || flatfgrab::busy(h) || flatgrenade::busy(h) ||
           flatswap::busy() || (holds && flatsight::adsShare() > 0.01f);
}

void report_f()
{
    const edict_t* local = progs::localPlayer();
    Con_Printf("flatswim: swimming %d in %.2f phase %.2f strokes %d speed %.0f move %.2f kinds fwd %.2f back %.2f side %+.2f up %.2f "
               "down %.2f water %d at %.0f %.0f %.0f\n",
        sw.swimming ? 1 : 0, sw.in, sw.phase, sw.strokes, sw.speed, sw.move, sw.kind[Forward], sw.kind[Back], sw.kind[Side],
        sw.kind[Up], sw.kind[Down], local ? static_cast<int>(local->v.waterlevel) : -1, local ? local->v.origin[0] : 0.f,
        local ? local->v.origin[1] : 0.f, local ? local->v.origin[2] : 0.f);
    Con_Printf("flatswim: water: %d stroke sounds (last %s; vr_flat_swim_sound %g), %d bubbles (vr_flat_swim_bubbles %g), "
               "hands open main %.2f off %.2f (vr_flat_swim_cup %g); particles live %d; main hand at %.0f %.0f %.0f\n",
        sw.sounds, sw.lastSound < 0 ? "none" : sw.lastSound == 0 ? "under water" : "at the surface", vr_flat_swim_sound.value,
        sw.bubbles, vr_flat_swim_bubbles.value, openShare(HAND_MAIN), openShare(HAND_OFF), vr_flat_swim_cup.value,
        particles::liveCount(), sw.lastPos[HAND_MAIN].x, sw.lastPos[HAND_MAIN].y, sw.lastPos[HAND_MAIN].z);
    // The view against his own body (his note 2026-11-23: looking up under water he saw his own shoulders from below):
    // the shoulders' furthest ahead of the eye along the view (units; > 0 in front of it) and their least angle off it; the
    // eye ahead of the (hidden) head's joint, level along the view's yaw (< 0: back over the trunk, among the shoulders).
    if(avatar::Skeleton k; flatplay::drawnSkeleton(hands::current(), k))
    {
        const glm::vec3 eye{r_refdef.vieworg[0], r_refdef.vieworg[1], r_refdef.vieworg[2]};
        const glm::vec3 f = hands::forward(glm::vec3{r_refdef.viewangles[0], r_refdef.viewangles[1], r_refdef.viewangles[2]});
        const glm::vec3 level = hands::forward(glm::vec3{0.f, r_refdef.viewangles[1], 0.f});
        float ahead = -1e9f, off = 180.f;
        for(const glm::vec3& sh : k.shoulder)
        {
            const glm::vec3 d = sh - eye;
            ahead = za::max(ahead, glm::dot(d, f));
            if(glm::length(d) > 1e-3f)
            {
                off = za::min(off, glm::degrees(za::acos(za::clamp(glm::dot(glm::normalize(d), f), -1.f, 1.f))));
            }
        }
        Con_Printf("flatswim: view pitch %.0f: the shoulders %.1f ahead of the eye along the view, %.0f deg off it; the eye %.1f "
                   "ahead of the head joint; body pitch %.1f\n",
            r_refdef.viewangles[0], ahead, off, glm::dot(eye - k.headJoint, level), avatar::ownPitchAngle());
    }
    for(int h : {HAND_OFF, HAND_MAIN})
    {
        const Probe& p = sw.probe[h];
        const float n = static_cast<float>(za::max(1, p.frames));
        const glm::vec3 palm = p.palm / n;
        Con_Printf("flatswim: %s hand %s share %.2f take %.2f helping %d stance %.2f; probe %d frames: ahead %.2f..%.2f out %.2f..%.2f up "
                   "%.2f..%.2f palm (%+.2f %+.2f %+.2f) mean share %.2f seen %.0f..%.0f deg under the view\n",
            handName(h), flatplay::heldIn(h) >= 0 || flatplay::holdsThing(h) ? "holds" : "free", share(h), sw.take[h],
            twohand::helping(h) ? 1 : 0, flatsight::stanceShare(1 - h), p.frames, p.lo.x, p.hi.x, p.lo.y, p.hi.y, p.lo.z,
            p.hi.z, palm.x, palm.y, palm.z, p.share / n, p.frames > 0 ? p.underLo : 0.f, p.frames > 0 ? p.underHi : 0.f);
    }
}

void probe_f()
{
    for(Probe& p : sw.probe)
    {
        p = Probe{};
        p.on = true;
    }
}

} // namespace

void init()
{
    Cmd_AddCommand("vr_flatswim_report", report_f);
    Cmd_AddCommand("vr_flatswim_probe", probe_f);
}

bool swimming()
{
    const edict_t* local = progs::localPlayer();
    if(!local || local->v.health <= 0.f || static_cast<int>(local->v.movetype) == MOVETYPE_NOCLIP)
    {
        return false;
    }
    const int level = static_cast<int>(local->v.waterlevel);
    return level >= 3 || (level == 2 && (static_cast<int>(local->v.flags) & FL_ONGROUND) == 0);
}

bool freesHand()
{
    return on() && vr_flat_swim_one_hand.value != 0.f && swimming();
}

void step(float dt, const glm::vec3& aim)
{
    sw.swimming = on() && swimming();
    const float enter = za::max(0.02f, vr_flat_swim_enter_time.value);
    sw.in = za::clamp(sw.in + (sw.swimming ? dt : -dt) / enter, 0.f, 1.f);

    // The move in the view's frame: its forward along the look, its up the world's, its side the body's right.
    const glm::vec3 v{cl.velocity[0], cl.velocity[1], cl.velocity[2]};
    const float pitch = za::clamp(aim.x, -pitchMost, pitchMost);
    glm::vec3 viewFwd, right, up, levelFwd, levelRight, levelUp;
    hands::angleVectors({pitch, aim.y, 0.f}, viewFwd, right, up);
    hands::angleVectors({0.f, aim.y, 0.f}, levelFwd, levelRight, levelUp);
    const float along = glm::dot(v, levelFwd) / za::max(0.3f, za::cos(glm::radians(pitch)));
    const float rise = v.z - along * viewFwd.z;
    const float aside = glm::dot(v, levelRight);
    sw.speed = glm::length(v);
    const float total = za::abs(along) + za::abs(rise) + za::abs(aside);
    float want[KindCount]{0.f, 0.f, 0.f, 0.f, 0.f};
    if(total > 1.f)
    {
        want[Forward] = za::max(along, 0.f) / total;
        want[Back] = za::max(-along, 0.f) / total;
        want[Side] = aside / total;
        want[Up] = za::max(rise, 0.f) / total;
        want[Down] = za::max(-rise, 0.f) / total;
    }
    const float turnK = 3.f / za::max(0.02f, vr_flat_swim_turn_time.value); // (95% of the way in that time)
    for(int k = 0; k < KindCount; k++)
    {
        sw.kind[k] += (want[k] - sw.kind[k]) * ease(turnK, dt);
    }
    const float full = za::max(10.f, vr_flat_swim_full_speed.value);
    const float moveWant = sw.swimming ? za::clamp(sw.speed / full, 0.f, 1.f) : 0.f;
    sw.move += (moveWant - sw.move) * ease(turnK, dt);
    const float shapeWant = sw.swimming ? za::clamp(sw.speed / (full * shapeSpeed), 0.f, 1.f) : 0.f;
    sw.shape += (shapeWant - sw.shape) * ease(turnK, dt);

    // The cycle: treading's rate still, the stroke's at full speed.
    const float rate = za::clamp(vr_flat_swim_tread_rate.value, 0.f, 4.f) +
                       (za::clamp(vr_flat_swim_rate.value, 0.f, 4.f) - za::clamp(vr_flat_swim_tread_rate.value, 0.f, 4.f)) * sw.move;
    if(sw.in > 0.f)
    {
        // (Each stroke's sound as its power starts: the cycle passing strokeStart.)
        const float before = sw.phase, after = sw.phase + rate * dt;
        if(sw.swimming && sw.in >= 0.5f && za::floor(after - strokeStart) > za::floor(before - strokeStart))
        {
            strokeSound();
        }
        sw.phase += rate * dt;
        while(sw.phase >= 1.f)
        {
            sw.phase -= 1.f;
            sw.strokes++;
        }
    }
    else
    {
        sw.phase = 0.f; // (each swim starts with the stroke's reach)
    }

    // Each hand's take-over: out fast, back slower. (On land, nothing to give way: each starts in the water free.)
    if(!sw.swimming && sw.in <= 0.f)
    {
        sw.take[HAND_OFF] = sw.take[HAND_MAIN] = 1.f;
        sw.firedAt[HAND_OFF] = sw.firedAt[HAND_MAIN] = -1e9;
        return;
    }
    const float out = za::max(0.01f, vr_flat_swim_takeover_time.value);
    const float back = za::max(0.01f, vr_flat_swim_return_time.value);
    for(int h : {HAND_OFF, HAND_MAIN})
    {
        const bool holds = flatplay::heldIn(h) >= 0 || flatplay::holdsThing(h);
        sw.take[h] = takenOver(h, holds) ? za::max(0.f, sw.take[h] - dt / out) : za::min(1.f, sw.take[h] + dt / back);
    }
}

void poseHand(hands::State& s, int h, const glm::vec3& aim, const glm::vec3& shoulder, float span, bool measure)
{
    const float w = share(h);
    if(w <= 0.f || span <= 0.f)
    {
        return;
    }
    const float side = h == HAND_OFF ? -1.f : 1.f;
    const Pose p = strokeNow(side);
    // The stroke's frame: the body's way, tipped with a share of the look's pitch (looking down, the strokes stay in sight).
    const float tip = za::clamp(aim.x * za::clamp(vr_flat_swim_pitch.value, 0.f, 1.f), -strokePitchMost, strokePitchMost);
    glm::vec3 fwd, right, up;
    hands::angleVectors({tip, aim.y, 0.f}, fwd, right, up);
    const glm::vec3 out = right * side;
    const auto world = [&](const glm::vec3& l) { return fwd * l.x + out * l.y + up * l.z; };
    const bool holds = flatplay::heldIn(h) >= 0 || flatplay::holdsThing(h);
    if(holds)
    {
        // (A weapon or a prop: the stroke's motion, a little; its aim kept.)
        const float g = za::clamp(vr_flat_swim_gun_share.value, 0.f, 1.f);
        s.pos[h] += world(p.at - p.centre) * (span * g * w);
    }
    else
    {
        // (the strokes moved ahead and up as a whole, into sight: vr_flat_swim_fwd, vr_flat_swim_up)
        const glm::vec3 lift{za::clamp(vr_flat_swim_fwd.value, -0.5f, 0.5f), 0.f, za::clamp(vr_flat_swim_up.value, -0.5f, 0.5f)};
        const glm::vec3 to = shoulder + world(p.at + lift) * span;
        s.pos[h] += (to - s.pos[h]) * w;
        s.rot[h] = flatplay::blendHandAngles(s.rot[h], flatplay::handFacing(h, world(p.fingers), world(p.palm)), w);
        handsource::mark(h, handsource::Source::Synthesized, "swim stroke");
    }
    if(measure)
    {
        bubblesOff(h, s.pos[h], holds ? w * za::clamp(vr_flat_swim_gun_share.value, 0.f, 1.f) : w);
    }

    Probe& pr = sw.probe[h];
    if(measure && pr.on)
    {
        const glm::vec3 d = (s.pos[h] - shoulder) / span;
        const glm::vec3 l{glm::dot(d, fwd), glm::dot(d, out), glm::dot(d, up)};
        pr.lo = glm::min(pr.lo, l);
        pr.hi = glm::max(pr.hi, l);
        glm::vec3 hf, hr, hu;
        hands::angleVectors(s.rot[h], hf, hr, hu);
        const glm::vec3 palm = -side * hr;
        pr.palm += glm::vec3{glm::dot(palm, fwd), glm::dot(palm, out), glm::dot(palm, up)};
        pr.share += w;
        pr.frames++;
        // (where it is seen: degrees under the view's way, from the eye)
        glm::vec3 vf, vr, vu;
        hands::angleVectors(aim, vf, vr, vu);
        const glm::vec3 e = s.pos[h] - s.head;
        const float under = glm::degrees(za::atan2(-glm::dot(e, vu), za::max(0.01f, glm::dot(e, vf))));
        pr.underLo = za::min(pr.underLo, under);
        pr.underHi = za::max(pr.underHi, under);
    }
}

float share(int h)
{
    return h >= 0 && h < 2 ? geom::smooth01(sw.in) * geom::smooth01(sw.take[h]) : 0.f;
}

float openShare(int h)
{
    return flatplay::heldIn(h) >= 0 || flatplay::holdsThing(h) ? 0.f : share(h);
}

void prepare()
{
    for(int i = 0; i < underCount; i++)
    {
        strokeSounds.soft[i] = S_PrecacheSound(softNames[i]);
        strokeSounds.hard[i] = S_PrecacheSound(hardNames[i]);
    }
    for(int i = 0; i < surfaceCount; i++)
    {
        strokeSounds.surface[i] = S_PrecacheSound(surfaceNames[i]);
    }
}

} // namespace qvr::flatswim
