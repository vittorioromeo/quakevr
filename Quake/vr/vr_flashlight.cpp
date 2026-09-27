// vr_flashlight.cpp -- see vr_flashlight.hpp.

#include "vr_flashlight.hpp"
#include "vr_avatar.hpp"
#include "vr_cvars.hpp"
#include "vr_gfx.hpp"
#include "vr_lighting.hpp"
#include "vr_lines.hpp"
#include "vr_main.hpp"
#include "vr_trace.hpp"
#include "vr_units.hpp"
#include "vr_weapons.hpp"

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
constexpr float gunReach = 0.12f;      // metres from the gun (its line from the hand to the muzzle) it clips on at
constexpr float flipTime = 0.16f;      // seconds the torch takes to turn round in the fist

// The fist's hole, where the grip's middle goes, from the tracked hand as vr_flashlight_hand_forward and _up leave it
// (metres): towards the back of the hand and down along the fist, found by fitting the drawn fist (the fingers' curls
// held open as below) round the tube: the tube centred along the four fingers and through their curl, clear of the
// palm. And how far open the fingers stay round the tube (the grip limits, 0..1): the four fingers, the thumb.
constexpr float gripBack = 0.025f;
constexpr float gripUp = -0.03f;
constexpr float fingersOpen = 0.45f;
constexpr float thumbOpen = 0.3f;

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
    OnGun // clipped under the barrel of the gun in st.gunHand
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

    // Round 21: each hand's grip, flipped with its B/Y while held away from a gun: the low grip (false: the beam out of
    // the thumb's side) or the overhead one (true: out of the little finger's side); kept for the next time that hand
    // takes it. The flip turns the torch round in the fist over flipTime (flipAt: when it began).
    bool overhead[2]{};
    double flipAt[2]{-10.0, -10.0};

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

// Clipped to the chest on the off hand's side, pointing forward, its tail just in front of the chest where the cord
// comes out of the clip (vr_flashlight_forward, _up and _out move it), the beam where the torso faces, tilted down by
// vr_flashlight_tilt, its switch up.
[[nodiscard]] Pose mountPose(const hands::State& s)
{
    const avatar::Torso torso = avatar::torso(s);
    const glm::vec3 up = torso.chest.rot[0];
    const glm::vec3 fwd = torso.chest.rot[2];
    const glm::vec3 left = glm::cross(up, fwd);
    const float side = vr_lefthanded.value != 0.f ? -1.f : 1.f;

    // The chest's front where the clip is (make_vrbody.py's torso rings, 6 cm above the chest joint
    // and 8.5 cm to the side): deeper for the brawnier builds; the tail 1.2 cm in front of it.
    const int build = static_cast<int>(vr_body_build.value);
    const float depth = build <= 0 ? 0.9f : build >= 2 ? 1.1f : 1.f;
    const float m2w = bodyUnits();
    const glm::vec3 clip = torso.chest.pos + (fwd * (0.125f * depth + 0.012f + vr_flashlight_forward.value) +
                                                 left * (side * (0.085f + vr_flashlight_out.value)) +
                                                 up * (0.06f + vr_flashlight_up.value)) *
                                                 m2w;

    const float tilt = glm::radians(CLAMP(-45.f, vr_flashlight_tilt.value, 60.f));
    const glm::vec3 beam = fwd * std::cos(tilt) - up * std::sin(tilt);
    const glm::vec3 beamUp = up * std::cos(tilt) + fwd * std::sin(tilt);
    return poseFromAxes(clip + beam * (tailLength * units::metresToUnits()), beam, left, beamUp);
}

// In the hand, held like a torch (round 21; before, like a pistol's grip): the tube through the curled fingers, along
// the fist's axis (the hand's up, as a pistol's grip is), the switch towards the knuckles. In the low grip the head is
// out past the thumb and the index finger and the beam goes that way: the hand is pitched forward a quarter turn from
// a pistol's aim to light ahead. In the overhead grip (B/Y flips it) the torch is the other way round in the same
// fist, the beam out of the little finger's side: the fist raised by the head, the thumb towards the face, lights
// ahead. The tracked hand is ahead of and above the drawn fist: vr_flashlight_hand_forward and _up move the grip's
// middle back and down into it.
[[nodiscard]] Pose handPose(const hands::State& s, int hand)
{
    glm::vec3 fwd, right, up;
    hands::angleVectors(s.rot[hand], fwd, right, up);
    const float m2u = units::metresToUnits();
    const float back = hand == HAND_OFF ? -1.f : 1.f; // the back of the hand's side (the palm faces the other way)
    const glm::vec3 offset = fwd * vr_flashlight_hand_forward.value + up * (vr_flashlight_hand_up.value + gripUp) +
                             right * (back * gripBack);
    Pose p = poseFromAxes(s.pos[hand] + offset * m2u, up, right, fwd);

    // Flipped: half a turn about the knuckles' way (the switch stays towards them), eased over flipTime.
    const float k = std::clamp(static_cast<float>(realtime - st.flipAt[hand]) / flipTime, 0.f, 1.f);
    const float turned = st.overhead[hand] ? k : 1.f - k; // 0 low, 1 overhead
    const float eased = turned * turned * (3.f - 2.f * turned);
    p.rot = glm::normalize(p.rot * glm::angleAxis(eased * 3.14159265f, glm::vec3{0.f, 0.f, 1.f}));
    return p;
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
    static std::unordered_map<std::string, GunSpot> cache;
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

// How far (world units) the lamp's middle is from the gun: from its line from the hand to a little
// past the muzzle.
[[nodiscard]] float gunDistance(const Pose& lamp, const view::WeaponMount& m)
{
    const glm::vec3 middle = modelPointAt(lamp, glm::vec3{0.f});
    const glm::vec3 a = m.pos;
    const glm::vec3 ab = m.muzzle - a;
    const float len2 = glm::dot(ab, ab);
    const float tMax = 1.f + (len2 > 1e-4f ? 0.03f * units::metresToUnits() / std::sqrt(len2) : 0.f);
    const float t = len2 > 1e-4f ? std::clamp(glm::dot(middle - a, ab) / len2, 0.f, tMax) : 0.f;
    return glm::distance(middle, a + ab * t);
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
    st.overhead[hand] = !st.overhead[hand];
    st.flipAt[hand] = realtime;
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

// Off the gun: into `hand` (its grip held at the lamp), or else back to the chest on its cord.
void clipOff(const hands::State& s, int hand)
{
    const int gunHand = st.gunHand;
    st.gunHand = -1;
    st.gunModel = nullptr;
    Con_DPrintf("flashlight: off the gun, %s\n", hand >= 0 ? "into the other hand" : "back to the chest");
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
        // beyond the cone, too).
        for(int j = 0; j < beamSides; j++)
        {
            beam.reach[i][j] =
                i == 0 ? beamLookPast : beamLookPast * worldtrace::line(beam.axis[i], beam.axis[i] + beam.around[j] * (beam.radius[i] * beamLookPast));
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
    const glm::vec3 warm{1.f, 0.94f, 0.82f};

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

// The retracting cord from the clip on the chest to the lamp's bottom, while it is off the chest:
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

void place(view::ViewEntity& ve, const Pose& p, bool hover)
{
    entity_t& e = ve.ent;
    qmodel_t* model = Mod_ForName(modelName, false);
    if(model != ve.lastModel)
    {
        e.lerpflags |= LERP_RESETANIM;
        ve.lastModel = model;
    }
    e.model = model;
    e.frame = 0;
    e.skinnum = st.on ? 1 : 0;
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

    // A new map: back on the chest (switched as it was).
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
    // ammo): back to the chest.
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
        const bool inReach = key_dest == key_game && view::weaponMount(1 - st.holder, other) &&
                          gunDistance(p, other) < gunReach * units::metresToUnits();
        if(inReach && !st.nearGun)
        {
            haptic(st.holder, 0.015f, 0.25f);
        }
        st.nearGun = inReach;
    }
    else
    {
        st.nearGun = false;
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
    bool hover = st.nearGun;
    for(int hand = 0; hand < 2; hand++)
    {
        const bool atLamp = st.mode != Mode::Held && hand != st.gunHand && key_dest == key_game && handNear(s, hand);
        if(atLamp && !st.hovered[hand])
        {
            haptic(hand, 0.015f, 0.2f);
        }
        st.hovered[hand] = atLamp;
        hover = hover || atLamp;
    }

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
    }
    place(ve, drawn, hover);
    if(st.mode != Mode::Mounted)
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

void drawTranslucent()
{
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
    const bool atLamp = st.mode != Mode::Held && hand != st.gunHand && handNear(s, hand);

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
        // Held away from a gun: the holding hand's B/Y flips the grip (low / overhead).
        if(holding)
        {
            flip(hand);
            swallowed = true;
            return true;
        }
        // On a gun: the free hand at the lamp takes it off with its B/Y (or with the gun hand's while
        // it grips the lamp). Gripping, the lamp goes into it; otherwise back to the chest.
        if(st.mode == Mode::OnGun && st.gunHand >= 0)
        {
            const int freeHand = 1 - st.gunHand;
            if(handNear(s, freeHand) && (hand == freeHand || st.gripDown[freeHand]))
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
        return false;
    }

    if(!grip && (holding || atLamp))
    {
        toggle(hand);
        swallowed = true;
        return true;
    }
    // (On a gun, a grip at the lamp is the game's: the foregrip is near. B/Y takes it off.)
    if(grip && atLamp && st.mode != Mode::OnGun && handEmpty(hand))
    {
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
    st.placed = false;
    killLights();
}

bool holds(int hand)
{
    return enabled() && st.mode == Mode::Held && st.holder == hand;
}

float fingerOpen(int hand, int finger)
{
    if(!holds(hand))
    {
        return -1.f;
    }
    return finger == 1 ? thumbOpen : fingersOpen;
}

} // namespace qvr::flashlight

// A new game, a map started afresh or a save loaded (host_cmd.c), not a changelevel: the
// flashlight off, on the chest (in the game, it stays as it was from level to level).
extern "C" void VR_OnFreshStart()
{
    qvr::flashlight::reset();
}
