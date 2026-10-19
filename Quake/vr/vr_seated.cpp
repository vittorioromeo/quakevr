// vr_seated.cpp -- seated play (vr_seated.hpp; docs/vr-port/SEATED.md).

#include "vr_seated.hpp"
#include "vr_avatar.hpp"
#include "vr_backend.hpp"
#include "vr_body.hpp"
#include "vr_bodycal.hpp"
#include "vr_bodytrack.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_hands.hpp"
#include "vr_main.hpp"
#include "vr_setup.hpp"
#include "vr_units.hpp"

#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Exp.hpp"
#include "Zancle/Math/MinMax.hpp"

namespace qvr::seated
{

namespace
{

// The crouch toggle (vr_crouch_toggle): on or not, and how far it has eased in (0 .. 1).
struct Crouch
{
    bool on{false};
    float eased{0.f};
    double last{-1.0};
};
Crouch crouch;

// The share of the seated eye height the standing one is taken for while it isn't measured (a seated adult's eyes are
// about 0.74 of the standing height over the floor, on a chair at knee height).
constexpr float ESTIMATED_SHARE = 0.74f;

void easeCrouch()
{
    const double now = realtime;
    const float dt = crouch.last >= 0.0 ? static_cast<float>(za::clamp(now - crouch.last, 0.0, 0.1)) : 0.f;
    crouch.last = now;
    const float target = crouch.on && active() ? 1.f : 0.f;
    const float tau = za::max(0.f, vr_seated_crouch_ease.value);
    crouch.eased = tau > 0.f ? crouch.eased + (target - crouch.eased) * (1.f - za::exp(-dt / tau)) : target;
}

// A calibration is measuring the body (its poses' hands where they really are).
[[nodiscard]] bool measuring()
{
    return bodycal::phase() == bodycal::Phase::Capturing || setup::running();
}

void crouchToggle_f()
{
    if(!active())
    {
        Con_Printf("vr_crouch_toggle: seated play only (Position: Seated)\n");
        return;
    }
    crouch.on = !crouch.on;
    Con_Printf("vr_crouch_toggle: %s\n", crouch.on ? "crouched" : "standing");
}

void heightNow_f()
{
    const TrackingState& t = tracking();
    if(!vrActive() || !t.head.valid)
    {
        Con_Printf("vr_seated_height_now: no headset\n");
        return;
    }
    Cvar_SetValueQuick(&vr_seated_height, t.head.position.y);
    Con_Printf("vr_seated_height_now: your eyes at %.2f m sitting\n", t.head.position.y);
}

// vr_seated_status: the lift, the head and the drawn legs over the floor, the leg tracking's weights and where the
// holsters and pouches are on the body (units forward, right, up over the floor; the body's facing).
void status_f()
{
    Con_Printf("seated: %s, standing eyes %.3f m, seated eyes %.3f m%s, lift %.3f m, crouch toggle %s (%.2f)\n",
        active() ? "on" : "off", units::eyeHeight(), eyeHeight(), vr_seated_height.value > 0.3f ? "" : " (estimated)",
        lift(), crouch.on ? "on" : "off", crouch.eased);
    const hands::State& s = hands::current();
    if(!s.valid)
    {
        Con_Printf("seated: no hands state\n");
        return;
    }
    const float m2u = units::metresToUnits();
    const float floorZ = s.head.z - s.headHeight * m2u;
    Con_Printf("seated: head %.3f m over the floor (%.1f units), crouch ratio %.2f, leg tracking %.2f %.2f, torso %.2f\n",
        s.headHeight, s.headHeight * m2u, s.crouchRatio, bodytrack::legWeight(0), bodytrack::legWeight(1),
        bodytrack::torsoWeight());
    avatar::Skeleton sk;
    if(avatar::skeleton(sk) && sk.legs)
    {
        Con_Printf("seated: legs: hips %.1f %.1f, knees %.1f %.1f, ankles %.1f %.1f units over the floor\n",
            sk.hip[0].z - floorZ, sk.hip[1].z - floorZ, sk.knee[0].z - floorZ, sk.knee[1].z - floorZ,
            sk.ankle[0].z - floorZ, sk.ankle[1].z - floorZ);
    }
    else
    {
        Con_Printf("seated: legs: not posed\n");
    }
    glm::vec3 fwd, right, up;
    hands::angleVectors({0.f, s.bodyYaw, 0.f}, fwd, right, up);
    const glm::vec3 centre{s.head.x, s.head.y, floorZ};
    const auto rel = [&](const char* name, const glm::vec3& p) {
        const glm::vec3 d = p - centre;
        Con_Printf("seated: %-14s fwd %6.1f right %6.1f up %6.1f\n", name, glm::dot(d, fwd), glm::dot(d, right), d.z);
    };
    static constexpr const char* names[body::HolsterCount] = {
        "left shoulder", "right shoulder", "left hip", "right hip", "left upper", "right upper"};
    const body::HolsterPositions hp = body::holsterPositions(s);
    for(int h = 0; h < body::HolsterCount; h++)
    {
        rel(names[h], hp[h]);
    }
    rel("ammo pouch", body::ammoPouchPosition(s));
    rel("grenade pouch", body::pouchPosition(s));
    rel("main hand", s.pos[HAND_MAIN]);
    rel("off hand", s.pos[HAND_OFF]);
}

} // namespace

bool active()
{
    return vr_bodycal_seated.value != 0.f;
}

float eyeHeight()
{
    return vr_seated_height.value > 0.3f ? vr_seated_height.value : ESTIMATED_SHARE * units::eyeHeight();
}

float lift()
{
    if(!active())
    {
        return 0.f;
    }
    const float stand = vr_seated_stand.value != 0.f ? za::max(0.f, units::eyeHeight() - eyeHeight()) : 0.f;
    return stand - za::max(0.f, vr_seated_crouch_depth.value) * crouch.eased;
}

float handDrop(float realY)
{
    const float up = lift();
    if(up <= 0.f || vr_seated_reach.value == 0.f || measuring())
    {
        return 0.f;
    }
    const float start = vr_seated_reach_start.value;
    const float floor = za::min(vr_seated_reach_floor.value, start - 0.05f);
    return up * za::clamp((start - realY) / (start - floor), 0.f, 1.f);
}

bool raise(const TrackingState& in, TrackingState& out)
{
    easeCrouch();
    const float up = lift();
    if(up == 0.f)
    {
        return false;
    }
    out = in;
    out.head.position.y += up;
    for(int h = 0; h < HAND_COUNT; h++)
    {
        out.hands[h].position.y += up - handDrop(in.hands[h].position.y);
    }
    return true;
}

void registerCommands()
{
    Cmd_AddCommand("vr_seated_status", status_f);
    Cmd_AddCommand("vr_crouch_toggle", crouchToggle_f);
    Cmd_AddCommand("vr_seated_height_now", heightNow_f);
}

} // namespace qvr::seated
