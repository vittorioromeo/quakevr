// vr_flashlight.cpp -- see vr_flashlight.hpp.

#include "vr_flashlight.hpp"
#include "vr_avatar.hpp"
#include "vr_cvars.hpp"
#include "vr_lighting.hpp"
#include "vr_lines.hpp"
#include "vr_main.hpp"
#include "vr_trace.hpp"
#include "vr_units.hpp"
#include "vr_weapons.hpp"

#include <algorithm>
#include <cmath>

namespace qvr::flashlight
{
namespace
{

// progs/vrflashlight.mdl (make_flashlight.py): model units at vr_world_scale 1, +x the beam, +z up
// the body; the origin in the middle of the body.
constexpr const char* modelName = "progs/vrflashlight.mdl";
constexpr glm::vec3 lensPoint{0.997f, 0.f, 1.234f};
constexpr glm::vec3 capPoint{0.f, 0.f, -1.05f}; // the bottom of the body, where the cord goes in

constexpr float reach = 0.11f;         // metres from the lamp's middle a hand reaches it at
constexpr float returnOmega = 14.f;    // the cord's pull (critically damped; home in about 0.4 s)
constexpr float maxThrow = 3.f;        // metres per second the lamp keeps of the hand's at a release

// The dynamic lights' keys (entities' keys are their numbers, never negative).
constexpr int keyPool = -0x0F1A51;
constexpr int keyMid = -0x0F1A52;
constexpr int keyLamp = -0x0F1A53;

enum class Mode
{
    Mounted,
    Held,
    Returning
};

struct Pose
{
    glm::vec3 pos{0.f};
    glm::quat rot{1.f, 0.f, 0.f, 0.f}; // model axes to the world: x the beam, y left, z up
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

    bool swallowed[2][2]{}; // [hand][grip]: a press the flashlight took, whose release it takes too
    bool hovered[2]{};
    const qmodel_t* world{nullptr};
};

State st;

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

// Clipped to the chest on the off hand's side, its back against the chest (vr_flashlight_forward,
// _up and _out move it), the beam where the torso faces, tilted down by vr_flashlight_tilt.
[[nodiscard]] Pose mountPose(const hands::State& s)
{
    const avatar::Torso torso = avatar::torso(s);
    const glm::vec3 up = torso.chest.rot[0];
    const glm::vec3 fwd = torso.chest.rot[2];
    const glm::vec3 left = glm::cross(up, fwd);
    const float side = vr_lefthanded.value != 0.f ? -1.f : 1.f;

    // The chest's front where the lamp is (make_vrbody.py's torso rings, 6 cm above the chest joint
    // and 8.5 cm to the side): deeper for the brawnier builds; the lamp's back 2 cm in front of it.
    const int build = static_cast<int>(vr_body_build.value);
    const float depth = build <= 0 ? 0.9f : build >= 2 ? 1.1f : 1.f;
    const float m2w = bodyUnits();
    const glm::vec3 pos = torso.chest.pos + (fwd * (0.125f * depth + 0.021f + vr_flashlight_forward.value) +
                                                left * (side * (0.085f + vr_flashlight_out.value)) +
                                                up * (0.06f + vr_flashlight_up.value)) *
                                                m2w;

    const float tilt = glm::radians(CLAMP(-45.f, vr_flashlight_tilt.value, 60.f));
    const glm::vec3 beam = fwd * std::cos(tilt) - up * std::sin(tilt);
    const glm::vec3 beamUp = up * std::cos(tilt) + fwd * std::sin(tilt);
    return poseFromAxes(pos, beam, left, beamUp);
}

// In the hand, held like a pistol's grip: the body in the fist, the beam where the hand points.
[[nodiscard]] Pose handPose(const hands::State& s, int hand)
{
    glm::vec3 fwd, right, up;
    hands::angleVectors(s.rot[hand], fwd, right, up);
    const float m2u = units::metresToUnits();
    return poseFromAxes(s.pos[hand] + (fwd * 0.005f - up * 0.01f) * m2u, fwd, -right, up);
}

// Whether a hand holds nothing (the "fist" or no weapon at all).
[[nodiscard]] bool handEmpty(int hand)
{
    const int slot = weapons::heldSlot(hand);
    return slot < 0 || slot == weapons::fistSlot();
}

[[nodiscard]] bool handNear(const hands::State& s, int hand)
{
    if(!st.placed || !s.valid)
    {
        return false;
    }
    const glm::vec3 middle = modelPointAt(st.pose, glm::vec3{0.f, 0.f, 0.3f});
    return glm::distance(s.pos[hand], middle) < reach * units::metresToUnits();
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
    S_LocalSound(st.on ? "vr/flashlight_on.wav" : "vr/flashlight_off.wav");
    if(hand >= 0)
    {
        haptic(hand, 0.03f, 0.55f);
    }
}

void take(int hand)
{
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
    for(int key : {keyPool, keyMid, keyLamp})
    {
        killLight(key);
    }
}

// Only the pool of light may cast shadows, with vr_flashlight_shadows.
void light(int key, const glm::vec3& at, float radius, const glm::vec3& color)
{
    dlight_t* dl = CL_AllocDlight(key);
    dl->origin[0] = at.x;
    dl->origin[1] = at.y;
    dl->origin[2] = at.z;
    dl->radius = radius;
    dl->minlight = 0.f;
    dl->die = static_cast<float>(cl.time) + 0.1f;
    dl->color[0] = color.x;
    dl->color[1] = color.y;
    dl->color[2] = color.z;
    lighting::dlightLook(dl, 0.f, 0.f); // no share for what faces away, and no DarkPlaces boost
    if(key != keyPool || !vr_flashlight_shadows.value)
    {
        lighting::dlightNoShadow(dl);
    }
}

// The beam: where it lands (the world, doors and lifts; monsters too when hosting), a pool of
// light there -- the light off the surface along its normal, which makes a round pool, and a little
// back along the beam -- growing and dimming with the distance; a dimmer, wider light halfway, for
// the beam's spill on what it passes; a faint glow at the lamp.
void lightBeam(const Pose& p)
{
    const glm::vec3 lens = modelPointAt(p, lensPoint);
    const glm::vec3 dir = p.rot * glm::vec3{1.f, 0.f, 0.f};
    const float range = std::max(64.f, vr_flashlight_range.value);
    const glm::vec3 start = lens + dir * 0.5f;
    const glm::vec3 end = lens + dir * range;

    std::optional<trace_t> tr = worldtrace::move(start, glm::vec3{0.f}, glm::vec3{0.f}, end, MOVE_NORMAL);
    if(!tr)
    {
        tr = worldtrace::world(start, end);
    }
    const float fraction = tr->fraction;
    const float dist = 0.5f + (range - 0.5f) * fraction;
    const glm::vec3 hit = lens + dir * dist;
    glm::vec3 normal = worldtrace::normal(*tr);
    if(glm::dot(normal, normal) < 0.5f)
    {
        normal = -dir;
    }

    // DarkPlaces' falloff keeps a light full out to about 40% of its radius, Quake's is linear:
    // the latter needs more.
    const float base = std::max(0.f, vr_flashlight_brightness.value) * (vr_dlight_falloff.value != 0.f ? 1.6f : 2.f);
    const glm::vec3 warm{1.f, 0.94f, 0.82f};
    const float farRatio = dist / range;

    if(fraction < 1.f)
    {
        // The pool's size: a cone of about 18 degrees either side of the beam.
        const float pool = std::max(10.f, dist * 0.32f);
        const glm::vec3 at = hit + normal * (pool * 0.6f) - dir * (pool * 0.25f);
        const float radius = CLAMP(40.f, pool * 2.2f, 450.f);
        const float k = base * (1.f - 0.55f * farRatio) * std::min(1.f, (1.f - farRatio) / 0.15f);
        light(keyPool, at, radius, warm * k);
    }
    else
    {
        killLight(keyPool); // nothing to land on (the sky, or out of reach)
    }

    if(dist > 120.f)
    {
        light(keyMid, lens + dir * (dist * 0.45f), 24.f + dist * 0.3f, warm * (base * 0.2f * (1.f - 0.5f * farRatio)));
    }
    else
    {
        killLight(keyMid);
    }

    light(keyLamp, lens + dir * (0.2f * units::metresToUnits()), 0.7f * units::metresToUnits(), warm * (base * 0.25f));

    // The beam itself, faint in the air: widening, fading out over a metre or so. Lines are drawn
    // over everything (no depth test), so it is off by default.
    const float beam = CLAMP(0.f, vr_flashlight_beam.value, 1.f);
    if(beam > 0.f)
    {
        const float m2u = units::metresToUnits();
        const float from = 0.03f * m2u;
        const float length = std::min(dist, 1.3f * m2u) - from;
        constexpr int segments = 5;
        for(int i = 0; length > 0.f && i < segments; i++)
        {
            const float t0 = static_cast<float>(i) / segments;
            const float t1 = static_cast<float>(i + 1) / segments;
            const float a0 = beam * 0.05f * (1.f - t0) * (1.f - t0);
            const float a1 = beam * 0.05f * (1.f - t1) * (1.f - t1);
            const float width = (0.03f + 0.16f * (t0 + t1) * 0.5f) * m2u;
            // Added onto the scene: the colour is the light added.
            lines::glow(lens + dir * (from + length * t0), lens + dir * (from + length * t1), width,
                glm::vec4{warm * a0, 1.f}, glm::vec4{warm * a1, 1.f});
        }
    }
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
    if(cl.worldmodel != st.world)
    {
        st.world = cl.worldmodel;
        st.mode = Mode::Mounted;
        st.holder = -1;
        st.placed = false;
    }

    const bool alive = cl.stats[STAT_HEALTH] > 0 && !cl.intermission;
    if(!enabled() || !s.valid || !alive)
    {
        ve.visible = false;
        st.placed = false;
        st.mode = Mode::Mounted;
        st.holder = -1;
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

    Pose p = mount;
    if(st.mode == Mode::Held)
    {
        p = handPose(s, st.holder);
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

    // A hand at the lamp lights it up (and taps, once).
    bool hover = false;
    for(int hand = 0; hand < 2; hand++)
    {
        const bool atLamp = st.mode != Mode::Held && key_dest == key_game && handNear(s, hand);
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

bool button(int hand, bool grip, bool pressed)
{
    if(hand < 0 || hand > 1)
    {
        return false;
    }
    bool& swallowed = st.swallowed[hand][grip ? 1 : 0];

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
    const bool atLamp = st.mode != Mode::Held && handNear(s, hand);

    if(!grip && (holding || atLamp))
    {
        toggle(hand);
        swallowed = true;
        return true;
    }
    if(grip && atLamp && handEmpty(hand))
    {
        take(hand);
        swallowed = true;
        return true;
    }
    return false;
}

} // namespace qvr::flashlight
