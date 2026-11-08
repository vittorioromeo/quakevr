// vr_teleport.cpp -- see vr_teleport.hpp. Ported from the old engine's VR_DoTeleportation.
//
// While +teleport is held, a small box is swept from the player along the off hand's aim, up
// to vr_teleport_range units; the spot is valid when it lands on a floor or a gentle slope
// (normal z 0.75 .. 1), and is marked in blue if valid, red otherwise. Letting
// go on a valid spot sends it with the move (VRBITS0_TELEPORTING); the server moves the
// player there. The aim is drawn as a line with a dot at the destination (vr_lines). The sweep
// needs the local server's world, so it only works when hosting.
//
// Smooth + Teleport (vr_teleport_enabled 2; ROUND24.md "Teleport and smooth locomotion together"): the moving stick walks
// as ever and the turning stick's push forward aims with that stick's hand (vr_input.cpp routeSticks: back to the middle
// lands, pulled back cancels; its left and right still turn, never while it aims). +teleport works too. Every teleport in
// this mode is from the ground only (never in the air, hanging from a ledge or swimming: red), and arrives standing
// still (VR_ClientTeleport: no walk's or shove's momentum carried through).

#include "vr_teleport.hpp"
#include "vr_engine.hpp"
#include "vr_backend.hpp"
#include "vr_climb.hpp"
#include "vr_cvars.hpp"
#include "vr_geom.hpp"
#include "vr_hue.hpp"
#include "vr_lines.hpp"
#include "vr_main.hpp"
#include "vr_progs.hpp"
#include "vr_testkit.hpp"
#include "vr_trace.hpp"

extern "C" kbutton_t in_up; // cl_input.c (+moveup: the turning stick's push forward outside Smooth + Teleport)

namespace qvr::teleport
{
namespace
{

bool held = false;      // +teleport
bool stickHeld = false; // the turning stick aiming (Smooth + Teleport: stickAim)
int stickHand = HAND_MAIN;
int aimHand = HAND_OFF; // the hand this aim began with: the off hand for +teleport, the stick's for the stick
bool wasHeld = false;
bool impactValid = false;
glm::vec3 impact{0.f};
int sent = 0; // teleports sent (vr_teleport_both_test's count)

void teleportDown_f()
{
    held = true;
}

void teleportUp_f()
{
    held = false;
}

[[nodiscard]] int mode()
{
    return static_cast<int>(vr_teleport_enabled.value);
}

// Smooth + Teleport's rule: from the ground only (this machine's server's player; VR_ClientTeleport checks again).
[[nodiscard]] bool standing()
{
    const edict_t* p = progs::localPlayer();
    return p && (static_cast<int>(p->v.flags) & FL_ONGROUND) != 0 && p->v.waterlevel < 2.f && !climb::holding(HAND_OFF) &&
           !climb::holding(HAND_MAIN);
}

[[nodiscard]] bool trace(const hands::State& s, int hand)
{
    const glm::vec3 end = s.pos[hand] + hands::forward(s.rot[hand]) * vr_teleport_range.value;
    const auto found = worldtrace::move(s.playerOrigin, {-6.f, -6.f, -12.f}, {6.f, 6.f, 12.f}, end, MOVE_NORMAL);
    if(!found)
    {
        return false;
    }
    const trace_t& tr = *found;

    impact = {tr.endpos[0], tr.endpos[1], tr.endpos[2] + 12.f}; // player origin is above the feet

    // Floors and slopes, not walls or ceilings.
    return tr.fraction < 1.f && !tr.allsolid && tr.plane.normal[2] >= 0.75f;
}

void drawAim(const hands::State& s, int hand)
{
    // Where it can land in the player's hue (vr_teleport_hue; made in blue, 228), red where it can't.
    const glm::vec4 color = impactValid ? hue::color(vr_teleport_hue, 0.8f, 1.f, 0.7f) : glm::vec4{1.f, 0.f, 0.f, 0.5f};
    const glm::vec4 faded{color.r, color.g, color.b, 0.f};

    const glm::vec3 start = s.pos[hand];
    const glm::vec3 feet = impact - glm::vec3{0.f, 0.f, 12.f};
    const glm::vec3 fadedEnd = glm::mix(start, feet, 0.15f);
    lines::line(start, fadedEnd, 0.6f, faded, color);
    lines::line(fadedEnd, feet, 0.6f, color, color);
    lines::point(feet, 12.f, color);
}

// ---- vr_teleport_both_test (Debug > Tests > Teleport: Smooth + Teleport): Smooth + Teleport on the mock headset, from
// vrtesthall's open floor (or where you stand: "here"). The moving stick walks and never teleports; the turning stick's
// push forward aims (no turning, no +moveup, the moving stick not walking meanwhile) and its return lands on the aimed
// spot standing still (momentum from the walk not carried); pulled back cancels; left and right still snap-turn; no
// teleport from the air. Then the single modes as before: 1 the stick's forward is +moveup and +teleport aims, 0 nothing
// aims. Then the sticks swapped (vr_stick_swap 1): the off hand's stick aims, the main hand's walks. Ends with
// "teleport both: N passed, M failed" (each phase gives up after a few seconds: FAIL "timed out").
constexpr int bothPhaseFrames = 360; // a phase's safety net (5 s of game time)

struct BothTest
{
    bool active{false};
    bool here{false};
    int phase{0};
    int frames{0}; // in this phase
    testkit::Tally tally;
    glm::vec3 mark{0.f}; // the origin when the phase's measure began
    float yaw{0.f};      // the view's yaw then
    float speed{0.f};    // the horizontal speed then
    int sentAt{0};       // teleports sent then
    glm::vec3 aimed{0.f};
    char saved[4][32]{}; // vr_teleport_enabled, vr_snap_turn, vr_stick_swap, vr_enable_joystick_turn
};
BothTest both;

cvar_t* const bothCvars[4] = {&vr_teleport_enabled, &vr_snap_turn, &vr_stick_swap, &vr_enable_joystick_turn};

void bothCheck(bool ok, const char* what)
{
    if(both.tally.check(ok, "teleport both", what))
    {
        Con_Printf("teleport both: ok %s\n", what);
    }
}

[[nodiscard]] glm::vec3 bothOrigin(const edict_t* p)
{
    return {p->v.origin[0], p->v.origin[1], p->v.origin[2]};
}

[[nodiscard]] float bothSpeed(const edict_t* p)
{
    return glm::length(glm::vec2{p->v.velocity[0], p->v.velocity[1]});
}

[[nodiscard]] float bothFlat(glm::vec3 a, glm::vec3 b)
{
    return glm::length(glm::vec2{a.x - b.x, a.y - b.y});
}

[[nodiscard]] float bothYaw()
{
    return hands::current().headAngles.y;
}

[[nodiscard]] bool bothGrounded(const edict_t* p)
{
    return (static_cast<int>(p->v.flags) & FL_ONGROUND) != 0;
}

// Back to vrtesthall's open floor facing its 640 free units (unless "here").
void bothPlace(edict_t* p)
{
    if(both.here)
    {
        return;
    }
    progs::ServerVm serverVm;
    const float at[3] = {0.f, -288.f, 25.f};
    for(int k = 0; k < 3; k++)
    {
        p->v.origin[k] = p->v.oldorigin[k] = at[k];
        p->v.velocity[k] = 0.f;
    }
    p->v.angles[PITCH] = p->v.angles[ROLL] = p->v.v_angle[PITCH] = p->v.v_angle[ROLL] = 0.f;
    p->v.angles[YAW] = p->v.v_angle[YAW] = 90.f;
    p->v.fixangle = 1;
    SV_LinkEdict(p, true);
}

void bothNext(const edict_t* p, const char* commands)
{
    both.phase++;
    both.frames = 0;
    both.mark = bothOrigin(p);
    both.yaw = bothYaw();
    both.speed = bothSpeed(p);
    both.sentAt = sent;
    if(commands)
    {
        Cbuf_AddText(commands);
    }
}

void bothFinish()
{
    for(int i = 0; i < 4; i++)
    {
        Cvar_SetQuick(bothCvars[i], both.saved[i]);
    }
    Cbuf_AddText("-teleport; -jump; vr_mock_stick main 0 0; vr_mock_stick off 0 0; vr_mock_hand main; vr_mock_hand off\n");
    both.tally.verdict("teleport both");
    both.active = false;
}

void bothTimeout(const char* what)
{
    bothCheck(false, va("timed out: %s", what));
    bothFinish();
}

// Phases: each waits on its condition (the player, an aim, a landing) or a counted span the logic needs (a walk's
// frames, a settle), and gives up after bothPhaseFrames.
void bothTick()
{
    edict_t* p = progs::localPlayer();
    if(!p)
    {
        if(++both.frames > bothPhaseFrames)
        {
            bothTimeout("no player");
        }
        return;
    }
    both.frames++;
    const float moved = bothFlat(bothOrigin(p), both.mark);
    const float turned = za::fabs(geom::yawDelta(bothYaw(), both.yaw));
    const bool aiming = wasHeld;
    if(aiming)
    {
        both.aimed = impact; // (the last aim's spot: where a release sends the player)
    }
    switch(both.phase)
    {
        case 0: // place the player, the hands pointing down ahead
            if(!bothGrounded(p) || !vrActive())
            {
                break;
            }
            Cvar_SetQuick(&vr_teleport_enabled, "2");
            Cvar_SetQuick(&vr_snap_turn, "45");
            Cvar_SetQuick(&vr_stick_swap, "0");
            Cvar_SetQuick(&vr_enable_joystick_turn, "1");
            bothPlace(p);
            bothNext(p, "vr_mock_stick main 0 0; vr_mock_stick off 0 0; vr_mock_hand main 0.2 1.1 -0.3 60 0 0; "
                        "vr_mock_hand off -0.2 1.1 -0.3 60 0 0\n");
            return;
        case 1: // settled (the place and the turn taken, on the ground): walk with the moving stick
            if(both.frames < 24 || !bothGrounded(p))
            {
                break;
            }
            bothNext(p, "vr_mock_stick off 0 1\n");
            return;
        case 2: // half a second's walk: moved, no aim, no teleport, no turn
            if(both.frames < 36)
            {
                if(aiming)
                {
                    bothCheck(false, va("walking aims nothing (frame %d)", both.frames));
                }
                break;
            }
            bothCheck(moved > 48.f && sent == both.sentAt && turned < 0.5f,
                va("the moving stick walks: %.0f units, %d teleports, turned %.1f", moved, sent - both.sentAt, turned));
            bothNext(p, "vr_mock_stick main 0 1\n"); // (still walking: the aim begins at a run)
            return;
        case 3: // the turning stick forward: aiming, onto a valid spot
            if(!aiming || !impactValid)
            {
                break;
            }
            bothNext(p, nullptr);
            return;
        case 4: // eight frames' aim, the moving stick still pushed: slowing (not walking), not turning, no +moveup
            if((in_up.state & 1) != 0)
            {
                bothCheck(false, "the aim is no +moveup");
                bothFinish();
                return;
            }
            if(both.frames < 8)
            {
                break;
            }
            bothCheck(aiming && turned < 0.5f && bothSpeed(p) < both.speed - 10.f,
                va("aiming: no turn (%.1f), no walk (speed %.0f -> %.0f)", turned, both.speed, bothSpeed(p)));
            bothNext(p, "vr_mock_stick main 0 0; vr_mock_stick off 0 0\n");
            both.speed = bothSpeed(p);
            return;
        case 5: // let go: the landing, on the aimed spot, standing still
            if(sent == both.sentAt || glm::distance(bothOrigin(p), both.aimed) > 1.f)
            {
                break;
            }
            bothCheck(sent == both.sentAt + 1 && moved > 48.f && glm::distance(bothOrigin(p), both.aimed) < 1.f &&
                          bothSpeed(p) < 1.f && turned < 0.5f,
                va("jumped %.0f units, landed %.1f from the aimed spot, speed %.0f before -> %.1f, turned %.1f", moved,
                    glm::distance(bothOrigin(p), both.aimed), both.speed, bothSpeed(p), turned));
            bothNext(p, nullptr);
            return;
        case 6: // settle, then aim again
            if(both.frames < 12)
            {
                break;
            }
            bothNext(p, "vr_mock_stick main 0 1\n");
            return;
        case 7: // aiming: pull back to cancel
            if(!aiming)
            {
                break;
            }
            bothNext(p, "vr_mock_stick main 0 -1\n");
            return;
        case 8: // the aim ended: back to the middle
            if(aiming)
            {
                break;
            }
            bothNext(p, "vr_mock_stick main 0 0\n");
            return;
        case 9: // a dozen frames: no teleport, no turn
            if(both.frames < 12)
            {
                break;
            }
            bothCheck(sent == both.sentAt && turned < 0.5f && moved < 1.f,
                va("pulled back cancels: %d teleports, turned %.1f, moved %.1f", sent - both.sentAt, turned, moved));
            bothNext(p, "vr_mock_stick main 1 0\n");
            return;
        case 10: // the turning stick's right: a 45-degree snap, no aim
            if(turned < 1.f && both.frames < 12)
            {
                break;
            }
            bothCheck(za::fabs(turned - 45.f) < 1.f && !aiming && sent == both.sentAt,
                va("left and right still snap-turn: %.1f degrees, aiming %d", turned, aiming ? 1 : 0));
            bothNext(p, "vr_mock_stick main 0 0\n");
            return;
        case 11: // settle, then jump
            if(both.frames < 12)
            {
                break;
            }
            bothNext(p, "+jump\n");
            return;
        case 12: // in the air: aim
            if(bothGrounded(p))
            {
                break;
            }
            bothNext(p, "-jump; vr_mock_stick main 0 1\n");
            return;
        case 13: // aiming in the air: red; let go while still in the air
            if(!aiming)
            {
                break;
            }
            bothCheck(!impactValid && !bothGrounded(p), va("in the air the aim is red (valid %d)", impactValid ? 1 : 0));
            bothNext(p, "vr_mock_stick main 0 0\n");
            return;
        case 14: // landed by falling, a few frames on: no teleport
            if(!bothGrounded(p) || both.frames < 4)
            {
                break;
            }
            bothCheck(sent == both.sentAt, va("no teleport from the air: %d sent", sent - both.sentAt));
            Cvar_SetQuick(&vr_teleport_enabled, "1");
            bothPlace(p);
            bothNext(p, "vr_mock_stick main 0 1\n");
            return;
        case 15: // Teleport 1 (unchanged): the turning stick's forward is +moveup, no aim
            if(both.frames < 8)
            {
                break;
            }
            bothCheck(!aiming && (in_up.state & 1) != 0, va("Teleport 1: the stick's forward is +moveup, aiming %d", aiming ? 1 : 0));
            bothNext(p, "vr_mock_stick main 0 0; +teleport\n");
            return;
        case 16: // ... and +teleport aims with the off hand
            if(!aiming || !impactValid)
            {
                break;
            }
            bothCheck(aimHand == HAND_OFF, "Teleport 1: +teleport aims with the off hand");
            bothNext(p, "-teleport\n");
            return;
        case 17:
            if(sent == both.sentAt || glm::distance(bothOrigin(p), both.aimed) > 1.f)
            {
                break;
            }
            bothCheck(true, "Teleport 1: +teleport lands");
            Cvar_SetQuick(&vr_teleport_enabled, "0");
            bothNext(p, "+teleport; vr_mock_stick main 0 1\n");
            return;
        case 18: // Teleport 0: neither aims
            if(both.frames < 12)
            {
                break;
            }
            bothCheck(!aiming && sent == both.sentAt, "Teleport 0: neither +teleport nor the stick aims");
            Cvar_SetQuick(&vr_teleport_enabled, "2");
            Cvar_SetQuick(&vr_stick_swap, "1");
            bothPlace(p);
            bothNext(p, "-teleport; vr_mock_stick main 0 0\n");
            return;
        case 19: // the sticks swapped (vr_stick_swap 1): the off hand's stick aims
            if(both.frames < 12)
            {
                break;
            }
            bothNext(p, "vr_mock_stick off 0 1\n");
            return;
        case 20:
            if(!aiming || !impactValid)
            {
                break;
            }
            bothCheck(aimHand == HAND_OFF, "sticks swapped: the off hand's stick aims with the off hand");
            bothNext(p, "vr_mock_stick off 0 0\n");
            return;
        case 21:
            if(sent == both.sentAt || glm::distance(bothOrigin(p), both.aimed) > 1.f)
            {
                break;
            }
            bothCheck(bothSpeed(p) < 1.f, "sticks swapped: lands");
            bothNext(p, "vr_mock_stick main 0 1\n");
            return;
        case 22: // ... and the main hand's walks
            if(both.frames < 36)
            {
                if(aiming)
                {
                    bothCheck(false, va("sticks swapped: walking aims nothing (frame %d)", both.frames));
                }
                break;
            }
            bothCheck(moved > 48.f && sent == both.sentAt,
                va("sticks swapped: the main hand's stick walks: %.0f units, %d teleports", moved, sent - both.sentAt));
            bothFinish();
            return;
        default: bothFinish(); return;
    }
    if(both.frames > bothPhaseFrames)
    {
        bothTimeout(va("phase %d (teleports %d, moved %.0f, aiming %d, valid %d, on the ground %d)", both.phase,
            sent - both.sentAt, moved, aiming ? 1 : 0, impactValid ? 1 : 0, bothGrounded(p) ? 1 : 0));
    }
}

void bothTest_f()
{
    if(both.active)
    {
        Con_Printf("teleport both: already running\n");
        return;
    }
    // (Run with the map's load, as headless: phase 0 waits for the player in VR, on the ground.)
    both = BothTest{};
    both.here = Cmd_Argc() > 1 && !q_strcasecmp(Cmd_Argv(1), "here");
    for(int i = 0; i < 4; i++)
    {
        q_strlcpy(both.saved[i], bothCvars[i]->string, sizeof(both.saved[i]));
    }
    both.active = true;
    Con_Printf("teleport both: start%s\n", both.here ? " (here)" : " (vrtesthall's floor)");
}

} // namespace

void init()
{
    Cmd_AddCommand("+teleport", teleportDown_f);
    Cmd_AddCommand("-teleport", teleportUp_f);
    Cmd_AddCommand("vr_teleport_both_test", bothTest_f);
}

bool stickMode()
{
    return mode() == 2;
}

void stickAim(int hand, bool on, bool cancel)
{
    stickHeld = on && !cancel;
    stickHand = hand;
    if(cancel)
    {
        impactValid = false; // (never sent: wasHeld's release needs a valid spot)
    }
}

bool update(const hands::State& s, glm::vec3& target)
{
    if(both.active)
    {
        bothTick();
    }

    const bool button = held && mode() != 0;
    const bool stick = stickHeld && stickMode();
    const bool aiming = (button || stick) && s.valid;
    if(aiming && !wasHeld)
    {
        aimHand = stick ? stickHand : HAND_OFF;
    }

    bool release = false;
    if(aiming)
    {
        impactValid = trace(s, aimHand) && (!stickMode() || standing());
        drawAim(s, aimHand);
    }
    else if(wasHeld && impactValid)
    {
        release = true;
        target = impact;
        impactValid = false;
        sent++;
    }

    wasHeld = aiming;
    return release;
}

} // namespace qvr::teleport
