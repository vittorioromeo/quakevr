// vr_flashlight.cpp -- see vr_flashlight.hpp.

#include "vr_flashlight.hpp"
#include "vr_avatar.hpp"
#include "vr_body.hpp"
#include "vr_cvars.hpp"
#include "vr_gfx.hpp"
#include "vr_hue.hpp"
#include "vr_lighting.hpp"
#include "vr_lines.hpp"
#include "vr_main.hpp"
#include "vr_profile.hpp"
#include "vr_trace.hpp"
#include "vr_units.hpp"
#include "vr_view.hpp"
#include "vr_weapons.hpp"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_map>
#include <vector>

namespace qvr::flashlight
{
namespace
{

// progs/vrflashlight.mdl (make_flashlight.py): a straight torch (round 21), model units at vr_world_scale 1: +x along
// the tube to the lens (the beam), +z the side its switch is on, +y left; the origin on the axis in the middle of the
// grip, where the fist holds it.
constexpr const char* modelName = "progs/vrflashlight.mdl";
constexpr glm::vec3 lensPoint{1.982f, 0.f, 0.f};
constexpr glm::vec3 capPoint{-1.365f, 0.f, 0.f};      // the tail's end, where the cord goes in
constexpr glm::vec3 switchPoint{0.840f, 0.f, 0.373f}; // the switch (its clicks come from there)
constexpr float lensRadius = 0.4147f;                 // the lens's (1.58 cm)
constexpr float tailLength = 0.052f;                  // metres from the grip's middle back to the tail

// The beam: a spot light, full within innerAngle degrees of its axis and smoothly down to none at
// outerAngle; a faint spill round it out to spillAngle. The visible beam's cones are the same:
// spread the tangent of the outer half angle, coreSpread the inner's.
constexpr float innerAngle = 10.f;
constexpr float outerAngle = 22.f;
constexpr float spillAngle = 45.f;
const float spread = std::tan(glm::radians(outerAngle));
const float coreSpread = std::tan(glm::radians(innerAngle));

constexpr float reach = 0.09f;         // metres from the torch's axis (tail to lens) a hand reaches it at
constexpr float returnOmega = 14.f;    // the cord's pull (critically damped; home in about 0.4 s)
constexpr float maxThrow = 3.f;        // metres per second the lamp keeps of the hand's at a release
constexpr float gunReach = 0.12f;      // metres from the other hand's weapon a hand holds it with both at (the intent gate)
// Where the belt clip is from the pelvis joint (metres): its front, to the side, up.
constexpr float beltFront = 0.11f;
constexpr float beltSide = 0.09f;
constexpr float beltUp = 0.10f;
constexpr float headAim = 4.f;         // metres ahead of the eyes the head torch's beam crosses the line of sight


// The dynamic lights' keys (entities' keys are their numbers, never negative).
constexpr int keySpot = -0x0F1A51;
constexpr int keySpill = -0x0F1A52;
constexpr int keyLamp = -0x0F1A53;
constexpr int soundEntity = -0x0F1A54; // its clicks' (heard from the lamp, not the head)

enum class Mode
{
    Mounted,
    Held,
    Returning,
    OnGun, // clipped under the barrel of the gun in st.gunHand
    OnHead // round 21: clipped at a temple (st.headSide), a head torch
};

struct Pose
{
    glm::vec3 pos{0.f};
    glm::quat rot{1.f, 0.f, 0.f, 0.f}; // model axes to the world: x the beam, y left, z up
};

// Where the torch is clipped on a gun (findGunSpot): metres under the line the gun aims along, or beside it.
struct GunSpot
{
    float down{0.045f};
    float out{0.f};
};

struct State
{
    bool on{false};
    Mode mode{Mode::Mounted};
    int holder{-1};
    Pose pose;               // as last placed
    bool placed{false};

    // The flight home: the offset from the mount and its velocity at the release, and the turn.
    double flightStart{0.0};
    glm::vec3 flightOffset{0.f};
    glm::vec3 flightVel{0.f};
    glm::quat flightRot{1.f, 0.f, 0.f, 0.f};

    // On a gun: the hand holding it and its model (the same gun with its other ammo keeps it), and where on it.
    int gunHand{-1};
    const qmodel_t* gunModel{nullptr};
    GunSpot gunSpot;
    bool nearGun{false}; // held within reach of the other hand's gun (B/Y clips it on)

    // On the head (round 21): which temple, -1 left or 1 right; held within reach of one (B/Y clips it on there).
    float headSide{-1.f};
    bool nearHead{false};
    float nearHeadSide{-1.f};

    // Round 21: each hand's grip, flipped with its B/Y while held away from a gun: the low grip (false: the beam out of
    // the thumb's side) or the overhead one (true: out of the little finger's side); kept for the next time that hand
    // takes it.
    bool overhead[2]{};
    double flipAt[2]{-10.0, -10.0}; // realtime each hand's last flip began (its spin: flipTime)

    // Round 21, what a deliberate press is (intent): per hand, for the grip [0] and the trigger [1], since when the
    // analog value has been under openBelow (-1: it is not) and when it last was; and until when the hand counts as
    // moving fast (a punch, a swing).
    double openSince[2][2]{{-1.0, -1.0}, {-1.0, -1.0}};
    double openFrom[2][2]{};  // the last open stretch's start and end
    double openUntil[2][2]{};
    double fastUntil[2]{};
    glm::vec3 lastPos[2]{glm::vec3{0.f}, glm::vec3{0.f}}; // where the hands were at the last frame (lastPosTime)
    double lastPosTime{-1.0};

    bool swallowed[2][3]{}; // [hand][Button]: a press the flashlight took, whose release it takes too
    bool gripDown[2]{};
    bool tookGrip[2]{};     // see flashlight::tookGrip
    bool hovered[2]{};
    const qmodel_t* world{nullptr};

    float beamLength{-1.f}; // the visible beam's length, eased towards where the beam lands (<0: none yet)
    double beamTime{0.0};
};

State st;

// The visible beam (vr_flashlight_beam): an open cone of light in the air from the lens, and a
// narrower one inside it for its brighter core, added onto each eye's scene. Shaped once a frame:
// rings along the axis, each ring's points pulled in where a wall or the floor cuts the cone (and
// dark there, so it fades out where it meets them rather than showing a hard line); each eye then
// shades it by the angle it sees each point at.
constexpr int beamSides = 16;
constexpr int beamRings = 10;
constexpr float beamLookPast = 1.3f; // how far out walls are looked for, of the radius
constexpr float beamGain = 0.35f;    // the light the air adds at vr_flashlight_beam 1, near the lens

struct Beam
{
    bool visible{false};
    glm::vec3 dir{1.f, 0.f, 0.f};
    glm::vec3 color{0.f};
    glm::vec3 around[beamSides];         // unit directions round the axis
    glm::vec3 axis[beamRings];           // each ring's middle
    float radius[beamRings]{};           // the outer cone's radius there
    float glow[beamRings]{};             // how bright the air is there (the light spreading, the end)
    float reach[beamRings][beamSides]{}; // how far out a wall lets it reach, of the radius (up to beamLookPast)
};

Beam beam;

// How closely the beam's cone finds the walls that cut it (vr_flashlight_beam_quality). Each ring's sides are traced
// (a short line out from the axis) every `sideStep`, and the rings every `ringStep` (the last ring always); the ones in
// between take the mean of their traced neighbours'. With `reuse`, a trace is kept while its line has moved less than
// 1% of its length (at least a tenth of a unit) and for at most `reuseSeconds`: a torch held still, or on the chest
// of a player standing still, traces little; a moving one, all of them. High is the beam as it always was.
struct BeamQuality
{
    int sideStep;
    int ringStep;
    bool reuse;
};
constexpr BeamQuality beamQualities[] = {
    {2, 2, true},  // low: 8 sides of 5 rings, 40 traces at most
    {2, 1, true},  // medium: 8 sides of 9 rings, 72 at most
    {1, 1, false}, // high: 16 sides of 9 rings, 144 every frame
};
constexpr double reuseSeconds = 0.1; // (a door moving through a still beam is seen within this)

// The traces kept for reuse: each one's line and result.
struct BeamTrace
{
    glm::vec3 from{0.f}, to{0.f};
    float reach{0.f};
    double time{-1.0}; // realtime it was traced (-1: never)
};
BeamTrace beamTraces[beamRings][beamSides];

[[nodiscard]] bool enabled()
{
    return vr_flashlight.value != 0.f && vrActive();
}

// Metres to world units for things sized with the body.
[[nodiscard]] float bodyUnits()
{
    return units::metresToUnits() * units::bodyScale();
}

[[nodiscard]] glm::vec3 modelPointAt(const Pose& p, const glm::vec3& point)
{
    return p.pos + p.rot * (point * units::worldScale());
}

[[nodiscard]] Pose poseFromAxes(const glm::vec3& pos, const glm::vec3& fwd, const glm::vec3& left, const glm::vec3& up)
{
    return {pos, glm::normalize(glm::quat_cast(glm::mat3{fwd, left, up}))};
}

// Stored on the belt (round 21: hanging; on the chest at first, where a boxing guard's fist closed on it): clipped by
// its tail to the belt on the off hand's side, between the buckle and the hip holster, hanging straight down over the
// hip, the lens at the bottom, the switch out, leaning its lens out from the body by vr_flashlight_tilt
// (vr_flashlight_forward, _up and _out move it). Switched on there, it lights the floor at your feet: of no use but to
// find it, so that it is taken in a hand, clipped on a gun or put on the head.
//
// Why the belt: in your 474 recorded takes no hand pressed its grip or trigger within reach of it (at the chest, 7
// takes did: the guard, the pommel's draw back), and it is out of the way of the guard, the gadget and the upper
// holsters; the hip holster's slot is 12 cm further out, beyond the torch's reach, so a draw from it takes the gun.
[[nodiscard]] Pose mountPose(const hands::State& s)
{
    const avatar::Torso torso = avatar::torso(s);
    const glm::vec3 up = torso.pelvis.rot[0];
    const glm::vec3 fwd = torso.pelvis.rot[2];
    const glm::vec3 left = glm::cross(up, fwd);
    const float side = vr_lefthanded.value != 0.f ? -1.f : 1.f;

    // The clip on the belt's front (make_vrbody.py's torso rings: the belt 10-20% up from the hips), beltUp above the
    // pelvis joint and beltSide to the side, deeper for the brawnier builds; the tube's axis 2.2 cm in front of it
    // (the head's radius and a little). The torch hangs 13 cm down from there, over the hip.
    const int build = static_cast<int>(vr_body_build.value);
    const float depth = build <= 0 ? 0.9f : build >= 2 ? 1.1f : 1.f;
    const float m2w = bodyUnits();
    const glm::vec3 clip = torso.pelvis.pos + (fwd * (beltFront * depth + 0.022f + vr_flashlight_forward.value) +
                                                  left * (side * (beltSide + vr_flashlight_out.value)) +
                                                  up * (beltUp + vr_flashlight_up.value)) *
                                                  m2w;

    const float lean = glm::radians(CLAMP(-30.f, vr_flashlight_tilt.value, 60.f));
    const glm::vec3 beam = -up * std::cos(lean) + fwd * std::sin(lean); // down, the lens leaning out
    const glm::vec3 out = fwd * std::cos(lean) + up * std::sin(lean);   // the switch's side
    return poseFromAxes(clip + beam * (tailLength * units::metresToUnits()), beam, glm::cross(out, beam), out); // the tail at the clip
}

// In the hand, held like a torch (round 21; before, like a pistol's grip): the tube through the curled fingers, along
// the fist's axis (the hand's up, as a pistol's grip is), the switch towards the knuckles. In the low grip the head is
// out past the thumb and the index finger and the beam goes that way: the hand is pitched forward a quarter turn from
// a pistol's aim to light ahead. In the overhead grip (B/Y flips it) the torch is the other way round in the same
// fist, the beam out of the little finger's side: the fist raised by the head, the thumb towards the face, lights
// ahead. The tracked hand is ahead of and above the drawn fist: vr_flashlight_hand_forward and _up move the grip's
// middle back and down into it; the drawn hand's grasp (vr_grasp.cpp) then fits the palm and fingers round the tube
// (the palm moves 1.2-1.5 cm at the defaults, either grip, either hand).
//
// Round 21, the author's notes: each grip has its own place in the hand (vr_flashlight_low_* and _high_*: cm forward,
// towards the palm and up in the hand, degrees of pitch, yaw and roll about the grip's middle), the off hand's
// mirrored; and B/Y spins the torch over (flipTime), eased, about the knuckles' way across the fist.
constexpr float flipTime = 0.25f;

struct GripAdjust
{
    glm::vec3 pos{0.f}; // cm: forward, towards the palm, up (the hand's)
    glm::vec3 ang{0.f}; // degrees: pitch (about the hand's right), yaw (about its up, towards the palm), roll (about its forward)
};

[[nodiscard]] GripAdjust gripAdjust(bool overhead)
{
    GripAdjust a;
    if(overhead)
    {
        a.pos = {vr_flashlight_high_x.value, vr_flashlight_high_y.value, vr_flashlight_high_z.value};
        a.ang = {vr_flashlight_high_pitch.value, vr_flashlight_high_yaw.value, vr_flashlight_high_roll.value};
    }
    else
    {
        a.pos = {vr_flashlight_low_x.value, vr_flashlight_low_y.value, vr_flashlight_low_z.value};
        a.ang = {vr_flashlight_low_pitch.value, vr_flashlight_low_yaw.value, vr_flashlight_low_roll.value};
    }
    return a;
}

// How far `hand`'s torch has turned over at `when`: 0 the low grip, 1 the overhead one, eased through a flip.
[[nodiscard]] float turnedAt(int hand, double when)
{
    const float k = std::clamp(static_cast<float>(when - st.flipAt[hand]) / flipTime, 0.f, 1.f);
    const float t = st.overhead[hand] ? k : 1.f - k;
    return t * t * (3.f - 2.f * t);
}

// The torch in `hand` turned `turned` of the way from the low grip (0) to the overhead one (1).
[[nodiscard]] Pose handPoseTurned(const hands::State& s, int hand, float turned)
{
    glm::vec3 fwd, right, up;
    hands::angleVectors(s.rot[hand], fwd, right, up);
    const float m2u = units::metresToUnits();
    const float mirror = hand == HAND_OFF ? -1.f : 1.f;
    const glm::vec3 palm = -right * mirror; // the main hand's palm faces its left, the off hand's its right

    const GripAdjust lo = gripAdjust(false), hi = gripAdjust(true);
    const glm::vec3 cm = glm::mix(lo.pos, hi.pos, turned);
    const glm::vec3 deg = glm::mix(lo.ang, hi.ang, turned);
    const glm::vec3 offset = fwd * (vr_flashlight_hand_forward.value + 0.01f * cm.x) + palm * (0.01f * cm.y) +
                             up * (vr_flashlight_hand_up.value + 0.01f * cm.z);
    Pose p = poseFromAxes(s.pos[hand] + offset * m2u, up, right, fwd);

    // Overhead: half a turn about the knuckles' way (the switch stays towards them), spun through while flipping.
    p.rot = glm::normalize(p.rot * glm::angleAxis(3.14159265f * turned, glm::vec3{0.f, 0.f, 1.f}));
    // The grip's own turn in the hand, about its middle.
    const glm::quat adjust = glm::angleAxis(glm::radians(deg.y * mirror), up) * glm::angleAxis(glm::radians(deg.x), right) *
                             glm::angleAxis(glm::radians(deg.z * mirror), fwd);
    p.rot = glm::normalize(adjust * p.rot);
    return p;
}

[[nodiscard]] Pose handPose(const hands::State& s, int hand)
{
    return handPoseTurned(s, hand, turnedAt(hand, realtime));
}

// Round 21, the author's tuning notes: a grip's fingers on the torch (vr_flashlight_low_* or _high_*), as a weapon's.
[[nodiscard]] Fingers gripFingers(bool overhead)
{
    const auto v = [&](const cvar_t& low, const cvar_t& high) { return overhead ? high.value : low.value; };
    Fingers f;
    f.manual = v(vr_flashlight_low_fingers, vr_flashlight_high_fingers) >= 0.5f;
    f.overlap = CLAMP(0.f, v(vr_flashlight_low_overlap, vr_flashlight_high_overlap), 1.f);
    f.curl[0] = v(vr_flashlight_low_curl_thumb, vr_flashlight_high_curl_thumb);
    f.curl[1] = v(vr_flashlight_low_curl_index, vr_flashlight_high_curl_index);
    f.curl[2] = v(vr_flashlight_low_curl_middle, vr_flashlight_high_curl_middle);
    f.curl[3] = v(vr_flashlight_low_curl_ring, vr_flashlight_high_curl_ring);
    f.curl[4] = v(vr_flashlight_low_curl_pinky, vr_flashlight_high_curl_pinky);
    f.thumbAcross = v(vr_flashlight_low_thumb_across, vr_flashlight_high_thumb_across);
    f.bias[0] = v(vr_flashlight_low_bias_thumb, vr_flashlight_high_bias_thumb);
    f.bias[1] = v(vr_flashlight_low_bias_index, vr_flashlight_high_bias_index);
    f.bias[2] = v(vr_flashlight_low_bias_middle, vr_flashlight_high_bias_middle);
    f.bias[3] = v(vr_flashlight_low_bias_ring, vr_flashlight_high_bias_ring);
    f.bias[4] = v(vr_flashlight_low_bias_pinky, vr_flashlight_high_bias_pinky);
    f.thumb = {v(vr_flashlight_low_thumb_x, vr_flashlight_high_thumb_x), v(vr_flashlight_low_thumb_y, vr_flashlight_high_thumb_y),
        v(vr_flashlight_low_thumb_z, vr_flashlight_high_thumb_z)};
    return f;
}

// The beam's colour (vr_flashlight_hue, _saturation: white at the default saturation 0; the hue -1: the player's).
[[nodiscard]] glm::vec3 beamColor()
{
    return hue::color(vr_flashlight_hue, CLAMP(0.f, vr_flashlight_saturation.value, 1.f), 1.f);
}

// The torch's radius (metres) at `back` metres behind its lens: the head's, then the tube's (its grip rings, the tail
// cap), and how long it is.
constexpr float headBack = 0.025f;
constexpr float torchLength = 0.129f;
constexpr float lensBack = 0.01f; // metres the lens is behind a gun's muzzle
[[nodiscard]] float torchRadius(float back)
{
    return back < headBack ? 0.0195f : back < 0.1245f ? 0.0142f : 0.0065f; // (the tail's rubber button last)
}

// Each gun's spot for the torch (findGunSpot), by model name and size.
std::unordered_map<std::string, GunSpot> gunSpots;

// Where the torch goes on a gun (metres from the line the gun aims along to the torch's axis): under it (`down`) or,
// when the gun's underside there goes too deep (a super nailgun's drum), beside it on the side away from the body
// (`out`). Found from the drawn gun's surface at rest (its first frame: points over each triangle, 5 mm apart or
// closer) over the torch's length, the lens 1 cm behind the muzzle: under, the lowest of those within its reach
// sideways; beside, the outermost within its reach up and down; each with the torch's radius there and 3 mm. Under
// at least 4.5 cm, beside at least 3.5; under unless beside is nearer by 2 cm. Once per gun model and size (the
// weapons' scale), cached.
[[nodiscard]] GunSpot findGunSpot(const view::WeaponMount& m)
{
    // The gun's own view entity: the one drawing its model, mirrored as it is, nearest the hand.
    const view::ViewEntity* gun = nullptr;
    float nearest = 1e9f;
    for(int i = 0; i < cl_numvisedicts; i++)
    {
        const view::ViewEntity* ve = view::find(cl_visedicts[i]);
        if(ve && ve->ent.model == m.model && ve->mirrored == m.mirrored)
        {
            const float d = glm::distance(glm::vec3{ve->ent.origin[0], ve->ent.origin[1], ve->ent.origin[2]}, m.pos);
            if(d < nearest)
            {
                nearest = d;
                gun = ve;
            }
        }
    }
    if(!gun || m.model->type != mod_alias)
    {
        return {}; // (not cached: it is drawn from the next frame)
    }
    std::unordered_map<std::string, GunSpot>& cache = gunSpots;
    const float size = glm::distance(view::modelPoint(*gun, glm::vec3{0.f}), view::modelPoint(*gun, glm::vec3{1.f, 0.f, 0.f}));
    const std::string key = std::string{m.model->name} + va("/%.4f", size);
    if(const auto it = cache.find(key); it != cache.end())
    {
        return it->second;
    }
    const double started = Sys_DoubleTime();
    glm::vec3 fwd, right, up;
    hands::angleVectors(m.rot, fwd, right, up);
    const glm::vec3 outward = right * (m.mirrored ? -1.f : 1.f);
    const float u2m = 1.f / units::metresToUnits();
    const auto* hdr = static_cast<const aliashdr_t*>(Mod_Extradata(gun->ent.model));
    if(hdr->poseverttype != aliashdr_t::PV_QUAKE1 || hdr->numposes < 1)
    {
        return {};
    }
    const auto* base = reinterpret_cast<const byte*>(hdr);
    const auto* verts = reinterpret_cast<const trivertx_t*>(base + hdr->vertexes) + hdr->frames[0].firstpose * hdr->numverts;
    const auto* mesh = reinterpret_cast<const aliasmesh_t*>(base + hdr->meshdesc);
    const auto* indexes = reinterpret_cast<const unsigned short*>(base + hdr->indexes);
    const auto corner = [&](int i) { // a triangle's corner: metres from the muzzle, in the gun's axes (back, out, up)
        const trivertx_t& t = verts[mesh[indexes[i]].vertindex];
        const glm::vec3 p{t.v[0] * hdr->scale[0] + hdr->scale_origin[0], t.v[1] * hdr->scale[1] + hdr->scale_origin[1],
            t.v[2] * hdr->scale[2] + hdr->scale_origin[2]};
        const glm::vec3 d = (view::modelPoint(*gun, p) - m.muzzle) * u2m;
        return glm::vec3{-glm::dot(d, fwd) - lensBack, glm::dot(d, outward), glm::dot(d, up)};
    };
    float down = 0.045f, side = 0.035f;
    for(int i = 0; i + 2 < hdr->numindexes; i += 3)
    {
        const glm::vec3 a = corner(i), b = corner(i + 1), c = corner(i + 2);
        const float span = std::max({glm::distance(a, b), glm::distance(b, c), glm::distance(c, a)});
        const int n = std::clamp(static_cast<int>(std::ceil(span / 0.005f)), 1, 64);
        for(int u = 0; u <= n; u++)
        {
            for(int v = 0; u + v <= n; v++)
            {
                const glm::vec3 q = a + (b - a) * (static_cast<float>(u) / n) + (c - a) * (static_cast<float>(v) / n);
                if(q.x < -0.005f || q.x > torchLength)
                {
                    continue;
                }
                const float r = torchRadius(q.x) + 0.003f;
                if(std::abs(q.y) < r)
                {
                    down = std::max(down, r - q.z);
                }
                if(std::abs(q.z) < r)
                {
                    side = std::max(side, q.y + r);
                }
            }
        }
    }
    GunSpot spot;
    if(side + 0.02f < down)
    {
        spot = {0.f, side};
    }
    else
    {
        spot = {down, 0.f};
    }
    Con_DPrintf("flashlight: %s: under it %.1f cm, beside it %.1f cm: %s (%.1f ms)\n", m.model->name, down * 100.f,
        side * 100.f, spot.out > 0.f ? "beside" : "under", (Sys_DoubleTime() - started) * 1000.0);
    cache.emplace(key, spot);
    return spot;
}

// Clipped on the gun, parallel to its barrel (round 21; before, hanging below like a foregrip): the lens a little behind
// the muzzle, the tube's axis just under the gun or beside it (findGunSpot; vr_flashlight_gun_forward, _up and _out move
// it), running back along the gun, its switch out to the side (away from the body), the beam where the gun aims.
[[nodiscard]] Pose gunPose(const view::WeaponMount& m)
{
    glm::vec3 fwd, right, up;
    hands::angleVectors(m.rot, fwd, right, up);
    const float out = m.mirrored ? -1.f : 1.f; // away from the body: right for the main hand
    const glm::vec3 lens = m.muzzle + (fwd * (-lensBack + vr_flashlight_gun_forward.value) + up * (vr_flashlight_gun_up.value - st.gunSpot.down) +
                                          right * (out * (st.gunSpot.out + vr_flashlight_gun_out.value))) *
                                          units::metresToUnits();
    Pose p = poseFromAxes(glm::vec3{0.f}, fwd, up * out, right * out);
    p.pos = lens - p.rot * (lensPoint * units::worldScale());
    return p;
}

// On the head (round 21): at a temple (side -1 left, 1 right), the lens level with the eyes and 3.5 cm over them, 8.5
// cm out to the side (vr_flashlight_head_forward, _up and _out move it), the tube running back along the side of the
// head, the switch out. The beam goes where the head looks, crossing the line of sight headAim ahead: lit from beside
// the eyes, not from them, the shadows read. Nothing of it is in view: behind the eyes, out at the side.
[[nodiscard]] Pose headPose(const hands::State& s, float side)
{
    glm::vec3 fwd, right, up;
    hands::angleVectors(s.headAngles, fwd, right, up);
    const float m2w = bodyUnits();
    const glm::vec3 lens = s.head + (fwd * vr_flashlight_head_forward.value + right * (side * (0.085f + vr_flashlight_head_out.value)) +
                                        up * (0.035f + vr_flashlight_head_up.value)) *
                                        m2w;
    const glm::vec3 beam = glm::normalize(s.head + fwd * (headAim * units::metresToUnits()) - lens);
    glm::vec3 out = right * side - beam * glm::dot(right * side, beam);
    out = glm::normalize(out);
    Pose p = poseFromAxes(glm::vec3{0.f}, beam, glm::cross(out, beam), out);
    p.pos = lens - p.rot * (lensPoint * units::worldScale());
    return p;
}

// The head's reach zone (round 21; the author's tuning notes: vr_flashlight_head_zone_*): where a held torch's middle
// clips it on the head, and where a hand takes it off. A ball at each temple and one at the forehead, moved forward, up
// and out (away from the head: to the side at the temples, ahead at the forehead), apart from the torch's own place
// there (headPose).
struct HeadZone
{
    glm::vec3 temple[2]; // the left's, the right's
    glm::vec3 forehead;
    float radius;        // world units
};

[[nodiscard]] HeadZone headZone(const hands::State& s)
{
    glm::vec3 fwd, right, up;
    hands::angleVectors(s.headAngles, fwd, right, up);
    const float m2w = bodyUnits();
    const float forward = vr_flashlight_head_zone_forward.value, lift = vr_flashlight_head_zone_up.value,
                out = vr_flashlight_head_zone_out.value;
    HeadZone z;
    for(int i = 0; i < 2; i++)
    {
        const float side = i == 0 ? -1.f : 1.f;
        z.temple[i] = s.head + (right * (side * (0.085f + out)) + up * (0.035f + lift) + fwd * (forward - 0.03f)) * m2w;
    }
    z.forehead = s.head + (fwd * (0.07f + forward + out) + up * (0.06f + lift)) * m2w;
    z.radius = std::fmax(vr_flashlight_head_zone_radius.value, 0.f) * m2w;
    return z;
}

// Where on the head a held torch would clip on (the temple on its side) and how far its middle is from the zone there
// (world units); the forehead counts as the nearer temple.
[[nodiscard]] float headDistance(const hands::State& s, const Pose& lamp, float& side)
{
    glm::vec3 fwd, right, up;
    hands::angleVectors(s.headAngles, fwd, right, up);
    const glm::vec3 middle = modelPointAt(lamp, glm::vec3{0.f});
    side = glm::dot(middle - s.head, right) >= 0.f ? 1.f : -1.f;
    const HeadZone z = headZone(s);
    return std::min(glm::distance(middle, z.temple[side > 0.f ? 1 : 0]), glm::distance(middle, z.forehead));
}

// The gun's reach zone (round 21; the author's tuning notes: vr_flashlight_gun_zone_*): where a held torch's middle clips
// it on the gun: a capsule round the gun's line from the hand to 3 cm past the muzzle, moved along the gun, up and out
// (away from the body), `radius` world units across.
struct GunZone
{
    glm::vec3 a, b;
    float radius;
};

[[nodiscard]] GunZone gunZone(const view::WeaponMount& m)
{
    glm::vec3 fwd, right, up;
    hands::angleVectors(m.rot, fwd, right, up);
    const float m2u = units::metresToUnits();
    const float out = m.mirrored ? -1.f : 1.f;
    const glm::vec3 shift =
        (fwd * vr_flashlight_gun_zone_forward.value + up * vr_flashlight_gun_zone_up.value + right * (out * vr_flashlight_gun_zone_out.value)) * m2u;
    const glm::vec3 along = m.muzzle - m.pos;
    const float len = glm::length(along);
    GunZone z;
    z.a = m.pos + shift;
    z.b = m.muzzle + (len > 1e-3f ? along * (0.03f * m2u / len) : glm::vec3{0.f}) + shift;
    z.radius = std::fmax(vr_flashlight_gun_zone_radius.value, 0.f) * m2u;
    return z;
}

// How far (world units) the lamp's middle is from the gun's zone's line (inside the zone: under its radius).
[[nodiscard]] float gunDistance(const Pose& lamp, const GunZone& z)
{
    const glm::vec3 middle = modelPointAt(lamp, glm::vec3{0.f});
    const glm::vec3 ab = z.b - z.a;
    const float len2 = glm::dot(ab, ab);
    const float t = len2 > 1e-4f ? std::clamp(glm::dot(middle - z.a, ab) / len2, 0.f, 1.f) : 0.f;
    return glm::distance(middle, z.a + ab * t);
}

// Intent (round 21). A fist clenched in a guard next to the stored lamp once switched it on and took it mid-fight (the
// author's recorded punches): a press takes, switches or unclips the lamp only when it is deliberate. The grip or
// trigger pressed from an open hand (under openBelow for at least openFor, the squeeze begun within squeezeWithin: a
// slow squeeze counts), and the hand about still at the lamp (under slowSpeed, and so for the last fastHold seconds).
constexpr float openBelow = 0.3f;
constexpr double openFor = 0.15;
constexpr double squeezeWithin = 0.6;
constexpr float slowSpeed = 1.f; // metres per second (a punch is 2.75 and up)
constexpr double fastHold = 0.15;

// Once a frame: the hands' analog grip and trigger and their speed, for deliberate().
void noteIntent(const hands::State& s)
{
    const InputState& in = tracking().input;
    for(int hand = 0; hand < 2; hand++)
    {
        const float values[2] = {in.hands[hand].gripValue, in.hands[hand].triggerValue};
        for(int k = 0; k < 2; k++)
        {
            if(values[k] < openBelow)
            {
                if(st.openSince[hand][k] < 0.0)
                {
                    st.openSince[hand][k] = realtime;
                }
                st.openFrom[hand][k] = st.openSince[hand][k];
                st.openUntil[hand][k] = realtime;
            }
            else
            {
                st.openSince[hand][k] = -1.0;
            }
        }
        // Fast by the runtime's velocity, or by where it is drawn from frame to frame (a jump: a teleport, the
        // tracking regained, a recorded take starting with the hand already somewhere).
        const float dt = static_cast<float>(realtime - st.lastPosTime);
        const bool jumped = st.lastPosTime >= 0.0 && dt > 0.f && dt < 0.25f &&
                            glm::distance(s.pos[hand], st.lastPos[hand]) / units::metresToUnits() >= slowSpeed * std::max(dt, 1.f / 90.f);
        if(s.valid && (glm::length(s.vel[hand]) >= slowSpeed || jumped))
        {
            st.fastUntil[hand] = realtime + fastHold;
        }
        st.lastPos[hand] = s.pos[hand];
    }
    st.lastPosTime = s.valid ? realtime : -1.0;
}

// Whether a press of `b` by `hand` is deliberate (see noteIntent): the hand still, and for the grip and the trigger,
// pressed from an open hand. Why not, for developer 1.
[[nodiscard]] bool deliberate(int hand, Button b)
{
    const hands::State& s = hands::current();
    const bool still = realtime >= st.fastUntil[hand] && (!s.valid || glm::length(s.vel[hand]) < slowSpeed);
    if(!still)
    {
        Con_DPrintf("torch press ignored: the %s hand moving\n", hand == HAND_MAIN ? "main" : "off");
        return false;
    }
    if(b == Button::Secondary)
    {
        return true;
    }
    const int k = b == Button::Grip ? 0 : 1;
    const bool opened = st.openUntil[hand][k] - st.openFrom[hand][k] >= openFor && realtime - st.openUntil[hand][k] <= squeezeWithin;
    if(!opened)
    {
        Con_DPrintf("torch press ignored: the %s hand's %s not from an open hand\n", hand == HAND_MAIN ? "main" : "off",
            k == 0 ? "grip" : "trigger");
    }
    return opened;
}

// Whether a hand holds nothing (the "fist" or no weapon at all).
[[nodiscard]] bool handEmpty(int hand)
{
    const int slot = weapons::heldSlot(hand);
    return slot < 0 || slot == weapons::fistSlot();
}

// Whether a hand is at the lamp: near its axis, anywhere from the tail to the lens (a long torch is taken by its
// tube or by its head).
[[nodiscard]] bool handNear(const hands::State& s, int hand)
{
    if(!st.placed || !s.valid)
    {
        return false;
    }
    const glm::vec3 a = modelPointAt(st.pose, capPoint);
    const glm::vec3 ab = modelPointAt(st.pose, lensPoint) - a;
    const float t = std::clamp(glm::dot(s.pos[hand] - a, ab) / std::max(glm::dot(ab, ab), 1e-4f), 0.f, 1.f);
    return glm::distance(s.pos[hand], a + ab * t) < reach * units::metresToUnits();
}

// Whether a hand is at the head torch, to take it off: at the lamp, or its fist (where the torch's middle is when that
// hand holds it) in the head's zone on the torch's side (vr_flashlight_head_zone_*: the zone that clips it on).
[[nodiscard]] bool handAtHeadTorch(const hands::State& s, int hand)
{
    if(!st.placed || !s.valid || st.mode != Mode::OnHead)
    {
        return false;
    }
    const HeadZone z = headZone(s);
    const glm::vec3 fist = modelPointAt(handPose(s, hand), glm::vec3{0.f});
    return handNear(s, hand) || glm::distance(fist, z.temple[st.headSide > 0.f ? 1 : 0]) < z.radius;
}

// Whether a hand is at the lamp where it is (on the head: handAtHeadTorch).
[[nodiscard]] bool handAt(const hands::State& s, int hand)
{
    return st.mode == Mode::OnHead ? handAtHeadTorch(s, hand) : handNear(s, hand);
}

// Whether a hand is at the other hand's weapon, to hold it with both: within gunReach of its line from 30 cm behind
// the hand (a two-handed sword's grip below it, its pommel) to the muzzle or the tip.
[[nodiscard]] bool otherWeaponNear(const hands::State& s, int hand)
{
    view::WeaponMount m;
    if(!view::weaponMount(1 - hand, m))
    {
        return false;
    }
    const float m2u = units::metresToUnits();
    const glm::vec3 along = m.muzzle - m.pos;
    const float len = glm::length(along);
    if(len < 1e-3f)
    {
        return false;
    }
    const glm::vec3 a = m.pos - along * (0.3f * m2u / len);
    const glm::vec3 ab = m.muzzle - a;
    const float t = std::clamp(glm::dot(s.pos[hand] - a, ab) / glm::dot(ab, ab), 0.f, 1.f);
    return glm::distance(s.pos[hand], a + ab * t) < gunReach * m2u;
}

// Whether the game's grip wins over the stored lamp's for a hand at it: at the other hand's weapon (holding it with
// both: a two-handed sword held low reaches the belt), or at a hotspot (s.hotspot): the other weapon's two-handed grip,
// passing a weapon between the hands, the handle of a gun carried by its foregrip; or nearer a holster whose reach
// it is in (a draw).
[[nodiscard]] bool gameGripWins(const hands::State& s, int hand)
{
    if(otherWeaponNear(s, hand))
    {
        return true;
    }
    body::Holster holster;
    switch(s.hotspot[hand])
    {
        case body::HS_NONE: return false;
        case body::HS_OFFHAND_2H_GRAB:
        case body::HS_MAINHAND_2H_GRAB:
        case body::HS_HAND_SWITCH:
        case body::HS_CARRIED_GRIP: return !handEmpty(1 - hand); // (with nothing in the other hand, nothing to take)
        case body::HS_LEFT_SHOULDER_HOLSTER: holster = body::LeftShoulder; break;
        case body::HS_RIGHT_SHOULDER_HOLSTER: holster = body::RightShoulder; break;
        case body::HS_LEFT_HIP_HOLSTER: holster = body::LeftHip; break;
        case body::HS_RIGHT_HIP_HOLSTER: holster = body::RightHip; break;
        case body::HS_LEFT_UPPER_HOLSTER: holster = body::LeftUpper; break;
        case body::HS_RIGHT_UPPER_HOLSTER: holster = body::RightUpper; break;
        default: return false;
    }
    const glm::vec3 a = modelPointAt(st.pose, capPoint);
    const glm::vec3 ab = modelPointAt(st.pose, lensPoint) - a;
    const float t = std::clamp(glm::dot(s.pos[hand] - a, ab) / std::max(glm::dot(ab, ab), 1e-4f), 0.f, 1.f);
    return glm::distance(s.pos[hand], body::holsterPosition(s, holster)) < glm::distance(s.pos[hand], a + ab * t);
}

// One of its sounds at a point of it (the switch's clicks at the switch, the clamp's at its middle): heard from the
// lamp, wherever it is (from the head before it has been placed).
void sound(const char* name, const glm::vec3& point)
{
    if(!st.placed)
    {
        S_LocalSound(name);
        return;
    }
    if(sfx_t* sfx = S_PrecacheSound(name))
    {
        const glm::vec3 p = modelPointAt(st.pose, point);
        vec3_t org{p.x, p.y, p.z};
        S_StartSound(soundEntity, 1, sfx, org, 1.f, 1.f);
    }
}

void haptic(int hand, float seconds, float amplitude)
{
    if(Backend* be = backend(); be && !vr_disablehaptics.value)
    {
        be->haptic(hand, seconds, 160.f, amplitude);
    }
}

void toggle(int hand)
{
    st.on = !st.on;
    Con_DPrintf("flashlight: %s\n", st.on ? "on" : "off");
    sound(st.on ? "vr/flashlight_on.wav" : "vr/flashlight_off.wav", switchPoint);
    if(hand >= 0)
    {
        haptic(hand, 0.03f, 0.55f);
    }
}

void take(int hand)
{
    Con_DPrintf("flashlight: taken in the %s hand\n", hand == HAND_MAIN ? "main" : "off");
    st.mode = Mode::Held;
    st.holder = hand;
    haptic(hand, 0.05f, 0.45f);
}

void letGo(const hands::State& s, const Pose& mount)
{
    const int hand = st.holder;
    st.mode = Mode::Returning;
    st.holder = -1;
    st.flightStart = realtime;
    st.flightOffset = st.pose.pos - mount.pos;
    st.flightRot = st.pose.rot;
    st.flightVel = glm::vec3{0.f};
    if(hand >= 0 && s.valid)
    {
        glm::vec3 v = s.vel[hand];
        if(const float len = glm::length(v); len > maxThrow)
        {
            v *= maxThrow / len;
        }
        st.flightVel = v * units::metresToUnits();
    }
    // Far away (a teleport, a respawn): straight home.
    if(glm::length(st.flightOffset) > 2.5f * units::metresToUnits())
    {
        st.mode = Mode::Mounted;
    }
}

// B/Y with the torch in the hand, away from a gun: the other grip (see handPose), with a click and a light buzz.
void flip(int hand)
{
    // A flip during a flip turns back from where the spin is.
    const float was = turnedAt(hand, realtime);
    st.overhead[hand] = !st.overhead[hand];
    const float remaining = st.overhead[hand] ? 1.f - was : was; // of the way still to go, eased
    st.flipAt[hand] = realtime - flipTime * (1.f - remaining);
    Con_DPrintf("flashlight: %s grip in the %s hand\n", st.overhead[hand] ? "overhead" : "low", hand == HAND_MAIN ? "main" : "off");
    sound("vr/flashlight_flip.wav", glm::vec3{0.f});
    haptic(hand, 0.025f, 0.3f);
}

void clipOn(int gunHand, const view::WeaponMount& m)
{
    const int hand = st.holder;
    st.mode = Mode::OnGun;
    st.holder = -1;
    st.gunHand = gunHand;
    st.gunModel = m.model;
    st.gunSpot = findGunSpot(m);
    st.nearGun = false;
    Con_DPrintf("flashlight: clipped on the %s hand's gun\n", gunHand == HAND_MAIN ? "main" : "off");
    sound("vr/flashlight_attach.wav", glm::vec3{0.f});
    haptic(gunHand, 0.04f, 0.6f);
    if(hand >= 0)
    {
        haptic(hand, 0.04f, 0.6f);
    }
}

// Round 21: held at the head, B/Y clips it at the temple there: the gun's clamp's click and buzz.
void clipOnHead(float side)
{
    const int hand = st.holder;
    st.mode = Mode::OnHead;
    st.holder = -1;
    st.headSide = side;
    st.nearHead = false;
    Con_DPrintf("flashlight: on the head, the %s temple\n", side < 0.f ? "left" : "right");
    sound("vr/flashlight_attach.wav", glm::vec3{0.f});
    if(hand >= 0)
    {
        haptic(hand, 0.04f, 0.6f);
    }
}

// Off the head: into `hand` (its grip held at the lamp), or else back to the belt on its cord.
void clipOffHead(const hands::State& s, int hand)
{
    Con_DPrintf("flashlight: off the head, %s\n", hand >= 0 ? "into the hand" : "back to the belt");
    sound("vr/flashlight_detach.wav", glm::vec3{0.f});
    if(hand >= 0)
    {
        take(hand);
        return;
    }
    st.holder = -1;
    letGo(s, mountPose(s));
}

// Off the gun: into `hand` (its grip held at the lamp), or else back to the belt on its cord.
void clipOff(const hands::State& s, int hand)
{
    const int gunHand = st.gunHand;
    st.gunHand = -1;
    st.gunModel = nullptr;
    Con_DPrintf("flashlight: off the gun, %s\n", hand >= 0 ? "into the other hand" : "back to the belt");
    sound("vr/flashlight_detach.wav", glm::vec3{0.f});
    if(gunHand >= 0)
    {
        haptic(gunHand, 0.03f, 0.4f);
    }
    if(hand >= 0)
    {
        take(hand);
        return;
    }
    st.holder = -1; // letGo flies it home from where it is, with no throw
    letGo(s, mountPose(s));
}

void killLight(int key)
{
    for(dlight_t& dl : cl_dlights)
    {
        if(dl.key == key)
        {
            dl.die = 0.f;
            dl.radius = 0.f;
        }
    }
}

void killLights()
{
    for(int key : {keySpot, keySpill, keyLamp})
    {
        killLight(key);
    }
    beam.visible = false;
    st.beamLength = -1.f;
}

// Only the spot light may cast shadows, with vr_flashlight_shadows.
dlight_t* light(int key, const glm::vec3& at, float radius, const glm::vec3& color)
{
    dlight_t* dl = CL_AllocDlight(key);
    dl->origin[0] = at.x;
    dl->origin[1] = at.y;
    dl->origin[2] = at.z;
    dl->radius = radius;
    dl->minlight = 0.f;
    dl->die = static_cast<float>(cl.time) + 0.1f;
    // Quake's falloff (vr_dlight_falloff 0) gives (radius - distance) / 256 of the colour: scaled to
    // about as bright as DarkPlaces' close by, whatever the radius.
    const float k = vr_dlight_falloff.value != 0.f ? 1.f : 200.f / std::max(radius, 1.f);
    dl->color[0] = color.x * k;
    dl->color[1] = color.y * k;
    dl->color[2] = color.z * k;
    // No DarkPlaces boost. The spot light's angle term is softened (a quarter of its light whatever the
    // angle, in its cone): floors and walls the beam grazes are not much darker than what faces it.
    lighting::dlightLook(dl, key == keySpot ? 0.25f : 0.f, 0.f);
    if(key != keySpot || !vr_flashlight_shadows.value)
    {
        lighting::dlightNoShadow(dl);
    }
    return dl;
}

// Shapes the visible beam for this frame (see Beam): from the lens to where the beam lands, `dist`
// away, fading out before it (or in the air, when it lands nowhere near).
void shapeBeam(const Pose& p, const glm::vec3& lens, const glm::vec3& dir, float dist, const glm::vec3& color)
{
    QVR_PROFILE("flashlight beam");
    const float strength = CLAMP(0.f, vr_flashlight_beam.value, 1.f);
    const float m2u = units::metresToUnits();
    // Past 10 m or so the light in the air is too thin to see.
    const float length = std::min(dist - 1.f, 10.f * m2u);
    beam.visible = strength > 0.f && length > 0.1f * m2u;
    if(!beam.visible)
    {
        return;
    }

    beam.dir = dir;
    beam.color = color * (strength * beamGain);
    const glm::vec3 left = p.rot * glm::vec3{0.f, 1.f, 0.f};
    const glm::vec3 up = p.rot * glm::vec3{0.f, 0.f, 1.f};
    for(int j = 0; j < beamSides; j++)
    {
        const float a = 6.2831853f * static_cast<float>(j) / beamSides;
        beam.around[j] = left * std::cos(a) + up * std::sin(a);
    }

    const BeamQuality& quality = beamQualities[CLAMP(0, static_cast<int>(vr_flashlight_beam_quality.value), 2)];
    const auto tracedSide = [&](int j) { return j % quality.sideStep == 0; };
    const auto tracedRing = [&](int i) { return i >= 1 && ((i - 1) % quality.ringStep == 0 || i == beamRings - 1); };
    const float lensR = lensRadius * units::worldScale();
    for(int i = 0; i < beamRings; i++)
    {
        // The rings closer together near the lens, where the most changes.
        const float t = static_cast<float>(i) / (beamRings - 1);
        const float d = length * t * t;
        beam.axis[i] = lens + dir * d;
        beam.radius[i] = lensR + d * spread;

        // The light thins as it spreads (less of it on each bit of air); the last third fades out.
        const float thin = 1.f / (1.f + d / (0.5f * m2u));
        const float end = glm::smoothstep(0.f, 1.f, std::min(1.f, (1.f - t * t) / 0.35f));
        beam.glow[i] = thin * end;

        // Where a wall cuts the cone, looked for a little past it (to fade out before a wall just
        // beyond the cone, too): on the traced rings and sides (the quality's), the other sides in between.
        if(i == 0)
        {
            std::fill(std::begin(beam.reach[i]), std::end(beam.reach[i]), beamLookPast);
            continue;
        }
        if(!tracedRing(i))
        {
            continue; // (below, once the next ring is done)
        }
        for(int j = 0; j < beamSides; j += quality.sideStep)
        {
            const glm::vec3 from = beam.axis[i];
            const glm::vec3 to = from + beam.around[j] * (beam.radius[i] * beamLookPast);
            BeamTrace& t = beamTraces[i][j];
            const float still = std::max(0.1f, 0.01f * glm::distance(from, to));
            if(!quality.reuse || t.time < 0.0 || realtime - t.time > reuseSeconds || realtime < t.time ||
               glm::distance(from, t.from) > still || glm::distance(to, t.to) > still)
            {
                t = {from, to, beamLookPast * worldtrace::line(from, to), realtime};
            }
            beam.reach[i][j] = t.reach;
        }
        for(int j = 0; j < beamSides; j++)
        {
            if(!tracedSide(j))
            {
                const int before = j - j % quality.sideStep, after = (before + quality.sideStep) % beamSides;
                beam.reach[i][j] = 0.5f * (beam.reach[i][before] + beam.reach[i][after]);
            }
        }
    }
    // The rings in between: the mean of the traced ones either side.
    for(int i = 1; i < beamRings; i++)
    {
        if(!tracedRing(i))
        {
            for(int j = 0; j < beamSides; j++)
            {
                beam.reach[i][j] = 0.5f * (beam.reach[i - 1][j] + beam.reach[i + 1][j]);
            }
        }
    }
}

// The beam: a spot light at the lens, lighting everything in its cone per pixel (the world, doors,
// monsters, pickups, hands), shadowed by its own perspective shadow map (vr_flashlight_shadows); a
// faint unshadowed spill cone round it, as a real torch's reflector gives; a faint glow at the lamp.
// No trace places the light, so nothing pops as the beam crosses an edge: the one trace left only
// sets how far the visible beam reaches, eased over time.
void lightBeam(const Pose& p)
{
    const glm::vec3 lens = modelPointAt(p, lensPoint);
    const glm::vec3 dir = p.rot * glm::vec3{1.f, 0.f, 0.f};
    const float range = std::max(64.f, vr_flashlight_range.value);
    const glm::vec3 at = lens + dir * 0.25f; // just out of the lens

    const float base = std::max(0.f, vr_flashlight_brightness.value) * 1.5f;
    const glm::vec3 warm = beamColor(); // (named for the warm white it was)

    lighting::dlightSpot(light(keySpot, at, range, warm * base), dir, innerAngle, outerAngle);
    lighting::dlightSpot(light(keySpill, at, range * 0.5f, warm * (base * 0.1f)), dir, outerAngle * 0.8f, spillAngle);
    light(keyLamp, lens + dir * (0.2f * units::metresToUnits()), 0.5f * units::metresToUnits(), warm * (base * 0.15f));

    // The visible beam's length: to where the beam lands (the world, doors and lifts; monsters too
    // when hosting), eased so that it does not jump as the beam crosses an edge (a stair's, a
    // doorway's); the beam's own traces cut it at the walls in the meantime.
    const glm::vec3 start = lens + dir * 0.5f;
    std::optional<trace_t> tr = worldtrace::move(start, glm::vec3{0.f}, glm::vec3{0.f}, lens + dir * range, MOVE_NORMAL);
    if(!tr)
    {
        tr = worldtrace::world(start, lens + dir * range);
    }
    const float target = 0.5f + (range - 0.5f) * tr->fraction;
    const float dt = static_cast<float>(std::clamp(realtime - st.beamTime, 0.0, 0.1));
    st.beamTime = realtime;
    st.beamLength = st.beamLength < 0.f ? target : st.beamLength + (target - st.beamLength) * (1.f - std::exp(-dt * 8.f));

    shapeBeam(p, lens, dir, st.beamLength, warm * std::max(0.f, vr_flashlight_brightness.value));
}

// The retracting cord from the clip on the belt to the lamp's bottom, while it is off the belt:
// taut, sagging a little when the lamp is close.
void drawCord(const Pose& mount, const Pose& lamp)
{
    const glm::vec3 a = modelPointAt(mount, capPoint);
    const glm::vec3 b = modelPointAt(lamp, capPoint);
    const float len = glm::distance(a, b);
    if(len < 0.5f)
    {
        return;
    }
    const float m2u = units::metresToUnits();
    const float sag = std::max(0.f, 0.06f * m2u - len * 0.08f);
    const glm::vec4 color{0.07f, 0.07f, 0.06f, 1.f};
    constexpr int segments = 8;
    glm::vec3 prev = a;
    for(int i = 1; i <= segments; i++)
    {
        const float t = static_cast<float>(i) / segments;
        const glm::vec3 p = glm::mix(a, b, t) - glm::vec3{0.f, 0.f, sag * 4.f * t * (1.f - t)};
        lines::line(prev, p, 0.004f * m2u, color, color);
        prev = p;
    }
}

// vr_show_flashlight_zones (round 21, the author's tuning notes): the reach zones drawn, green while in reach. A ball as
// three rings, filled faintly; not round the eyes themselves (the head's, from your own view: rings a few centimetres
// from the eye would be bands across it): those are seen in the body's preview (vr_body_debug 2, 3).
void zoneBall(const glm::vec3& centre, float radius, const glm::vec3& x, const glm::vec3& y, const glm::vec3& z, const glm::vec3& eye,
    const glm::vec4& color)
{
    if(glm::distance(eye, centre) < radius + 0.05f * units::metresToUnits())
    {
        return;
    }
    const float width = 0.003f * units::metresToUnits();
    constexpr int segments = 32;
    const glm::vec3 axes[3][2] = {{x, y}, {y, z}, {z, x}};
    for(const auto& ab : axes)
    {
        glm::vec3 prev = centre + ab[0] * radius;
        for(int i = 1; i <= segments; i++)
        {
            const float t = 6.2831853f * static_cast<float>(i) / segments;
            const glm::vec3 p = centre + (ab[0] * std::cos(t) + ab[1] * std::sin(t)) * radius;
            lines::line(prev, p, width, color, color);
            prev = p;
        }
    }
    lines::point(centre, 0.01f * units::metresToUnits(), color);
    lines::point(centre, 2.f * radius, glm::vec4{glm::vec3{color}, 0.12f});
}

// A capsule: its line, rings round both ends and the middle, four lines along it, and its ends' half rings.
void zoneCapsule(const glm::vec3& a, const glm::vec3& b, float radius, const glm::vec3& up, const glm::vec4& color)
{
    const float width = 0.003f * units::metresToUnits();
    const glm::vec3 along = glm::length(b - a) > 1e-3f ? glm::normalize(b - a) : glm::vec3{1.f, 0.f, 0.f};
    glm::vec3 u = up - along * glm::dot(up, along);
    u = glm::length(u) > 1e-3f ? glm::normalize(u) : glm::normalize(glm::cross(along, glm::vec3{0.f, 0.f, 1.f}) + glm::vec3{1e-3f, 0.f, 0.f});
    const glm::vec3 v = glm::cross(along, u);
    constexpr int segments = 32;
    const auto arc = [&](const glm::vec3& c, const glm::vec3& p, const glm::vec3& q, float from, float to) {
        glm::vec3 prev = c + (p * std::cos(from) + q * std::sin(from)) * radius;
        for(int i = 1; i <= segments; i++)
        {
            const float t = from + (to - from) * static_cast<float>(i) / segments;
            const glm::vec3 pt = c + (p * std::cos(t) + q * std::sin(t)) * radius;
            lines::line(prev, pt, width, color, color);
            prev = pt;
        }
    };
    constexpr float pi = 3.14159265f;
    for(const glm::vec3& c : {a, 0.5f * (a + b), b})
    {
        arc(c, u, v, 0.f, 2.f * pi);
    }
    for(const glm::vec3& side : {u, v, -u, -v})
    {
        lines::line(a + side * radius, b + side * radius, width, color, color);
    }
    arc(b, u, along, -0.5f * pi, 0.5f * pi); // the far end's cap
    arc(b, v, along, -0.5f * pi, 0.5f * pi);
    arc(a, u, -along, -0.5f * pi, 0.5f * pi); // the near end's
    arc(a, v, -along, -0.5f * pi, 0.5f * pi);
    lines::line(a, b, width, color, color);
}

// The head's zones and the held torch's middle, placed by `to` (the world, or the body's preview) and seen from `eye`;
// the guns' only in the world (the preview has no guns).
void drawZones(const hands::State& s, const glm::mat4& to, const glm::vec3& eye, bool guns)
{
    const glm::vec4 idle{1.f, 0.85f, 0.2f, 0.9f}, inReach{0.2f, 1.f, 0.3f, 1.f};
    const auto at = [&](const glm::vec3& p) { return glm::vec3{to * glm::vec4{p, 1.f}}; };
    const glm::mat3 turn{to};

    // The head's: the temples and the forehead (on the head: the temple on its side, where a hand takes it off).
    glm::vec3 fwd, right, up;
    hands::angleVectors(s.headAngles, fwd, right, up);
    fwd = turn * fwd;
    right = turn * right;
    up = turn * up;
    const HeadZone hz = headZone(s);
    const bool handAtHead = st.mode == Mode::OnHead && (st.hovered[0] || st.hovered[1]);
    for(int i = 0; i < 2; i++)
    {
        const float side = i == 0 ? -1.f : 1.f;
        if(st.mode == Mode::OnHead && side != st.headSide)
        {
            continue;
        }
        const bool lit = (st.nearHead && st.nearHeadSide == side) || handAtHead;
        zoneBall(at(hz.temple[i]), hz.radius, fwd, right, up, eye, lit ? inReach : idle);
    }
    if(st.mode != Mode::OnHead)
    {
        zoneBall(at(hz.forehead), hz.radius, fwd, right, up, eye, st.nearHead ? inReach : idle);
    }

    // The guns': round each gun in a hand, while the torch is off it (the one the held torch would clip on green).
    for(int hand = 0; guns && hand < 2; hand++)
    {
        view::WeaponMount m;
        if(st.mode == Mode::OnGun || !view::weaponMount(hand, m))
        {
            continue;
        }
        glm::vec3 gf, gr, gu;
        hands::angleVectors(m.rot, gf, gr, gu);
        const GunZone z = gunZone(m);
        const bool lit = st.mode == Mode::Held && st.nearGun && hand == 1 - st.holder;
        zoneCapsule(z.a, z.b, z.radius, gu, lit ? inReach : glm::vec4{1.f, 0.55f, 0.15f, 0.9f});
    }

    // The held torch's middle: what the zones measure (green in reach of one).
    if(st.mode == Mode::Held)
    {
        lines::point(at(modelPointAt(st.pose, glm::vec3{0.f})), 0.015f * units::metresToUnits(),
            st.nearGun || st.nearHead ? inReach : glm::vec4{1.f, 1.f, 1.f, 1.f});
    }
}

void place(view::ViewEntity& ve, const Pose& p, bool hover)
{
    entity_t& e = ve.ent;
    qmodel_t* model = view::viewModel(modelName);
    if(model != ve.lastModel)
    {
        e.lerpflags |= LERP_RESETANIM;
        ve.lastModel = model;
    }
    e.model = model;
    e.frame = 0;
    e.skinnum = 0; // (the lens's glow: drawLens, in the beam's colour)
    e.colormap = vid.colormap;
    e.alpha = ENTALPHA_DEFAULT;
    e.scale = static_cast<unsigned char>(CLAMP(1.f, units::worldScale() * ENTSCALE_DEFAULT + 0.5f, 255.f));

    const glm::mat3 m = glm::mat3_cast(p.rot);
    const glm::vec3 a = hands::anglesFromVectors(m[0], m[2]);
    const glm::vec3 angles{-a.x, a.y, a.z};
    for(int i = 0; i < 3; i++)
    {
        e.origin[i] = p.pos[i];
        e.angles[i] = angles[i];
    }
    ve.mirrored = false;
    ve.zeroBlend = 0.f;
    ve.visible = model != nullptr;
    ve.lightMultiply = hover;
    ve.lightMod = glm::vec3{hover ? 2.2f : 1.f};
}

void toggle_f()
{
    if(vr_flashlight.value != 0.f)
    {
        toggle(-1);
    }
}


} // namespace

void init()
{
    Cmd_AddCommand("vr_flashlight_toggle", toggle_f);
}

void setupView(const hands::State& s, view::ViewEntity& ve)
{
    static int lastFrame = -1;
    if(lastFrame == host_framecount)
    {
        return; // once per frame, however often the view is set up
    }
    lastFrame = host_framecount;
    noteIntent(s);

    // A new map: back on the belt (switched as it was).
    static int generation = -1;
    if(cl.worldmodel != st.world || worldGeneration() != generation)
    {
        st.world = cl.worldmodel;
        generation = worldGeneration();
        st.mode = Mode::Mounted;
        st.holder = -1;
        st.gunHand = -1;
        st.placed = false;
    }

    const bool alive = cl.stats[STAT_HEALTH] > 0 && !cl.intermission;
    if(!enabled() || !s.valid || !alive)
    {
        ve.visible = false;
        st.placed = false;
        st.mode = Mode::Mounted;
        st.holder = -1;
        st.gunHand = -1;
        st.nearGun = false;
        st.nearHead = false;
        st.hovered[0] = st.hovered[1] = false;
        killLights();
        return;
    }

    const Pose mount = mountPose(s);

    // The holding hand took a weapon (a pickup): the lamp goes home.
    if(st.mode == Mode::Held && (st.holder < 0 || !handEmpty(st.holder)))
    {
        letGo(s, mount);
    }

    // On a gun that left the hand (holstered, dropped, thrown, switched for another; not its other
    // ammo): back to the belt.
    view::WeaponMount gun;
    if(st.mode == Mode::OnGun &&
        (st.gunHand < 0 || !view::weaponMount(st.gunHand, gun) || !view::sameGun(gun.model, st.gunModel)))
    {
        clipOff(s, -1);
    }

    Pose p = mount;
    if(st.mode == Mode::Held)
    {
        p = handPose(s, st.holder);

        // Held near the gun in the other hand: a tap, the lamp lit up; B/Y clips it on.
        view::WeaponMount other;
        bool inReach = false;
        if(key_dest == key_game && view::weaponMount(1 - st.holder, other))
        {
            const GunZone z = gunZone(other);
            inReach = gunDistance(p, z) < z.radius;
        }
        if(inReach && !st.nearGun)
        {
            haptic(st.holder, 0.015f, 0.25f);
        }
        st.nearGun = inReach;

        // Held at the head (not by a gun): a tap, the lamp lit up; B/Y clips it on there.
        float side = -1.f;
        const bool atHead = key_dest == key_game && !inReach && headDistance(s, p, side) < headZone(s).radius;
        if(atHead && !st.nearHead)
        {
            haptic(st.holder, 0.015f, 0.25f);
        }
        st.nearHead = atHead;
        st.nearHeadSide = side;
    }
    else
    {
        st.nearGun = false;
        st.nearHead = false;
    }
    if(st.mode == Mode::OnGun)
    {
        if(gun.model != st.gunModel)
        {
            st.gunModel = gun.model; // the other ammo's model, after its button
            st.gunSpot = findGunSpot(gun);
        }
        p = gunPose(gun);
    }
    else if(st.mode == Mode::OnHead)
    {
        p = headPose(s, st.headSide);
    }
    else if(st.mode == Mode::Returning)
    {
        // Critically damped: x(t) = (x0 + (v0 + w x0) t) e^(-w t), relative to the (moving) mount.
        const float w = returnOmega;
        const float t = static_cast<float>(realtime - st.flightStart);
        const float decay = std::exp(-w * t);
        const glm::vec3 x = (st.flightOffset + (st.flightVel + w * st.flightOffset) * t) * decay;
        p.pos = mount.pos + x;
        p.rot = glm::slerp(st.flightRot, mount.rot, 1.f - (1.f + w * t) * decay);
        if(t > 7.f / w)
        {
            st.mode = Mode::Mounted;
            p = mount;
        }
    }
    st.pose = p;
    st.placed = true;

    // A hand at the lamp lights it up (and taps, once); on a gun, only the free hand.
    bool hover = st.nearGun || st.nearHead;
    for(int hand = 0; hand < 2; hand++)
    {
        const bool atLamp = st.mode != Mode::Held && hand != st.gunHand && key_dest == key_game && handAt(s, hand);
        if(atLamp && !st.hovered[hand])
        {
            haptic(hand, 0.015f, 0.2f);
        }
        st.hovered[hand] = atLamp;
        hover = hover || atLamp;
    }

    const glm::vec3 eyes = 0.5f * (s.eyeOrigin[0] + s.eyeOrigin[1]); // where the zones are seen from (the mock's camera too)

    // The body's preview (vr_body_debug 2 and 3: in front of the player, turned) carries it too.
    Pose drawn = p;
    Pose drawnMount = mount;
    if(vr_body_debug.value >= 2.f)
    {
        const glm::vec3 root{s.head.x, s.head.y, 0.f};
        const glm::vec3 centre = root + hands::forward({0.f, s.bodyYaw, 0.f}) * (1.8f * bodyUnits());
        const glm::quat turn = glm::angleAxis(glm::radians(vr_body_debug.value >= 3.f ? -90.f : 180.f), glm::vec3{0.f, 0.f, 1.f});
        for(Pose* q : {&drawn, &drawnMount})
        {
            q->pos = centre + turn * (q->pos - root);
            q->rot = turn * q->rot;
        }
        if(vr_show_flashlight_zones.value)
        {
            // The head's zones round the preview's head too (from your own view they are round your eyes).
            const glm::mat4 preview = glm::translate(glm::mat4{1.f}, centre) * glm::mat4_cast(turn) * glm::translate(glm::mat4{1.f}, -root);
            drawZones(s, preview, eyes, false);
        }
    }
    place(ve, drawn, hover);
    if(vr_show_flashlight_zones.value)
    {
        drawZones(s, glm::mat4{1.f}, eyes, true);
    }
    if(st.mode != Mode::Mounted && st.mode != Mode::OnHead && vr_flashlight_cord.value != 0.f) // (on the head, the cord runs behind the neck)
    {
        drawCord(drawnMount, drawn);
    }

    if(st.on)
    {
        lightBeam(p);
    }
    else
    {
        killLights();
    }
}

// The lens lit, in the beam's colour: a disc over it, bright in the middle, added onto the scene (the skin's own
// fullbright lens was one colour).
void drawLens()
{
    if(!st.on || !st.placed || !enabled())
    {
        return;
    }
    const Pose& p = st.pose;
    const glm::vec3 dir = p.rot * glm::vec3{1.f, 0.f, 0.f};
    const glm::vec3 a = p.rot * glm::vec3{0.f, 1.f, 0.f};
    const glm::vec3 b = p.rot * glm::vec3{0.f, 0.f, 1.f};
    const glm::vec3 centre = modelPointAt(p, lensPoint) + dir * (0.02f * units::worldScale());
    const float r = lensRadius * units::worldScale();
    const glm::vec3 c = beamColor() * (0.6f + 0.4f * CLAMP(0.f, vr_flashlight_brightness.value, 2.f));
    constexpr int sides = 16;
    static std::vector<gfx::Vertex> fan;
    fan.clear();
    for(int i = 0; i < sides; i++)
    {
        const float t0 = 6.2831853f * static_cast<float>(i) / sides, t1 = 6.2831853f * static_cast<float>(i + 1) / sides;
        const gfx::Vertex m{centre, glm::vec2{0.f}, glm::vec4{c * 0.9f, 0.f}};
        const gfx::Vertex e0{centre + (a * std::cos(t0) + b * std::sin(t0)) * r, glm::vec2{0.f}, glm::vec4{c * 0.35f, 0.f}};
        const gfx::Vertex e1{centre + (a * std::cos(t1) + b * std::sin(t1)) * r, glm::vec2{0.f}, glm::vec4{c * 0.35f, 0.f}};
        fan.insert(fan.end(), {m, e0, e1, m, e1, e0}); // both faces (drawn either way round)
    }
    gfx::State state;
    state.shade = gfx::Shade::Color;
    state.blend = gfx::Blend::Premultiplied;
    state.depthTest = true;
    state.depthWrite = false;
    gfx::draw(fan, gfx::sceneViewProjection(), state);
}

void drawTranslucent()
{
    drawLens();
    if(!beam.visible || !st.on || !enabled())
    {
        return;
    }

    glm::vec3 eye, right, up;
    gfx::sceneCamera(eye, right, up);
    const float m2u = units::metresToUnits();

    // The outer cone, and the core: its size of the outer's, its slope, its share of the light.
    struct Shell
    {
        float scale;
        float slope;
        float weight;
    };
    const Shell shells[] = {{1.f, spread, 0.5f}, {coreSpread / spread, coreSpread, 1.f}};

    static std::vector<gfx::Vertex> triangles;
    triangles.clear();
    gfx::Vertex grid[beamRings][beamSides];
    for(const Shell& shell : shells)
    {
        for(int i = 0; i < beamRings; i++)
        {
            for(int j = 0; j < beamSides; j++)
            {
                const float reach = beam.reach[i][j];
                const glm::vec3 pos = beam.axis[i] + beam.around[j] * (beam.radius[i] * std::min(shell.scale, reach));

                // Dark where a wall cuts it, fading in away from the wall.
                const float open = CLAMP(0.f, (reach - shell.scale) / (beamLookPast - 1.f), 1.f);

                // The cone seen face-on stands for a long way through the lit air, bright; seen
                // edge-on, for none: a soft volume rather than a shell with edges (squared, for a
                // beam brighter in its middle and soft at its edges).
                const glm::vec3 normal = glm::normalize(beam.around[j] - beam.dir * shell.slope);
                const glm::vec3 toPoint = pos - eye;
                const float away = glm::length(toPoint);
                const float cosine = away > 0.01f ? std::abs(glm::dot(normal, toPoint)) / away : 0.f;
                const float facing = cosine * cosine;

                // None right at the eye (the lamp held up to the face).
                const float atEye = glm::smoothstep(0.1f * m2u, 0.4f * m2u, away);

                const float k = shell.weight * beam.glow[i] * open * facing * atEye;
                // Added onto the scene (premultiplied, no alpha): the colour is the light added.
                grid[i][j] = {pos, glm::vec2{0.f}, glm::vec4{beam.color * k, 0.f}};
            }
        }
        for(int i = 0; i + 1 < beamRings; i++)
        {
            for(int j = 0; j < beamSides; j++)
            {
                const int jn = (j + 1) % beamSides;
                const gfx::Vertex& a = grid[i][j];
                const gfx::Vertex& b = grid[i][jn];
                const gfx::Vertex& c = grid[i + 1][jn];
                const gfx::Vertex& d = grid[i + 1][j];
                triangles.insert(triangles.end(), {a, b, c, a, c, d});
            }
        }
    }

    gfx::State state;
    state.shade = gfx::Shade::Color;
    state.blend = gfx::Blend::Premultiplied;
    state.depthTest = true;
    state.depthWrite = false;
    gfx::draw(triangles, gfx::sceneViewProjection(), state);
}

bool button(int hand, Button b, bool pressed)
{
    if(hand < 0 || hand > 1)
    {
        return false;
    }
    const bool grip = b == Button::Grip;
    if(grip)
    {
        st.gripDown[hand] = pressed;
    }
    bool& swallowed = st.swallowed[hand][static_cast<int>(b)];

    if(!pressed)
    {
        if(!swallowed)
        {
            return false;
        }
        swallowed = false;
        if(grip && st.mode == Mode::Held && st.holder == hand)
        {
            const hands::State& s = hands::current();
            letGo(s, mountPose(s));
        }
        return true;
    }

    if(!enabled() || key_dest != key_game || !st.placed)
    {
        return false;
    }

    const hands::State& s = hands::current();
    const bool holding = st.mode == Mode::Held && st.holder == hand;
    bool atLamp = st.mode != Mode::Held && hand != st.gunHand && handAt(s, hand);
    if(atLamp && (st.mode == Mode::Mounted || st.mode == Mode::Returning) && gameGripWins(s, hand))
    {
        atLamp = false; // a two-handed grip, a draw from the holster next to it
    }

    if(b == Button::Secondary)
    {
        // Held near the other hand's gun: either hand's B/Y clips it on.
        view::WeaponMount gun;
        if(st.mode == Mode::Held && st.nearGun && view::weaponMount(1 - st.holder, gun))
        {
            clipOn(1 - st.holder, gun);
            swallowed = true;
            return true;
        }
        // Held at the head: either hand's B/Y clips it on there (a head torch).
        if(st.mode == Mode::Held && st.nearHead)
        {
            clipOnHead(st.nearHeadSide);
            swallowed = true;
            return true;
        }
        // Held elsewhere: the holding hand's B/Y flips the grip (low / overhead).
        if(holding)
        {
            flip(hand);
            swallowed = true;
            return true;
        }
        // On a gun: the free hand at the lamp takes it off with its B/Y (or with the gun hand's while
        // it grips the lamp). Gripping, the lamp goes into it; otherwise back to the belt.
        if(st.mode == Mode::OnGun && st.gunHand >= 0)
        {
            const int freeHand = 1 - st.gunHand;
            if(handNear(s, freeHand) && (hand == freeHand || st.gripDown[freeHand]) && deliberate(hand, b))
            {
                const bool into = st.gripDown[freeHand] && handEmpty(freeHand);
                clipOff(s, into ? freeHand : -1);
                if(into)
                {
                    // The lamp has the grip now: its release sends it home. If the game saw the press
                    // (a hand on the gun's foregrip), it must see the grip let go.
                    bool& gripSwallowed = st.swallowed[freeHand][static_cast<int>(Button::Grip)];
                    st.tookGrip[freeHand] = !gripSwallowed;
                    gripSwallowed = true;
                }
                swallowed = true;
                return true;
            }
        }
        // On the head: a hand at the lamp takes it off with its B/Y, as from a gun: gripping, into that hand;
        // otherwise back to the belt.
        if(st.mode == Mode::OnHead && atLamp && deliberate(hand, b))
        {
            const bool into = st.gripDown[hand] && handEmpty(hand);
            clipOffHead(s, into ? hand : -1);
            if(into)
            {
                bool& gripSwallowed = st.swallowed[hand][static_cast<int>(Button::Grip)];
                st.tookGrip[hand] = !gripSwallowed;
                gripSwallowed = true;
            }
            swallowed = true;
            return true;
        }
        return false;
    }

    if(!grip && (holding || (atLamp && deliberate(hand, b))))
    {
        toggle(hand);
        swallowed = true;
        return true;
    }
    // (On a gun, a grip at the lamp is the game's: the foregrip is near. B/Y takes it off.) On the head, a grip at it
    // takes it off into the hand.
    if(grip && atLamp && st.mode != Mode::OnGun && handEmpty(hand) && deliberate(hand, b))
    {
        if(st.mode == Mode::OnHead)
        {
            clipOffHead(hands::current(), hand);
            swallowed = true;
            return true;
        }
        take(hand);
        swallowed = true;
        return true;
    }
    return false;
}

bool tookGrip(int hand)
{
    if(hand < 0 || hand > 1 || !st.tookGrip[hand])
    {
        return false;
    }
    st.tookGrip[hand] = false;
    return true;
}

void reset()
{
    Con_DPrintf("flashlight: reset (a fresh start)\n");
    st.on = false;
    st.mode = Mode::Mounted;
    st.holder = -1;
    st.gunHand = -1;
    st.gunModel = nullptr;
    st.nearGun = false;
    st.nearHead = false;
    st.placed = false;
    killLights();
}

void onGameDirChanged()
{
    gunSpots.clear(); // keyed by model name: another game's model of the same name may differ
}

bool holds(int hand)
{
    return enabled() && st.mode == Mode::Held && st.holder == hand;
}

bool heldPlace(const hands::State& s, int hand, glm::vec3& origin, glm::vec3& angles)
{
    if(!holds(hand))
    {
        return false;
    }
    // While it spins over, the grasp holds the grip it had (solved once, not chased through the turn); at the end
    // it is solved for the new one and blends to it (vr_hand_fit_blend).
    const bool spinning = realtime - st.flipAt[hand] < flipTime;
    const Pose p = spinning ? handPoseTurned(s, hand, st.overhead[hand] ? 0.f : 1.f) : handPose(s, hand);
    const glm::mat3 m = glm::mat3_cast(p.rot);
    const glm::vec3 a = hands::anglesFromVectors(m[0], m[2]);
    origin = p.pos;
    angles = glm::vec3{-a.x, a.y, a.z};
    return true;
}

bool fingers(int hand, Fingers& out)
{
    if(!holds(hand))
    {
        return false;
    }
    // The grip the grasp is solved for (heldPlace: while it spins over, the one it had) sets the fingers; the tweaks and
    // the thumb's place go from one grip's to the other's as it turns, as the curls ease (vr_finger_blending_speed).
    const bool spinning = realtime - st.flipAt[hand] < flipTime;
    const bool overhead = spinning ? !st.overhead[hand] : st.overhead[hand];
    out = gripFingers(overhead);
    const float turned = turnedAt(hand, realtime);
    const Fingers lo = gripFingers(false), hi = gripFingers(true);
    for(int f = 0; f < 5; f++)
    {
        out.bias[f] = glm::mix(lo.bias[f], hi.bias[f], turned);
    }
    out.thumb = glm::mix(lo.thumb, hi.thumb, turned);
    return true;
}

bool wantsSecondary(int hand)
{
    return holds(hand) || (enabled() && handAtHeadTorch(hands::current(), hand));
}


} // namespace qvr::flashlight

// A new game, a map started afresh or a save loaded (host_cmd.c), not a changelevel: the
// flashlight off, on the belt (in the game, it stays as it was from level to level).
extern "C" void VR_OnFreshStart()
{
    qvr::flashlight::reset();
}
