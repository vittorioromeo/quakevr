// vr_backend_mock.cpp -- desktop stand-in for an XR runtime.
//
// Reports a standing head and two hands held in front of the body, and renders the eyes into
// its own textures (the left eye is mirrored to the window), so the VR code paths -- including
// stereo rendering -- can run and be tested without a headset.

#include "vr_backend.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_gfx.hpp"
#include "vr_hands.hpp"
#include "vr_held.hpp"
#include "vr_progs.hpp"
#include "vr_twohand.hpp"
#include "vr_units.hpp"
#include "vr_view.hpp"

#include "Zancle/Algorithm/Sort.hpp"
#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/Fabs.hpp"
#include "Zancle/Math/Fmax.hpp"
#include "Zancle/Math/Fmod.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sin.hpp"
#include "Zancle/Math/Tan.hpp"
#include "Zancle/Vocabulary/UniquePtr.hpp"

#include <stdio.h>

namespace qvr
{
namespace
{

// The pretend headset's recommended eye size (its images'), and its largest.
constexpr int imageWidth = 1024;
constexpr int imageHeight = 1024;
constexpr int maxImageSize = 2048;
constexpr float halfIpd = 0.032f;

// Controller input set from the console, for testing without a headset.
InputState mockInput;

[[nodiscard]] int mockHand(const char* name)
{
    if(!q_strcasecmp(name, "main"))
    {
        return HAND_MAIN;
    }
    if(!q_strcasecmp(name, "off"))
    {
        return HAND_OFF;
    }
    return -1;
}

bool setMockButton(int hand, const char* control, bool on);

// vr_mock_button <main|off> <trigger|grip|primary|secondary|stickclick|menu> <0|1>
void mockButton_f()
{
    const int hand = Cmd_Argc() == 4 ? mockHand(Cmd_Argv(1)) : -1;
    if(hand < 0)
    {
        Con_Printf("usage: vr_mock_button <main|off> <trigger|grip|primary|secondary|stickclick|menu> <0|1>\n");
        return;
    }
    if(!setMockButton(hand, Cmd_Argv(2), Q_atoi(Cmd_Argv(3)) != 0))
    {
        Con_Printf("vr_mock_button: unknown control \"%s\"\n", Cmd_Argv(2));
    }
}

bool setMockButton(int hand, const char* control, bool on)
{
    struct Control
    {
        const char* name;
        bool HandInput::*button;
    };
    constexpr Control controls[] = {{"trigger", &HandInput::trigger}, {"grip", &HandInput::grip},
        {"primary", &HandInput::primary}, {"secondary", &HandInput::secondary},
        {"stickclick", &HandInput::stickClick}, {"menu", &HandInput::menu}};

    for(const Control& c : controls)
    {
        if(!q_strcasecmp(control, c.name))
        {
            HandInput& in = mockInput.hands[hand];
            in.*c.button = on;

            // Like a real controller: the fingers follow the trigger and grip, the thumb rests
            // on the face buttons.
            in.triggerValue = in.trigger ? 1.f : 0.f;
            in.gripValue = in.grip ? 1.f : 0.f;
            in.thumbTouch = in.primary || in.secondary || in.stickClick;
            in.triggerTouch = in.trigger;
            return true;
        }
    }
    return false;
}

// vr_mock_fingers <main|off> <trigger> <grip> [<thumb 0|1> [<index on the trigger 0|1>]]: the finger sensors alone
// (analog trigger and grip, 0..1, the thumb resting, the index finger touching the trigger; by default when the trigger
// is pressed at all), without pressing the buttons: for the drawn fingers (round 21's curl sweeps).
void mockFingers_f()
{
    const int hand = Cmd_Argc() >= 4 ? mockHand(Cmd_Argv(1)) : -1;
    if(hand < 0)
    {
        Con_Printf("usage: vr_mock_fingers <main|off> <trigger 0..1> <grip 0..1> [<thumb 0|1> [<index on the trigger 0|1>]]\n");
        return;
    }
    HandInput& in = mockInput.hands[hand];
    in.triggerValue = CLAMP(0.f, static_cast<float>(Q_atof(Cmd_Argv(2))), 1.f);
    in.gripValue = CLAMP(0.f, static_cast<float>(Q_atof(Cmd_Argv(3))), 1.f);
    if(Cmd_Argc() >= 5)
    {
        in.thumbTouch = Q_atoi(Cmd_Argv(4)) != 0;
    }
    in.triggerTouch = Cmd_Argc() >= 6 ? Q_atoi(Cmd_Argv(5)) != 0 : in.triggerValue > 0.f;
}

// vr_mock_hand <main|off|head> <x> <y> <z> [<pitch> <yaw> <roll>]: tracking-space position
// (metres, +x right, +y up, -z forward) and, for a hand, its orientation (degrees: pitch up,
// yaw left, roll right side up; the head's too, which vr_mock_look sets otherwise); "vr_mock_hand
// <main|off|head>" alone restores the standing pose's.
[[nodiscard]] glm::quat mockRotation(float pitch, float yaw, float roll)
{
    return glm::angleAxis(glm::radians(yaw), glm::vec3{0.f, 1.f, 0.f}) *
           glm::angleAxis(glm::radians(pitch), glm::vec3{1.f, 0.f, 0.f}) *
           glm::angleAxis(glm::radians(-roll), glm::vec3{0.f, 0.f, -1.f});
}

constexpr int mockHead = HAND_COUNT;
glm::vec3 mockHandPos[HAND_COUNT + 1];
bool mockHandSet[HAND_COUNT + 1]{};
glm::quat mockHandRot[HAND_COUNT];
bool mockHandRotSet[HAND_COUNT]{};
glm::quat mockHeadOrientation{1.f, 0.f, 0.f, 0.f}; // vr_mock_look, vr_mock_hand head, vr_mock_play

void mockHand_f()
{
    int hand = Cmd_Argc() >= 2 ? mockHand(Cmd_Argv(1)) : -1;
    if(Cmd_Argc() >= 2 && !q_strcasecmp(Cmd_Argv(1), "head"))
    {
        hand = mockHead;
    }
    if(hand < 0 || (Cmd_Argc() != 2 && Cmd_Argc() != 5 && Cmd_Argc() != 8))
    {
        Con_Printf("usage: vr_mock_hand <main|off|head> [<x> <y> <z> [<pitch> <yaw> <roll>]]\n");
        return;
    }
    mockHandSet[hand] = Cmd_Argc() >= 5;
    mockHandPos[hand] = {Q_atof(Cmd_Argv(2)), Q_atof(Cmd_Argv(3)), Q_atof(Cmd_Argv(4))};
    if(hand < HAND_COUNT)
    {
        mockHandRotSet[hand] = Cmd_Argc() == 8;
        mockHandRot[hand] = mockRotation(Q_atof(Cmd_Argv(5)), Q_atof(Cmd_Argv(6)), Q_atof(Cmd_Argv(7)));
    }
    else if(Cmd_Argc() == 8)
    {
        mockHeadOrientation = mockRotation(Q_atof(Cmd_Argv(5)), Q_atof(Cmd_Argv(6)), Q_atof(Cmd_Argv(7)));
    }
}

// vr_mock_play <file>: plays a scripted motion on the clock, so that it runs the same at any frame
// rate (vr_mock_hand moves once per command: scripts paced by "wait" run at the server's rate). The
// file has one keyframe per line, times in seconds from the start:
//   <t> <main|off|head> <x> <y> <z> [<pitch> <yaw> <roll>]   (as vr_mock_hand)
//   <t> button <main|off> <control> <0|1>                     (as vr_mock_button)
//   <t> cmd <console command>                                (e.g. +grabright, -grabright)
// Poses in between are interpolated (positions linearly, orientations by slerp), and the hands
// report the motion's velocities between their keyframes, as a runtime would. At the end the last
// poses stay, as vr_mock_hand leaves them. The head's keyframes with angles turn it too (a lean's tilt: pitch up,
// yaw left, roll as the hands'). "vr_mock_play" alone stops it.
struct PlayKey
{
    double t;
    glm::vec3 pos;
    bool hasRot;
    glm::quat rot;
};
struct PlayButton
{
    double t;
    int hand; // -1: `control` is a console command
    char control[64];
    bool on;
};
za::Vector<PlayKey> playKeys[HAND_COUNT + 1];
za::Vector<PlayButton> playButtons;
za::SizeT playNextButton = 0;
double playStart = -1.0;
double playEnd = 0.0;

void mockPlay_f()
{
    for(auto& keys : playKeys)
    {
        keys.clear();
    }
    playButtons.clear();
    playNextButton = 0;
    playStart = -1.0;
    playEnd = 0.0;
    if(Cmd_Argc() != 2)
    {
        return;
    }

    FILE* file = fopen(Cmd_Argv(1), "r");
    if(!file)
    {
        Con_Printf("vr_mock_play: can't open \"%s\"\n", Cmd_Argv(1));
        return;
    }
    char line[256];
    while(fgets(line, sizeof(line), file))
    {
        double t;
        char what[16], a[16];
        float x, y, z, pitch, yaw, roll;
        int on;
        char command[64];
        if(sscanf(line, "%lf cmd %63[^\r\n]", &t, command) == 2)
        {
            PlayButton b{t, -1, {}, true};
            q_strlcpy(b.control, command, sizeof(b.control));
            playButtons.pushBack(b);
            playEnd = za::max(playEnd, t);
            continue;
        }
        if(sscanf(line, "%lf button %15s %15s %d", &t, what, a, &on) == 4)
        {
            PlayButton b{t, mockHand(what), {}, on != 0};
            q_strlcpy(b.control, a, sizeof(b.control));
            if(b.hand >= 0)
            {
                playButtons.pushBack(b);
                playEnd = za::max(playEnd, t);
            }
            continue;
        }
        const int n = sscanf(line, "%lf %15s %f %f %f %f %f %f", &t, what, &x, &y, &z, &pitch, &yaw, &roll);
        if(n != 5 && n != 8)
        {
            continue;
        }
        const int target = !q_strcasecmp(what, "head") ? mockHead : mockHand(what);
        if(target < 0)
        {
            continue;
        }
        playKeys[target].pushBack({t, {x, y, z}, n == 8,
            n == 8 ? mockRotation(pitch, yaw, roll) : glm::quat{1.f, 0.f, 0.f, 0.f}});
        playEnd = za::max(playEnd, t);
    }
    fclose(file);
    za::insertionSort(playButtons.begin(), playButtons.end(),
        [](const PlayButton& l, const PlayButton& r) { return l.t < r.t; });
    for(auto& keys : playKeys)
    {
        za::insertionSort(keys.begin(), keys.end(), [](const PlayKey& l, const PlayKey& r) { return l.t < r.t; });
    }
    playStart = realtime;
}

// The played motion at realtime `now`: poses into mockHandPos/mockHandRot, and each played hand's
// velocities (tracking space) into `vel`/`angVel` with `played` set.
void playFrame(double now, glm::vec3* vel, glm::vec3* angVel, bool* played)
{
    if(playStart < 0.0)
    {
        return;
    }
    const double t = now - playStart;
    for(int target = 0; target <= HAND_COUNT; target++)
    {
        const za::Vector<PlayKey>& keys = playKeys[target];
        if(keys.empty())
        {
            continue;
        }
        za::SizeT i = 0;
        while(i + 1 < keys.size() && keys[i + 1].t <= t)
        {
            i++;
        }
        const PlayKey& k0 = keys[i];
        const PlayKey& k1 = i + 1 < keys.size() ? keys[i + 1] : keys[i];
        const double span = k1.t - k0.t;
        const float s = span > 0.0 ? static_cast<float>(za::clamp((t - k0.t) / span, 0.0, 1.0)) : 1.f;
        mockHandPos[target] = glm::mix(k0.pos, k1.pos, s);
        mockHandSet[target] = true;
        if(target == mockHead)
        {
            if(k0.hasRot && k1.hasRot)
            {
                mockHeadOrientation = glm::slerp(k0.rot, k1.rot, s);
            }
            continue;
        }
        if(k0.hasRot && k1.hasRot)
        {
            mockHandRot[target] = glm::slerp(k0.rot, k1.rot, s);
            mockHandRotSet[target] = true;
        }
        played[target] = true;
        vel[target] = glm::vec3{0.f};
        angVel[target] = glm::vec3{0.f};
        if(span > 0.0 && t >= k0.t && t <= k1.t)
        {
            vel[target] = (k1.pos - k0.pos) / static_cast<float>(span);
            if(k0.hasRot && k1.hasRot)
            {
                glm::quat d = k1.rot * glm::inverse(k0.rot);
                if(d.w < 0.f)
                {
                    d = -d;
                }
                angVel[target] = glm::axis(d) * (glm::angle(d) / static_cast<float>(span));
            }
        }
    }
    while(playNextButton < playButtons.size() && playButtons[playNextButton].t <= t)
    {
        const PlayButton& b = playButtons[playNextButton++];
        if(b.hand < 0)
        {
            Cbuf_InsertText(va("%s\n", b.control)); // ahead of a script waiting in the buffer
        }
        else
        {
            setMockButton(b.hand, b.control, b.on);
        }
    }
    if(t > playEnd)
    {
        playStart = -1.0;
    }
}

// vr_mock_look <pitch> <yaw>: the head's orientation in degrees (pitch down positive).

void mockLook_f()
{
    if(Cmd_Argc() != 3)
    {
        Con_Printf("usage: vr_mock_look <pitch> <yaw>\n");
        return;
    }
    const float pitch = glm::radians(Q_atof(Cmd_Argv(1)));
    const float yaw = glm::radians(Q_atof(Cmd_Argv(2)));
    mockHeadOrientation =
        glm::angleAxis(yaw, glm::vec3{0.f, 1.f, 0.f}) * glm::angleAxis(-pitch, glm::vec3{1.f, 0.f, 0.f});
}

// vr_mock_hand_to <main|off> <x> <y> <z>: moves the mock hand (its tracking-space position; its orientation kept) so
// that it is at that world point. "vr_mock_hand_to <main|off> weapon <fraction> [<height cm>]": over the weapon lying
// nearest you (a thrown_weapon), `fraction` of the way along its drawn length (0 and 1: its two drawn points farthest
// apart), `height` cm over its top there (0 as shipped). For grab tests: from the hand's place last frame, so a hand
// kept out of the floor lands short of a point under it; run it again (or a few frames on) to follow a weapon.
// "vr_mock_hand_to <main|off> spot <index> [<height cm>]": at its hotspot `index` (a grip's point, a blade's zone's middle:
// view::groundHotspotPoint), `height` cm over it (vr_weapon_grab_hotspots tests). "vr_mock_hand_to <main|off> carried": at
// the handle of the weapon the other hand carries (taking it back).

// The thrown_weapon nearest the player (the server's: its qcvm pushed), or null.
edict_t* nearestThrownWeapon()
{
    edict_t* player = EDICT_NUM(1);
    edict_t* best = nullptr;
    float bestDist = 0.f;
    for(int i = svs.maxclients + 1; i < qcvm->num_edicts; i++)
    {
        edict_t* e = EDICT_NUM(i);
        if(e->free || strcmp(PR_GetString(e->v.classname), "thrown_weapon"))
        {
            continue;
        }
        const float d = glm::distance(glm::vec3{e->v.origin[0], e->v.origin[1], e->v.origin[2]},
            glm::vec3{player->v.origin[0], player->v.origin[1], player->v.origin[2]});
        if(!best || d < bestDist)
        {
            best = e;
            bestDist = d;
        }
    }
    return best;
}

bool weaponSpot(int index, float height, glm::vec3& out)
{
    if(!sv.active || svs.maxclients < 1)
    {
        return false;
    }
    qcvm_t* oldVm = nullptr;
    PR_PushQCVM(&sv.qcvm, &oldVm);
    edict_t* best = nearestThrownWeapon();
    const int num = best ? NUM_FOR_EDICT(best) : 0;
    const char* name = best ? PR_GetString(best->v.netname) : "";
    PR_PopQCVM(oldVm);
    if(!num || !view::groundHotspotPoint(num, index, out))
    {
        return false;
    }
    out.z += height * 0.01f * units::metresToUnits();
    Con_Printf("vr_mock_hand_to: %s, its hotspot %d: %.1f %.1f %.1f\n", name, index, out.x, out.y, out.z);
    return true;
}

bool weaponPoint(float fraction, float height, glm::vec3& out)
{
    if(!sv.active || svs.maxclients < 1)
    {
        return false;
    }
    qcvm_t* oldVm = nullptr;
    PR_PushQCVM(&sv.qcvm, &oldVm);
    edict_t* best = nearestThrownWeapon();
    za::Vector<glm::vec3> verts;
    const bool found = best && held::drawnVertices(best, verts) && !verts.empty();
    const char* name = found ? PR_GetString(best->v.netname) : "";
    if(found)
    {
        // (In its axes from its origin: to the world.)
        const glm::mat3 axes = held::axesFromAngles(best->v.angles, false);
        const glm::vec3 origin{best->v.origin[0], best->v.origin[1], best->v.origin[2]};
        for(glm::vec3& v : verts)
        {
            v = origin + axes * v;
        }
    }
    PR_PopQCVM(oldVm);
    if(!found)
    {
        return false;
    }
    glm::vec3 centre{0.f};
    for(const glm::vec3& v : verts)
    {
        centre += v;
    }
    centre /= static_cast<float>(verts.size());
    const auto farthest = [&](const glm::vec3& from) {
        glm::vec3 pick = from;
        for(const glm::vec3& v : verts)
        {
            if(glm::distance(v, from) > glm::distance(pick, from))
            {
                pick = v;
            }
        }
        return pick;
    };
    const glm::vec3 a = farthest(centre);
    const glm::vec3 b = farthest(a);
    const glm::vec3 axis = glm::normalize(b - a + glm::vec3{0.f, 0.f, 1e-6f});
    const glm::vec3 at = glm::mix(a, b, fraction);
    // Its slice there (the drawn points within a unit and a half along its length): their middle, and its top.
    glm::vec3 mid{0.f};
    float top = -1e9f;
    int n = 0;
    for(const glm::vec3& v : verts)
    {
        if(za::fabs(glm::dot(v - at, axis)) <= 1.5f)
        {
            mid += v;
            top = za::fmax(top, v.z);
            n++;
        }
    }
    out = n ? glm::vec3{mid.x / n, mid.y / n, top} : at;
    out.z += height * 0.01f * units::metresToUnits();
    Con_Printf("vr_mock_hand_to: %s, %.2f of the way along (%.1f units long): %.1f %.1f %.1f\n",
        name, fraction, glm::distance(a, b), out.x, out.y, out.z);
    return true;
}

void mockHandTo_f()
{
    const int hand = Cmd_Argc() >= 2 ? mockHand(Cmd_Argv(1)) : -1;
    const bool weapon = Cmd_Argc() >= 4 && !q_strcasecmp(Cmd_Argv(2), "weapon");
    const bool spot = Cmd_Argc() >= 4 && !q_strcasecmp(Cmd_Argv(2), "spot");
    const bool carried = Cmd_Argc() == 3 && !q_strcasecmp(Cmd_Argv(2), "carried");
    if(hand < 0 || (!weapon && !spot && !carried && Cmd_Argc() != 5))
    {
        Con_Printf("usage: vr_mock_hand_to <main|off> <x> <y> <z>\n"
                   "       vr_mock_hand_to <main|off> weapon <fraction> [<height cm>]\n"
                   "       vr_mock_hand_to <main|off> spot <hotspot index> [<height cm>]\n"
                   "       vr_mock_hand_to <main|off> carried\n");
        return;
    }
    glm::vec3 target{0.f};
    if(!carried && !weapon && !spot)
    {
        target = glm::vec3{Q_atof(Cmd_Argv(2)), Q_atof(Cmd_Argv(3)), Q_atof(Cmd_Argv(4))};
    }
    if(carried && !twohand::carriedHandle(1 - hand, target))
    {
        Con_Printf("vr_mock_hand_to: the other hand carries no weapon\n");
        return;
    }
    const float height = Cmd_Argc() >= 5 ? Q_atof(Cmd_Argv(4)) : 0.f;
    if(weapon && !weaponPoint(Q_atof(Cmd_Argv(3)), height, target))
    {
        Con_Printf("vr_mock_hand_to: no weapon lying about\n");
        return;
    }
    if(spot && !weaponSpot(Q_atoi(Cmd_Argv(3)), height, target))
    {
        Con_Printf("vr_mock_hand_to: no weapon lying about, or no such hotspot (none, or a cup)\n");
        return;
    }
    const hands::State& st = hands::current();
    if(!st.valid)
    {
        Con_Printf("vr_mock_hand_to: the hands aren't known yet\n");
        return;
    }
    // The world's offset in the tracking space's axes (the play space's yaw), in metres.
    glm::vec3 fwd, right, up;
    hands::angleVectors(glm::vec3{0.f, hands::playSpaceYaw(), 0.f}, fwd, right, up);
    const glm::vec3 d = (target - st.pos[hand]) / units::metresToUnits();
    if(!mockHandSet[hand])
    {
        mockHandPos[hand] = standingPose().hands[hand].position;
        mockHandSet[hand] = true;
    }
    mockHandPos[hand] += glm::vec3{glm::dot(d, right), glm::dot(d, up), -glm::dot(d, fwd)};
    Con_Printf("vr_mock_hand_to: %s hand at %.3f %.3f %.3f (tracking)\n", hand == HAND_MAIN ? "main" : "off",
        mockHandPos[hand].x, mockHandPos[hand].y, mockHandPos[hand].z);
}

// vr_mock_camera <x> <y> <z> <pitch> <yaw>: the eyes drawn from there (tracking space, metres; pitch down positive)
// instead of from the head, which stays where it is (the body and the hands with it): a spectator's view of the
// player, for screenshots. "vr_mock_camera" alone: from the head again.
bool mockCameraSet = false;
glm::vec3 mockCameraPos{0.f};
glm::quat mockCameraRot{1.f, 0.f, 0.f, 0.f};

void mockCamera_f()
{
    if(Cmd_Argc() == 1)
    {
        mockCameraSet = false;
        return;
    }
    if(Cmd_Argc() != 6)
    {
        Con_Printf("usage: vr_mock_camera [<x> <y> <z> <pitch> <yaw>]\n");
        return;
    }
    mockCameraSet = true;
    mockCameraPos = {Q_atof(Cmd_Argv(1)), Q_atof(Cmd_Argv(2)), Q_atof(Cmd_Argv(3))};
    mockCameraRot = glm::angleAxis(glm::radians(Q_atof(Cmd_Argv(5))), glm::vec3{0.f, 1.f, 0.f}) *
                    glm::angleAxis(-glm::radians(Q_atof(Cmd_Argv(4))), glm::vec3{1.f, 0.f, 0.f});
}

// vr_mock_stick <main|off> <x> <y>
void mockStick_f()
{
    const int hand = Cmd_Argc() == 4 ? mockHand(Cmd_Argv(1)) : -1;
    if(hand < 0)
    {
        Con_Printf("usage: vr_mock_stick <main|off> <x> <y>\n");
        return;
    }
    mockInput.hands[hand].stick = {Q_atof(Cmd_Argv(2)), Q_atof(Cmd_Argv(3))};
}

// vr_mock_swing: the main hand swings on a 60cm arm around the shoulder, from behind the head
// to in front of the chest and back, with exact velocities (as a runtime reports them).
void swing(Pose& hand, double time, float period)
{
    const glm::vec3 shoulder{0.2f, 1.45f, 0.f};
    constexpr float radius = 0.6f;
    constexpr float centre = 0.6f; // radians forward from straight up
    constexpr float amplitude = 0.9f;

    const float w = 2.f * 3.14159265f / period;
    const float phase = static_cast<float>(za::fmod(time, static_cast<double>(period))) * w;
    const float theta = centre + amplitude * za::sin(phase);
    const float thetaRate = amplitude * w * za::cos(phase);

    // theta 0 is straight up; increasing theta brings the hand forward (-z) and down.
    hand.position = shoulder + radius * glm::vec3{0.f, za::cos(theta), -za::sin(theta)};
    hand.orientation = glm::angleAxis(-(theta - 1.2f), glm::vec3{1.f, 0.f, 0.f});
    hand.linearVelocity = radius * thetaRate * glm::vec3{0.f, -za::sin(theta), -za::cos(theta)};
    hand.angularVelocity = glm::vec3{-thetaRate, 0.f, 0.f};
    hand.velocityValid = true;
}

// The velocity of a pose moved from the console: a scripted move is a jump in one frame, so the
// velocity is measured between moves and held for 40 ms (the engine may run several frames per
// server frame): strokes, throws and swings can be scripted with a move every frame or few.
struct ScriptedMotion
{
    glm::vec3 lastPos{0.f};
    glm::vec3 velocity{0.f};
    double lastMove = 0.0;

    [[nodiscard]] glm::vec3 update(const glm::vec3& pos, double now)
    {
        if(pos != lastPos)
        {
            velocity = lastMove > 0.0 ? (pos - lastPos) / static_cast<float>(za::clamp(now - lastMove, 0.004, 0.05))
                                      : glm::vec3{0.f};
            lastPos = pos;
            lastMove = now;
        }
        else if(now - lastMove > 0.04)
        {
            velocity = glm::vec3{0.f};
        }
        return velocity;
    }
};

class MockBackend final : public Backend
{
public:
    [[nodiscard]] const char* name() const override
    {
        return "mock";
    }

    [[nodiscard]] bool start() override
    {
        ensureTextures();
        return true;
    }

    void stop() override
    {
        for(gfx::Texture& tex : textures)
        {
            gfx::destroyTexture(tex);
            tex = 0;
        }
        width_ = height_ = 0;
    }

    [[nodiscard]] bool beginFrame(TrackingState& tracking, FrameState& frame) override
    {
        if(frameBegun)
        {
            endFrame(false); // the previous frame was not rendered (as OpenXR's backend ends it)
        }
        frameBegun = true;

        glm::vec3 playVel[HAND_COUNT + 1], playAngVel[HAND_COUNT + 1];
        bool played[HAND_COUNT + 1]{};
        playFrame(realtime, playVel, playAngVel, played);

        tracking = standingPose();
        tracking.input = mockInput;
        tracking.time = realtime;
        for(int h = 0; h < HAND_COUNT; h++)
        {
            if(mockHandSet[h])
            {
                tracking.hands[h].position = mockHandPos[h];
            }
            if(mockHandRotSet[h])
            {
                tracking.hands[h].orientation = mockHandRot[h];
            }
        }
        if(mockHandSet[mockHead])
        {
            tracking.head.position = mockHandPos[mockHead];
        }
        tracking.head.orientation = mockHeadOrientation;
        if(vr_mock_shake.value != 0.f || vr_mock_shake_turn.value != 0.f)
        {
            // A shaky head (vr_mock_shake): quick small turns (a few incommensurate sines on each axis) and a small
            // wobble of the position (4 mm a degree), over a slow turn (vr_mock_shake_turn), from when it starts. Driven
            // by realtime, which vr_fixed_frames steps by 1/72 s a frame: the same poses every run.
            double& shakeStart = shake.start;
            int& shakeFrame = shake.frame;
            if(host_framecount > shakeFrame + 1)
            {
                shakeStart = realtime; // the shake's time from when it starts
            }
            shakeFrame = host_framecount;
            const float t = static_cast<float>(realtime - shakeStart);
            const float a = glm::radians(vr_mock_shake.value);
            const auto wave = [t](float f1, float f2, float f3, float phase) {
                return 0.5f * za::sin(6.2831853f * f1 * t + phase) +
                       0.3f * za::sin(6.2831853f * f2 * t + 2.1f * phase + 1.f) +
                       0.2f * za::sin(6.2831853f * f3 * t + 3.7f * phase + 2.f);
            };
            const float yaw = glm::radians(vr_mock_shake_turn.value) * t + a * wave(5.3f, 8.9f, 12.7f, 0.3f);
            const float pitch = a * wave(6.1f, 9.7f, 13.1f, 1.7f);
            const float roll = a * wave(4.9f, 7.3f, 11.3f, 2.9f);
            tracking.head.orientation = glm::angleAxis(yaw, glm::vec3{0.f, 1.f, 0.f}) * mockHeadOrientation *
                                        glm::angleAxis(-pitch, glm::vec3{1.f, 0.f, 0.f}) *
                                        glm::angleAxis(roll, glm::vec3{0.f, 0.f, -1.f});
            const float m = 0.004f * vr_mock_shake.value;
            tracking.head.position += m * glm::vec3{wave(5.7f, 9.1f, 12.1f, 0.9f), wave(6.7f, 8.3f, 11.9f, 2.3f),
                                              wave(4.3f, 7.9f, 10.9f, 1.1f)};
        }
        if(vr_mock_swing.value > 0.f)
        {
            swing(tracking.hands[HAND_MAIN], realtime, vr_mock_swing.value);
        }

        // Hands moved by vr_mock_hand report the velocity of the motion, as a runtime would.
        for(int h = 0; h < HAND_COUNT; h++)
        {
            Pose& hand = tracking.hands[h];
            if(h == HAND_MAIN && vr_mock_swing.value > 0.f)
            {
                continue; // exact velocities
            }
            hand.linearVelocity = handMotion[h].update(hand.position, realtime);
            hand.angularVelocity = glm::vec3{0.f};
            if(played[h])
            {
                hand.linearVelocity = playVel[h]; // vr_mock_play: the motion's own
                hand.angularVelocity = playAngVel[h];
            }
            hand.velocityValid = true;
        }

        // The mock stands for Quest controllers: with vr_controller_legacy_pose its hands are the raw poses the real
        // backend makes of a Touch controller's grip pose, whose grip lies where it would (the Show Controller preview).
        // With vr_mock_grip_velocity, the grip's velocity too, as the OpenXR backend reports it (toLegacyPose): the raw
        // point's, less the turn's swing of the raw point about the grip.
        for(int h = 0; h < HAND_COUNT; h++)
        {
            Pose& hand = tracking.hands[h];
            tracking.gripInHand[h] = vr_controller_legacy_pose.value ? legacyGripInRaw(true, h == HAND_MAIN ? 1 : 0) : GripInRaw{};
            hand.gripVelocityValid = vr_mock_grip_velocity.value != 0.f && vr_controller_legacy_pose.value != 0.f;
            hand.gripVelocity = hand.gripVelocityValid
                                    ? hand.linearVelocity - glm::cross(hand.angularVelocity, -(hand.orientation * tracking.gripInHand[h].offset))
                                    : glm::vec3{0.f};
        }

        // The head likewise (a lunge scripted with vr_mock_hand head).
        tracking.head.linearVelocity = headMotion.update(tracking.head.position, realtime);
        tracking.head.velocityValid = true;
        lastHead = tracking.head;

        frame.shouldRender = true;
        for(int eye = 0; eye < 2; eye++)
        {
            frame.eyes[eye].pose = tracking.head;
            if(mockCameraSet)
            {
                frame.eyes[eye].pose.position = mockCameraPos;
                frame.eyes[eye].pose.orientation = mockCameraRot;
            }
            frame.eyes[eye].pose.position.x += eye == 0 ? -halfIpd : halfIpd;
            frame.eyes[eye].fov = Fov{};
        }

        return true;
    }

    void eyeResolution(int& width, int& height) const override
    {
        width = width_;
        height = height_;
    }

    [[nodiscard]] EyeSizes eyeSizes() const override
    {
        EyeSizes s;
        s.recommendedWidth = imageWidth;
        s.recommendedHeight = imageHeight;
        s.maxWidth = s.maxHeight = maxImageSize;
        s.width = width_;
        s.height = height_;
        return s;
    }

    // vr_mock_hidden_area 1: the corners outside a circle a little wider than the image (about
    // 17% of it, as a headset's lenses hide), to test vr_visibility_mask.
    [[nodiscard]] const HiddenArea* hiddenArea(int /* eye */) const override
    {
        if(vr_mock_hidden_area.value == 0.f)
        {
            return nullptr;
        }
        if(hidden_.vertices.empty())
        {
            makeHiddenArea();
        }
        return &hidden_;
    }

    [[nodiscard]] unsigned acquireEyeImage(int eye) override
    {
        return textures[eye];
    }

    void releaseEyeImage(int /* eye */) override
    {
    }

    // The runtime's panel as a runtime shows it while the eyes are not rendered (menus before a map), placed as
    // OpenXR's is: no image, but the menus' laser pointer meets it (tests of it with vr_mock_hand).
    void endFrame(bool rendered) override
    {
        frameBegun = false;
        if(!rendered && lastHead.valid)
        {
            if(!panelShown && realtime - panelLastShown > 1.0)
            {
                panelPlaced = placeRuntimePanel(lastHead);
            }
            panelLastShown = realtime;
        }
        panelShown = !rendered && lastHead.valid;
    }

    [[nodiscard]] bool runtimePanel(Pose& pose, glm::vec2& size) const override
    {
        if(!panelShown)
        {
            return false;
        }
        const glm::vec2 canvas{static_cast<float>(glwidth), static_cast<float>(glheight)};
        pose = panelPlaced;
        size = {runtimePanelWidth, canvas.x > 0.f ? runtimePanelWidth * canvas.y / canvas.x : 0.f};
        return true;
    }

private:
    bool frameBegun{false};
    Pose lastHead;
    Pose panelPlaced;
    bool panelShown{false};
    double panelLastShown{-10.0};

    gfx::Texture textures[2]{};
    int width_{0};
    int height_{0};
    mutable HiddenArea hidden_;

    // The eye images, at the recommended size like a runtime's (vr_render_scale does not change
    // them: the eyes are resampled into them).
    void ensureTextures()
    {
        if(textures[0])
        {
            return;
        }
        // vr_mock_eye_size: other sizes, to measure the resample (vr_upscale) at a headset's.
        const int size = vr_mock_eye_size.value > 0.f
            ? za::clamp(static_cast<int>(vr_mock_eye_size.value), 256, maxImageSize)
            : imageWidth;
        for(gfx::Texture& tex : textures)
        {
            tex = gfx::createTexture(size, size);
        }
        width_ = size;
        height_ = size;
    }

    // Between a circle of radius 1.04 (the image's half-width 1) and the image's edge, in quads
    // from the circle out to the edge along rays from the middle (the corners among them), scaled
    // to the mock's field of view (Fov{}: 0.8 radians each way).
    void makeHiddenArea() const
    {
        constexpr int segments = 64; // a multiple of 8: the corners' rays are among them
        constexpr float radius = 1.04f;
        const float tangent = za::tan(Fov{}.right);
        for(int i = 0; i < segments; i++)
        {
            const float a = 2.f * 3.14159265f * static_cast<float>(i) / segments;
            const glm::vec2 dir{za::cos(a), za::sin(a)};
            const float toEdge = 1.f / za::max(za::fabs(dir.x), za::fabs(dir.y));
            hidden_.vertices.pushBack(dir * za::min(radius, toEdge) * tangent); // inner
            hidden_.vertices.pushBack(dir * toEdge * tangent);                      // outer
        }
        for(int i = 0; i < segments; i++)
        {
            const za::U32 in0 = 2 * i, out0 = 2 * i + 1;
            const za::U32 in1 = 2 * ((i + 1) % segments), out1 = in1 + 1;
            for(za::U32 v : {in0, out0, out1, in0, out1, in1})
            {
                hidden_.indices.pushBack(v);
            }
        }
    }
    ScriptedMotion handMotion[HAND_COUNT];
    ScriptedMotion headMotion; // headbutts
    struct
    {
        double start{-1.0}; // realtime when the shake started (vr_mock_shake)
        int frame{-10};      // the last frame it moved the head (host_framecount): a gap starts it again
    } shake;
};

} // namespace

TrackingState standingPose()
{
    constexpr float eyeHeight = 1.7f;

    TrackingState out;
    out.head.position = {0.f, eyeHeight, 0.f};
    out.head.valid = true;

    // Hands at chest height, 40cm forward, 20cm to each side.
    out.hands[HAND_OFF].position = {-0.2f, eyeHeight - 0.4f, -0.4f};
    out.hands[HAND_MAIN].position = {0.2f, eyeHeight - 0.4f, -0.4f};
    for(Pose& hand : out.hands)
    {
        hand.valid = true;
    }

    return out;
}

void registerMockCommands()
{
    Cmd_AddCommand("vr_mock_button", mockButton_f);
    Cmd_AddCommand("vr_mock_stick", mockStick_f);
    Cmd_AddCommand("vr_mock_hand", mockHand_f);
    Cmd_AddCommand("vr_mock_hand_to", mockHandTo_f);
    Cmd_AddCommand("vr_mock_look", mockLook_f);
    Cmd_AddCommand("vr_mock_camera", mockCamera_f);
    Cmd_AddCommand("vr_mock_fingers", mockFingers_f);
    Cmd_AddCommand("vr_mock_play", mockPlay_f);
}

za::UniquePtr<Backend> makeMockBackend()
{
    return za::makeUnique<MockBackend>();
}

} // namespace qvr
