// vr_flatreload.cpp -- flatscreen play's reload as drawn (his words 2026-11-08: "procedurally animate the hand holding it to
// bring it to the closest hip holster, both via translation and rotation (~90 degrees downwards), and then bringing it back
// into its normal pose"; docs/vr-port/FLATPLAY.md, "Reloading").
//
// The game decides (QC vr_flatreload.qc): R (vr_flat_key_reload, sent here as impulse 55) or an empty clip
// (vr_flat_autoreload) starts a hand's trip, the rounds go in at the hip, and the trip's bit (STAT_QVR_FLATRELOAD) stays up
// for vr_flat_reload_down + _hold + _up seconds. This file draws it: each hand's share of the way to the hip rises over
// vr_flat_reload_down from the frame the bit comes up, holds through vr_flat_reload_hold, and falls over vr_flat_reload_up
// (at once when the bit drops early: the weapon changed). The hip is the nearer of the two hip holsters to the hand when
// the trip starts (its own side, as the hands rest). The pose made is the game's too: the server sees the hand at the
// holster, and the flat-screen gate there (vr_wpnholster.qc vr_flatreload_go) keeps that from reloading anything itself.
//
// vr_flat_reload_debug 1 prints the trip's steps (flatreload: ...); vr_flatreload_report each hand's state;
// vr_flat_reload_pose 0..1 holds the main hand that far along the way without a reload (the tests: a hand at the holster
// that must not reload).

#include "vr_flatreload.hpp"

#include "vr_api.h"
#include "vr_backend.hpp" // HAND_OFF / HAND_MAIN
#include "vr_body.hpp"
#include "vr_cvars.hpp"
#include "vr_tunables.hpp"
#include "vr_engine.hpp"
#include "vr_geom.hpp"
#include "vr_protocol.hpp"

#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sin.hpp"
#include "vr_zancle.hpp"

extern "C" qboolean keydown[MAX_KEYS]; // keys.c
extern "C" int Key_StringToKeynum(const char* str); // keys.c

namespace qvr::flatreload
{

namespace
{

// Each hand's trip as drawn (no function-local statics: a named file-scope struct, CODE_STYLE.md).
struct Trip
{
    bool bit{false};       // the server's bit, last frame
    double startAt{-1e9};  // when it came up (vr_gametime)
    float amount{0.f};     // the share of the way to the hip, before its ease (0 at the pose, 1 at the hip)
    int hip{body::RightHip}; // the hip holster it goes to (chosen at the start)
    bool chooseHip{false}; // choose it at the next pose (the hand's place as the trip starts)
    bool arrived{false};   // printed its arrival (vr_flat_reload_debug)
    float hipGap{0.f};     // the game hand's distance from its holster at its last pose, and its pitch
    float pitch{0.f};
};

// Each hand's gun drawn open (his words 2026-11-10: "When the super shotgun is broken open in flatscreen automatically,
// can you also smoothly adjust the pitch/yaw/roll of the gun at the same time to give it a more natural pose?"): the
// super shotgun broken open (its weapon flag) or the revolver's cylinder out (its stat), as the server says.
struct OpenPose
{
    float amount{0.f};   // the share of the open pose, before its ease
    int kind{0};         // 0 none yet, 1 the super shotgun's tunables, 2 the revolver's (kept while it eases out)
    bool open{false};    // last frame
    double since{-1e9};  // when it opened or shut (vr_gametime)
    glm::vec3 rot{0.f};  // what it added last frame (the report: pitch, yaw, roll)
    glm::vec3 move{0.f}; // and its move (forward, up, outward)
};

struct ReloadState
{
    Trip trip[2];
    OpenPose openPose[2];
    bool keyDown{false};   // R, last frame
    int presses{0};
    double lastTime{-1e9};
};

ReloadState rs;

[[nodiscard]] float downTime()
{
    return za::max(0.05f, vr_flat_reload_down.value);
}

[[nodiscard]] float holdTime()
{
    return za::max(0.f, vr_flat_reload_hold.value);
}

[[nodiscard]] float upTime()
{
    return za::max(0.05f, vr_flat_reload_up.value);
}

[[nodiscard]] float share(int h)
{
    float a = rs.trip[h].amount;
    if(h == HAND_MAIN && vr_flat_reload_pose.value >= 0.f)
    {
        a = za::max(a, za::clamp(vr_flat_reload_pose.value, 0.f, 1.f)); // (the tests' hand held at the holster)
    }
    return geom::smooth01(a);
}

[[nodiscard]] const char* handName(int h)
{
    return h == HAND_MAIN ? "main" : "off";
}

constexpr int weaponFlagSsgOpen = 32; // QC's QVR_WPNFLAG_SSG_OPEN (vr_defs.qc)
constexpr int rvOpenBit = 4096;       // STAT_QVR_REVOLVER's cylinder swung out (QC vr_revolver.qc .vr_rv_hud)
constexpr int rvHeldBit = 131072;     // and a revolver in that hand

// What hand `h`'s gun is drawn open as now (0 shut, 1 the super shotgun broken open, 2 the revolver's cylinder out).
[[nodiscard]] int openKind(int h)
{
    const bool main = h == HAND_MAIN;
    const int rv = cl.stats[main ? protocol::STAT_QVR_REVOLVER : protocol::STAT_QVR_REVOLVER2];
    if((rv & rvHeldBit) && (rv & rvOpenBit))
    {
        return 2;
    }
    const int flags = cl.stats[main ? protocol::STAT_QVR_WEAPONFLAGS : protocol::STAT_QVR_WEAPONFLAGS2];
    return (flags & weaponFlagSsgOpen) ? 1 : 0;
}

struct OpenTune
{
    float pitch, yaw, roll, fwd, up, out, in, back;
};

[[nodiscard]] OpenTune openTune(int kind)
{
    if(kind == 2)
    {
        return {vr_flat_rv_open_pitch.value, vr_flat_rv_open_yaw.value, vr_flat_rv_open_roll.value, vr_flat_rv_open_fwd.value,
            vr_flat_rv_open_up.value, vr_flat_rv_open_out.value, vr_flat_rv_open_in_time.value, vr_flat_rv_open_out_time.value};
    }
    return {vr_flat_ssg_open_pitch.value, vr_flat_ssg_open_yaw.value, vr_flat_ssg_open_roll.value, vr_flat_ssg_open_fwd.value,
        vr_flat_ssg_open_up.value, vr_flat_ssg_open_out.value, vr_flat_ssg_open_in_time.value, vr_flat_ssg_open_out_time.value};
}

void report_f()
{
    const int bits = cl.stats[protocol::STAT_QVR_FLATRELOAD];
    for(int h : {HAND_OFF, HAND_MAIN})
    {
        const Trip& t = rs.trip[h];
        Con_Printf("flatreload: %s hand: server %d, share %.2f, hip %s, %.1f units from it, pitch %.1f, hotspot %d; R presses %d\n",
            handName(h), (bits >> h) & 1, share(h), t.hip == body::LeftHip ? "left" : "right", t.hipGap, t.pitch,
            hands::current().hotspot[h], rs.presses);
        const OpenPose& o = rs.openPose[h];
        Con_Printf("flatreload: %s hand: open %d kind %d share %.2f, turned %.1f %.1f %.1f, moved %.1f %.1f %.1f\n", handName(h),
            o.open ? 1 : 0, o.kind, geom::smooth01(o.amount), o.rot.x, o.rot.y, o.rot.z, o.move.x, o.move.y, o.move.z);
    }
}

} // namespace

void init()
{
    Cmd_AddCommand("vr_flatreload_report", report_f);
}

void openFrame(float dt); // (below)

void frame(bool on)
{
    double dt = rs.lastTime > -1e8 ? vr_gametime - rs.lastTime : 0.0;
    rs.lastTime = vr_gametime;
    dt = za::clamp(dt, 0.0, 0.1);
    if(!on)
    {
        rs = ReloadState{};
        return;
    }
    openFrame(static_cast<float>(dt));

    // R: the game reloads every hand whose gun can take rounds (QC VR_FlatReload_Key).
    const int k = vr_flat_key_reload.string[0] ? Key_StringToKeynum(vr_flat_key_reload.string) : -1;
    const bool down = key_dest == key_game && k > 0 && k < MAX_KEYS && keydown[k] != 0;
    if(down && !rs.keyDown)
    {
        rs.presses++;
        Cbuf_InsertText("impulse 55\n");
        if(vr_flat_reload_debug.value)
        {
            Con_Printf("flatreload: %s: reload\n", vr_flat_key_reload.string);
        }
    }
    rs.keyDown = down;

    const int bits = cl.stats[protocol::STAT_QVR_FLATRELOAD];
    for(int h : {HAND_OFF, HAND_MAIN})
    {
        Trip& t = rs.trip[h];
        const bool bit = ((bits >> h) & 1) != 0;
        if(bit && !t.bit)
        {
            t.startAt = vr_gametime;
            t.arrived = false;
            if(t.amount <= 0.f)
            {
                t.chooseHip = true;
            }
            if(vr_flat_reload_debug.value)
            {
                Con_Printf("flatreload: %s hand: the trip starts\n", handName(h));
            }
        }
        if(!bit && t.bit && vr_flat_reload_debug.value)
        {
            Con_Printf("flatreload: %s hand: the server's trip ends (%.2f s)\n", handName(h), vr_gametime - t.startAt);
        }
        t.bit = bit;
        if(h == HAND_MAIN && vr_flat_reload_pose.value > 0.f && t.amount <= 0.f)
        {
            t.chooseHip = true; // (the tests' hand held at the holster: its hip chosen as a trip's would be)
        }
        const double since = vr_gametime - t.startAt;
        const bool toHip = bit && since < static_cast<double>(downTime() + holdTime()) - 1e-4;
        const float before = t.amount;
        if(dt > 0.0)
        {
            const float step = static_cast<float>(dt);
            t.amount = toHip ? za::min(1.f, t.amount + step / downTime()) : za::max(0.f, t.amount - step / upTime());
        }
        if(vr_flat_reload_debug.value)
        {
            if(t.amount >= 1.f && before < 1.f)
            {
                Con_Printf("flatreload: %s hand at the hip (%.2f s): %.1f units from the %s hip holster, pitch %.1f\n", handName(h),
                    since, t.hipGap, t.hip == body::LeftHip ? "left" : "right", t.pitch);
            }
            if(t.amount <= 0.f && before > 0.f)
            {
                Con_Printf("flatreload: %s hand back in its pose (%.2f s)\n", handName(h), since);
            }
        }
    }
}

// Each frame (from frame): each hand's open pose eased in over its In Time while the server says the gun is open, out
// over its Out Time once it is shut.
void openFrame(float dt)
{
    for(int h : {HAND_OFF, HAND_MAIN})
    {
        OpenPose& o = rs.openPose[h];
        const int kind = openKind(h);
        const bool open = kind != 0;
        if(open)
        {
            o.kind = kind;
        }
        if(open != o.open)
        {
            o.open = open;
            o.since = vr_gametime;
            if(vr_flat_reload_debug.value)
            {
                Con_Printf("flatreload: %s hand: the gun %s\n", handName(h), open ? "opens" : "shuts");
            }
        }
        if(o.kind == 0)
        {
            continue;
        }
        const OpenTune t = openTune(o.kind);
        const float before = o.amount;
        o.amount = open ? za::min(1.f, o.amount + dt / za::max(0.02f, t.in))
                        : za::max(0.f, o.amount - dt / za::max(0.02f, t.back));
        if(vr_flat_reload_debug.value)
        {
            if(o.amount >= 1.f && before < 1.f)
            {
                Con_Printf("flatreload: %s hand in its open pose (%.2f s)\n", handName(h), vr_gametime - o.since);
            }
            if(o.amount <= 0.f && before > 0.f)
            {
                Con_Printf("flatreload: %s hand out of its open pose (%.2f s)\n", handName(h), vr_gametime - o.since);
            }
        }
    }
}

void apply(hands::State& s)
{
    // The open gun's pose first (the trip to the hip, below, goes on from it): turned and moved in the body's frame, the
    // yaw, roll and outward move mirrored for the left hand (the off hand).
    for(int h : {HAND_OFF, HAND_MAIN})
    {
        OpenPose& o = rs.openPose[h];
        const float a = geom::smooth01(o.amount);
        o.rot = o.move = glm::vec3{0.f};
        if(a <= 0.f || o.kind == 0)
        {
            continue;
        }
        const OpenTune t = openTune(o.kind);
        const float side = h == HAND_MAIN ? 1.f : -1.f; // (yaw grows to the left: the right hand turns in by +yaw)
        o.rot = glm::vec3{t.pitch, side * t.yaw, side * t.roll} * a;
        o.move = glm::vec3{t.fwd, t.up, t.out} * a;
        const float yaw = glm::radians(s.bodyYaw);
        const glm::vec3 fwd{za::cos(yaw), za::sin(yaw), 0.f};
        const glm::vec3 right{za::sin(yaw), -za::cos(yaw), 0.f};
        s.rot[h] += o.rot;
        s.pos[h] += fwd * o.move.x + glm::vec3{0.f, 0.f, o.move.y} + right * (side * o.move.z);
    }
    for(int h : {HAND_OFF, HAND_MAIN})
    {
        Trip& t = rs.trip[h];
        const float a = share(h);
        if(a <= 0.f)
        {
            continue;
        }
        if(t.chooseHip)
        {
            // The nearer hip holster to the hand where its trip starts (its own side, as the hands rest).
            const glm::vec3 left = body::holsterPosition(s, body::LeftHip);
            const glm::vec3 right = body::holsterPosition(s, body::RightHip);
            t.hip = glm::distance(s.pos[h], left) < glm::distance(s.pos[h], right) ? body::LeftHip : body::RightHip;
            t.chooseHip = false;
        }
        const glm::vec3 hip = body::holsterPosition(s, static_cast<body::Holster>(t.hip));
        s.pos[h] += (hip - s.pos[h]) * a;
        // Turned muzzle-down (pitch: positive is down), its roll levelled; its yaw stays the body's way.
        const float angle = za::clamp(vr_flat_reload_angle.value, 0.f, 150.f);
        s.rot[h].x += (angle - s.rot[h].x) * a;
        s.rot[h].y += geom::yawDelta(s.bodyYaw, s.rot[h].y) * a;
        s.rot[h].z -= s.rot[h].z * a;
        t.hipGap = glm::distance(s.pos[h], hip);
        t.pitch = s.rot[h].x;
    }
}

bool busy(int h)
{
    return h >= 0 && h < 2 && (rs.trip[h].bit || rs.trip[h].amount > 0.f);
}

float openShare(int h)
{
    return h >= 0 && h < 2 ? geom::smooth01(rs.openPose[h].amount) : 0.f;
}

} // namespace qvr::flatreload
