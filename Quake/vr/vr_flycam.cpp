// vr_flycam.cpp -- flatscreen play's fly camera (vr_flat_flycam). See vr_flycam.hpp.

#include "vr_flycam.hpp"

#include "vr_engine.hpp"
#include "vr_api.h"
#include "vr_cvars.hpp"
#include "vr_flatplay.hpp"
#include "vr_geom.hpp"
#include "vr_hands.hpp"
#include "vr_progs.hpp"
#include "vr_testkit.hpp"

#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Atan2.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/Sin.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Remainder.hpp"
#include "vr_zancle.hpp"

#include <glm/geometric.hpp>

extern "C" qboolean keydown[MAX_KEYS];                  // keys.c
extern "C" int Key_StringToKeynum(const char* str);     // keys.c
extern "C" kbutton_t in_attack;                         // cl_input.c: +attack (the test holds it: it must not fire)

namespace qvr::flycam
{

namespace
{

constexpr float pitchMost = 89.f;   // the camera's look up and down (degrees)
constexpr float dtMost = 0.1f;      // a frame's step at most (a hitch does not throw the camera)

struct Cam
{
    bool on{false};
    glm::vec3 pos{0.f};       // the eye flown
    glm::vec3 turn{0.f};      // its pitch and yaw (Quake's: pitch down positive; no roll)
    glm::vec3 vel{0.f};       // units a second
    glm::vec3 eyeOrg{0.f};    // his own eye, as V_CalcRefdef made it (VR_FlatFlycamView)
    glm::vec3 eyeAngles{0.f};
    bool eyeValid{false};
    double lastReal{-1.0};    // realtime of the last flight step (the flight runs on while the game is held)
    bool keyWas{false};       // vr_flat_key_flycam down last frame
};

Cam cam;

[[nodiscard]] bool levelUp()
{
    return cls.state == ca_connected && cls.signon == SIGNONS;
}

void setCvar(bool v)
{
    if((vr_flat_flycam.value != 0.f) != v)
    {
        Cvar_SetValueQuick(&vr_flat_flycam, v ? 1.f : 0.f);
    }
}

void enter()
{
    cam.on = true;
    cam.pos = cam.eyeValid ? cam.eyeOrg : geom::vec(r_refdef.vieworg);
    const glm::vec3 a = cam.eyeValid ? cam.eyeAngles : geom::vec(r_refdef.viewangles);
    cam.turn = {za::clamp(a.x, -pitchMost, pitchMost), a.y, 0.f};
    cam.vel = glm::vec3{0.f};
    cam.lastReal = realtime;
    Con_Printf("flycam: on -- the mouse looks, W A S D fly, Space up, Ctrl down, Shift faster; %s or "
               "vr_flat_flycam_toggle: back in your eyes (%s)\n",
        vr_flat_key_flycam.string[0] ? vr_flat_key_flycam.string : "vr_flat_flycam 0",
        holds() ? "the game paused: vr_flat_flycam_pause 0 runs it on"
                : (vr_flat_flycam_pause.value != 0.f ? "not paused: multiplayer or a demo runs on" : "the game runs on"));
}

// vr_flat_flycam_place [<forward> <right> <up> [<pitch> [<yaw>]]] (tests, and a look at his body): the fly camera on, put
// that many units off his eye along his level forward, right and up, and turned to look back at his eye (pitch: the
// look down, else at the eye; yaw: degrees left of his own view's, else at the eye: a look at his gun from over his
// shoulder); no arguments, 60 in front, a little above, looking back at him.
void place_f()
{
    if(!flatplay::active() || !levelUp())
    {
        Con_Printf("vr_flat_flycam_place: only in flatscreen play, in a game\n");
        return;
    }
    const float f = Cmd_Argc() > 1 ? Q_atof(Cmd_Argv(1)) : 60.f;
    const float r = Cmd_Argc() > 2 ? Q_atof(Cmd_Argv(2)) : 0.f;
    const float u = Cmd_Argc() > 3 ? Q_atof(Cmd_Argv(3)) : 4.f;
    const glm::vec3 eye = cam.eyeValid ? cam.eyeOrg : geom::vec(r_refdef.vieworg);
    const float yaw = cl.viewangles[YAW];
    const float y = glm::radians(yaw);
    const glm::vec3 fwd{za::cos(y), za::sin(y), 0.f}, right{za::sin(y), -za::cos(y), 0.f};
    if(!cam.on)
    {
        setCvar(true);
        enter();
    }
    cam.pos = eye + fwd * f + right * r + glm::vec3{0.f, 0.f, u};
    const glm::vec3 back = eye - cam.pos;
    const float flat = glm::length(glm::vec2{back.x, back.y});
    const float pitch = Cmd_Argc() > 4 ? Q_atof(Cmd_Argv(4)) : glm::degrees(za::atan2(-back.z, za::max(flat, 0.01f)));
    const float turnYaw = Cmd_Argc() > 5 ? yaw + Q_atof(Cmd_Argv(5)) : glm::degrees(za::atan2(back.y, back.x));
    cam.turn = {za::clamp(pitch, -pitchMost, pitchMost), turnYaw, 0.f};
    cam.vel = glm::vec3{0.f};
    Con_Printf("vr_flat_flycam_place: at %.0f %.0f %.0f, turned %.0f %.0f\n", cam.pos.x, cam.pos.y, cam.pos.z, cam.turn.x, cam.turn.y);
}

void leave(const char* why)
{
    cam.on = false;
    cam.vel = glm::vec3{0.f};
    Con_Printf("flycam: off (%s): the view back in your eyes\n", why);
}

// The flight: the camera's own frame (its turn), Space and Ctrl straight up and down, eased to the asked speed at
// vr_flat_flycam_accel (0: at once). Only while the game has the keys (not with the console or a menu up).
void fly()
{
    const float dt = cam.lastReal >= 0.0 ? za::clamp(static_cast<float>(realtime - cam.lastReal), 0.f, dtMost) : 0.f;
    cam.lastReal = realtime;
    glm::vec3 wish{0.f};
    bool fast = false;
    if(key_dest == key_game)
    {
        const auto held = [](const kbutton_t& b) { return (b.state & 1) != 0 ? 1.f : 0.f; };
        const float fwd = held(in_forward) - held(in_back);
        const float side = held(in_moveright) - held(in_moveleft);
        const float up = za::max(held(in_jump), held(in_up)) - za::max(held(in_down), keydown[K_CTRL] ? 1.f : 0.f);
        glm::vec3 f, r, u;
        hands::angleVectors(cam.turn, f, r, u);
        wish = f * fwd + r * side + glm::vec3{0.f, 0.f, up};
        const float len = glm::length(wish);
        if(len > 0.f)
        {
            wish /= len;
        }
        fast = keydown[K_SHIFT] || (in_speed.state & 1) != 0;
    }
    const float speed = za::max(vr_flat_flycam_speed.value, 1.f) * (fast ? za::max(vr_flat_flycam_fast.value, 0.1f) : 1.f);
    const glm::vec3 target = wish * speed;
    const float accel = vr_flat_flycam_accel.value;
    const glm::vec3 gap = target - cam.vel;
    const float gapLen = glm::length(gap);
    if(accel <= 0.f || gapLen <= accel * dt)
    {
        cam.vel = target;
    }
    else
    {
        cam.vel += gap * (accel * dt / gapLen);
    }
    cam.pos += cam.vel * dt;
}

void toggle_f()
{
    if(!flatplay::active())
    {
        Con_Printf("vr_flat_flycam_toggle: flat-screen play only (-flatscreenmode); a headset's view is its own\n");
        return;
    }
    setCvar(!cam.on);
}

// ---- vr_flat_flycam_test: on, turned round, flown 200 units off and back at him, shot; checked that nothing of his moved
// and the game's time stood still (pause 1) and ran (pause 0), no button reaching him; off, the view his own again.

enum class Phase
{
    Idle,
    Settle,
    Turn,
    Fly,
    Brake,
    Shot,
    Near,
    Live,
    Back,
};

struct Snap
{
    glm::vec3 svOrg{0.f}, svAng{0.f}, clAng{0.f}, hand[2]{}, handRot[2]{}, eye{0.f}, eyeAng{0.f};
    double gameTime{0.0};
};

struct Test
{
    Phase phase{Phase::Idle};
    int frames{0};
    int passed{0};
    int failed{0};
    Snap before;
    float turned{0.f};
    float yawWas{0.f};
    glm::vec3 camStart{0.f};
    double liveFrom{0.0};
    int leaks{0};
    float pauseWas{1.f};
    bool near{false}; // flown back in for the portrait
};

Test test;

constexpr int settleFrames = 144; // after the level is up: his spawn's pose settled
constexpr int shotFrames = 3;     // the screenshot taken from the frame drawn after it is asked for
constexpr int liveFrames = 72;    // a second of game time with the game running on (pause 0)
constexpr int phaseMost = 72 * 10; // a phase stuck this long: FAIL "timed out" (a safety net)
constexpr float riseBy = 6.f;     // the flight out rises this much (units), Space held
constexpr float nearDist = 80.f;  // the portrait's distance in front of his eye (units)
constexpr float nearPitch = 22.f; // and its look down at his chest (degrees)

[[nodiscard]] Snap snap()
{
    Snap s;
    if(edict_t* e = progs::localPlayer())
    {
        s.svOrg = geom::vec(e->v.origin);
        s.svAng = geom::vec(e->v.v_angle);
    }
    s.clAng = geom::vec(cl.viewangles);
    const hands::State& hs = hands::current();
    for(int h = 0; h < 2; h++)
    {
        s.hand[h] = hs.pos[h];
        s.handRot[h] = hs.rot[h];
    }
    s.eye = geom::vec(r_refdef.vieworg);
    s.eyeAng = geom::vec(r_refdef.viewangles);
    s.gameTime = sv.active ? sv.qcvm.time : cl.time;
    return s;
}

void check(bool ok, const char* what)
{
    (ok ? test.passed : test.failed)++;
    Con_Printf("flycam_test: %s %s\n", ok ? "PASS" : "FAIL", what);
}

[[nodiscard]] float apart(const glm::vec3& a, const glm::vec3& b)
{
    return glm::length(a - b);
}

[[nodiscard]] float anglesApart(const glm::vec3& a, const glm::vec3& b)
{
    float most = 0.f;
    for(int i = 0; i < 3; i++)
    {
        most = za::max(most, za::abs(za::remainder(a[i] - b[i], 360.f)));
    }
    return most;
}

// Nothing of his moved: where the server has him, his angles (the server's and the client's) and both hands.
void checkStill(const char* when)
{
    const Snap now = snap();
    float hands = 0.f, handTurn = 0.f;
    for(int h = 0; h < 2; h++)
    {
        hands = za::max(hands, apart(now.hand[h], test.before.hand[h]));
        handTurn = za::max(handTurn, anglesApart(now.handRot[h], test.before.handRot[h]));
    }
    const float org = apart(now.svOrg, test.before.svOrg);
    const float ang = za::max(anglesApart(now.svAng, test.before.svAng), anglesApart(now.clAng, test.before.clAng));
    Con_Printf("flycam_test: %s: origin moved %.3f, angles %.3f deg, hands %.3f units %.3f deg, buttons sent %d\n", when,
        org, ang, hands, handTurn, test.leaks);
    check(org < 0.01f, va("%s: his origin unchanged (%.3f)", when, org));
    check(ang < 0.01f, va("%s: his angles unchanged (%.3f deg)", when, ang));
    check(hands < 0.05f && handTurn < 0.05f, va("%s: his hands unchanged (%.3f units, %.3f deg)", when, hands, handTurn));
    check(test.leaks == 0, va("%s: no button or impulse reached him (%d frames with one)", when, test.leaks));
}

void verdict()
{
    in_right.state &= ~1;
    in_lookdown.state &= ~1;
    in_back.state &= ~1;
    in_forward.state &= ~1;
    in_jump.state &= ~1;
    in_attack.state &= ~1;
    Cvar_SetValueQuick(&vr_flat_flycam_pause, test.pauseWas);
    setCvar(false);
    testkit::verdict("vr_flat_flycam_test", test.passed, test.failed);
    test.phase = Phase::Idle;
}

void next(Phase p)
{
    test.phase = p;
    test.frames = 0;
}

void testFrame()
{
    if(test.phase == Phase::Idle)
    {
        return;
    }
    test.frames++;
    if(test.frames > phaseMost)
    {
        check(false, va("timed out in phase %d", static_cast<int>(test.phase)));
        verdict();
        return;
    }
    // A fire or a jump the server read from his move, or an impulse (SV_ReadClientMove), while the camera flies.
    if(cam.on)
    {
        if(edict_t* e = progs::localPlayer())
        {
            if(e->v.button0 != 0.f || e->v.button2 != 0.f || e->v.impulse != 0.f)
            {
                test.leaks++;
            }
        }
    }
    switch(test.phase)
    {
        case Phase::Idle: break;
        case Phase::Settle:
            if(!levelUp() || !flatplay::active() || test.frames < settleFrames)
            {
                return;
            }
            test.pauseWas = vr_flat_flycam_pause.value;
            Cvar_SetValueQuick(&vr_flat_flycam_pause, 1.f);
            setCvar(true); // (taken up by frame() next frame, before the server's frame: held from that one on)
            next(Phase::Turn);
            break;
        case Phase::Turn:
            if(test.frames == 1)
            {
                // Taken now: the camera came on this frame, before the server's frame and before any of his is posed --
                // the hands are still the last frame's and the view his eye's.
                test.before = snap();
                test.turned = 0.f;
                test.yawWas = cam.turn.y;
                check(cam.on, "on (vr_flat_flycam 1)");
                check(holds(), "the game held (vr_flat_flycam_pause 1, single player)");
            }
            test.turned += za::remainder(cam.turn.y - test.yawWas, 360.f);
            test.yawWas = cam.turn.y;
            if(za::abs(test.turned) < 180.f)
            {
                in_right.state |= 1; // (+right: CL_AdjustAngles, the keyboard's turn: the look the camera takes)
                return;
            }
            in_right.state &= ~1;
            test.camStart = cam.pos;
            next(Phase::Fly);
            break;
        case Phase::Fly:
        {
            // Backwards (he is in front of it now) and a little up, the fire button and the jump held: none reach him.
            const glm::vec3 off = cam.pos - test.camStart;
            const bool far = glm::length(glm::vec3{off.x, off.y, 0.f}) >= 200.f;
            in_back.state = far ? (in_back.state & ~1) : (in_back.state | 1);
            in_jump.state = off.z >= riseBy ? (in_jump.state & ~1) : (in_jump.state | 1);
            in_attack.state |= 1;
            if(far && off.z >= riseBy)
            {
                in_attack.state &= ~1;
                next(Phase::Brake);
            }
            break;
        }
        case Phase::Brake:
            if(glm::length(cam.vel) > 0.f)
            {
                return;
            }
            Cbuf_InsertText("screenshot\n"); // (Insert: ahead of a script's waits still queued)
            next(Phase::Shot);
            break;
        case Phase::Near:
        {
            // Then in to a portrait's distance in front of him (the second screenshot: his body, his hands, his weapon).
            const glm::vec3 off = cam.pos - test.before.eye;
            const bool close = glm::length(glm::vec3{off.x, off.y, 0.f}) <= nearDist;
            // (+lookdown once there, the keys' look, taken by the camera: flying forward goes where it looks)
            const bool down = close && cam.turn.x >= nearPitch;
            in_forward.state = close ? (in_forward.state & ~1) : (in_forward.state | 1);
            in_lookdown.state = !close || down ? (in_lookdown.state & ~1) : (in_lookdown.state | 1);
            if(close && down)
            {
                test.near = true;
                next(Phase::Brake);
            }
            break;
        }
        case Phase::Shot:
        {
            if(test.frames < shotFrames)
            {
                return;
            }
            if(test.near)
            {
                const double t = sv.active ? sv.qcvm.time : cl.time;
                check(t == test.before.gameTime, va("paused: the game's time stood still (%.4f s)", t - test.before.gameTime));
                checkStill("paused, flown back in");
                Cvar_SetValueQuick(&vr_flat_flycam_pause, 0.f);
                test.liveFrom = t;
                test.leaks = 0;
                next(Phase::Live);
                break;
            }
            const float flown = apart(geom::vec(r_refdef.vieworg), test.before.eye);
            const float turned = za::abs(za::remainder(r_refdef.viewangles[YAW] - test.before.eyeAng.y, 360.f));
            Con_Printf("flycam_test: the view %.1f units off his eye, turned %.1f deg; his eye %.1f %.1f %.1f, the camera "
                       "%.1f %.1f %.1f\n", flown, turned, test.before.eye.x, test.before.eye.y, test.before.eye.z,
                r_refdef.vieworg[0], r_refdef.vieworg[1], r_refdef.vieworg[2]);
            check(flown >= 200.f, va("the view flown 200 units off (%.1f)", flown));
            check(turned > 170.f, va("the view turned round to face him (%.1f deg)", turned));
            checkStill("paused, flown");
            next(Phase::Near);
            break;
        }
        case Phase::Live:
            // The game running on: his fire, his jump and his walk held all the while (the camera flies forward).
            in_forward.state |= 1;
            in_jump.state |= 1;
            in_attack.state |= 1;
            if(test.frames == 1)
            {
                check(!holds(), "not held with vr_flat_flycam_pause 0");
            }
            if(test.frames < liveFrames)
            {
                return;
            }
            in_forward.state &= ~1;
            in_jump.state &= ~1;
            in_attack.state &= ~1;
            {
                const double t = sv.active ? sv.qcvm.time : cl.time;
                check(t - test.liveFrom > 0.9, va("pause 0: the game's time ran on (%.3f s)", t - test.liveFrom));
            }
            checkStill("running on, his keys held");
            setCvar(false);
            next(Phase::Back);
            break;
        case Phase::Back:
        {
            if(test.frames < shotFrames)
            {
                return;
            }
            const float org = apart(geom::vec(r_refdef.vieworg), test.before.eye);
            const float ang = anglesApart(geom::vec(r_refdef.viewangles), test.before.eyeAng);
            check(!cam.on, "off (vr_flat_flycam 0)");
            check(org < 0.01f && ang < 0.01f, va("the view back in his eye exactly (%.4f units, %.4f deg)", org, ang));
            verdict();
            break;
        }
    }
}

void test_f()
{
    if(!flatplay::active())
    {
        Con_Printf("vr_flat_flycam_test: flat-screen play only (-flatscreenmode)\n");
        testkit::verdict("vr_flat_flycam_test", 0, 1);
        return;
    }
    test = Test{};
    test.pauseWas = vr_flat_flycam_pause.value;
    Con_Printf("flycam_test: started\n");
    next(Phase::Settle);
}

} // namespace

void init()
{
    Cmd_AddCommand("vr_flat_flycam_toggle", toggle_f);
    Cmd_AddCommand("vr_flat_flycam_test", test_f);
    Cmd_AddCommand("vr_flat_flycam_place", place_f);
}

bool on()
{
    return cam.on;
}

bool holds()
{
    // Only a game this client's own server runs for him alone: a multiplayer server is not his to stop, and a demo or a
    // remote server has nothing here to hold.
    return cam.on && vr_flat_flycam_pause.value != 0.f && sv.active && svs.maxclients == 1 && !cls.demoplayback;
}

glm::vec3 eyeOrg()
{
    return cam.on && cam.eyeValid ? cam.eyeOrg : geom::vec(r_refdef.vieworg);
}

glm::vec3 eyeAngles()
{
    return cam.on && cam.eyeValid ? cam.eyeAngles : geom::vec(r_refdef.viewangles);
}

void frame(bool flatOn)
{
    const int k = vr_flat_key_flycam.string[0] ? Key_StringToKeynum(vr_flat_key_flycam.string) : -1;
    const bool key = flatOn && key_dest == key_game && k > 0 && k < MAX_KEYS && keydown[k] != 0;
    if(key && !cam.keyWas)
    {
        setCvar(!cam.on);
    }
    cam.keyWas = key;

    const bool want = vr_flat_flycam.value != 0.f;
    if(want && !cam.on)
    {
        if(!flatOn)
        {
            Con_Printf("vr_flat_flycam: flat-screen play only (-flatscreenmode); a headset's view is its own\n");
            setCvar(false);
        }
        else if(!levelUp())
        {
            setCvar(false);
        }
        else
        {
            enter();
        }
    }
    else if(!want && cam.on)
    {
        leave("vr_flat_flycam 0");
    }
    if(cam.on && (!flatOn || !levelUp()))
    {
        leave(flatOn ? "the level ended" : "a headset took the game");
        setCvar(false);
    }
    if(cam.on)
    {
        fly();
    }
    testFrame();
}

} // namespace qvr::flycam

// V_CalcRefdef, last: his own eye as everything above made it, remembered (what the hands are posed from: eyeOrg); then,
// flying, the view put at the camera. With the game held his eye is kept as it was when it stopped (the view's bob of a
// run frozen mid-step would otherwise go on swinging it).
extern "C" void VR_FlatFlycamView(float* vieworg, float* viewangles)
{
    using qvr::flycam::cam;
    if(!(cam.on && cam.eyeValid && qvr::flycam::holds()))
    {
        cam.eyeOrg = qvr::geom::vec(vieworg);
        cam.eyeAngles = qvr::geom::vec(viewangles);
        cam.eyeValid = true;
    }
    if(!cam.on)
    {
        return;
    }
    for(int i = 0; i < 3; i++)
    {
        vieworg[i] = cam.pos[i];
    }
    viewangles[PITCH] = cam.turn.x;
    viewangles[YAW] = cam.turn.y;
    viewangles[ROLL] = 0.f;
}

// CL_AccumulateCmd, after the frame's looking (the keys' turn, the mouse, a stick): flying, that look turns the camera and
// his own angles go back to `before` -- what the server and his hands see does not turn.
extern "C" void VR_FlatFlycamLook(const float* before, float* angles)
{
    using qvr::flycam::cam;
    if(!cam.on)
    {
        return;
    }
    const float yaw = za::remainder(angles[YAW] - before[YAW], 360.f);
    const float pitch = angles[PITCH] - before[PITCH];
    cam.turn.y = za::remainder(cam.turn.y + yaw, 360.f);
    cam.turn.x = za::clamp(cam.turn.x + pitch, -qvr::flycam::pitchMost, qvr::flycam::pitchMost);
    for(int i = 0; i < 3; i++)
    {
        angles[i] = before[i];
    }
}

extern "C" int VR_FlatFlycamOwnsInput(void)
{
    return qvr::flycam::on() ? 1 : 0;
}
