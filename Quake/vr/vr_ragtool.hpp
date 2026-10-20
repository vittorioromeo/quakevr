// vr_ragtool.hpp -- the ragdoll tool (docs/vr-port/RAGTOOL.md): a flat-screen workbench to watch the active ragdolls'
// behaviours (vr_ragactive.cpp, vr_ragstruggle.cpp, the knockdowns and staggers of vr_box3d_knockdown.cpp) on a monster
// of one's choice, slowed, paused or stepped, with their skeleton, joint limits, motor targets, contacts and balance drawn
// over them, and their settings edited live from a registry grouped by behaviour (saved as presets: cfg files of cvars).
//
// It is the game itself, in a mode: `-ragtool` on the command line (it implies -flatscreenmode) or `vr_ragtool` in the
// console loads a small room (vr_ragtool_map), makes the player a notarget, god-mode onlooker and spawns the subject
// (vr_ragtool_model, the toolgun's spawn list's monsters with a rig) in front of him. The behaviours are the game's own,
// set going through the same calls the game and its tests make (vr_ragdoll_act, vr_knockdown_test, QC's parry, the
// physgun's hold as a hand). The camera orbits the subject (the mouse turns it, the wheel zooms); the VR menu's Ragdoll
// Tool page (Tab) is its panel. Off (no flag, no vr_ragtool), nothing here runs and the game is as it was.
#pragma once
#include "vr_engine.hpp"

namespace qvr::ragtool
{
// The mode is on: vr_ragtool was run (or -ragtool given) and vr_ragtool_exit has not been since.
[[nodiscard]] bool active();
// -ragtool was given (vr_flatplay.cpp: it implies -flatscreenmode).
[[nodiscard]] bool flagGiven();
// VR_Init: reads the flag, registers the commands.
void init();
// The server's frame (vr_box3d_step.cpp, with ragtest::frame): the behaviour under way's next step, the hand, the log.
void serverFrame();
// VR_BeginFrame (with flatplay::frame): -ragtool's start, once.
void clientFrame();
// The view's debug draws (vr_view.cpp): the subject's skeleton, limits, targets, contacts and balance.
void draw();
// The menu's rows (vr_menu_ragtool.inc).
struct Choice
{
    float value;
    const char* label;
};
[[nodiscard]] int modelCount();
[[nodiscard]] const char* modelLabel(int i);
[[nodiscard]] int behaviourCount();
[[nodiscard]] const char* behaviourLabel(int i);
[[nodiscard]] const char* behaviourHelp(int i);
// The parameter registry: each entry a cvar of a behaviour group, with its slider's range.
struct Param
{
    int group;
    const char* cvar;
    const char* label;
    float min, max, step;
    const char* format;
    bool respawn; // takes effect on the next spawn (a joint's limit: set as the ragdoll is made)
};
[[nodiscard]] int groupCount();
[[nodiscard]] const char* groupName(int g);
[[nodiscard]] int paramCount();
[[nodiscard]] const Param& param(int i);
// The status line over the panel (the subject, the behaviour, its time, paused or not, the speed).
[[nodiscard]] const char* statusLine();
} // namespace qvr::ragtool

// The physics side (vr_box3d_ragtool.cpp): what the tool reads of and does to the subject's ragdoll.
namespace qvr::ragtool::physics
{
struct Show
{
    bool bones{true};    // each part's frame (red x, green y, blue z) and its bone, pivot to end
    bool limits{true};   // each joint's limit: a ball's cone, a hinge's arc
    bool targets{true};  // the drive's targets against where the parts are (ragactive::debugDraw)
    bool contacts{true}; // where the parts touch the world
    bool balance{true};  // the centre of mass, its point on the floor, the feet's support
    float scale{1.f};    // the lines' width and the points' size times this (a far camera's: wider)
};
// Where the camera looks: the pelvis's middle (a ragdoll) or the box's (standing). False: no such entity.
[[nodiscard]] bool focus(int num, glm::vec3& out);
void draw(int num, const Show& show);
// The kinds of hold the virtual hand takes (vr_ragtool_hold).
enum class Hold : int
{
    Chest,
    Head,
    Arm,
    Leg,
    Pelvis,
    Count
};
[[nodiscard]] const char* holdName(Hold h);
// The physgun's hold on `num`'s part of that kind (its grip: out). False: no ragdoll, or no such part.
bool takeHold(int num, Hold h, glm::vec3& grip);
// A sample of the motion, for the log (each a behaviour's run's).
struct Sample
{
    bool ragdoll{false};
    bool driven{false};
    glm::vec3 pelvis{0.f}; // units
    glm::vec3 com{0.f};    // units
    float error{0.f};      // degrees: the driven joints' mean miss of their targets
    float strength{0.f};   // the driven joints' mean strength
    float floor{0.f};      // the floor under the centre of mass (z, units)
};
[[nodiscard]] Sample sample(int num);
} // namespace qvr::ragtool::physics
