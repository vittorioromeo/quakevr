// vr_navfollow.hpp -- the navmesh follower (docs/vr-port/NAVMESH.md, section 22): a path to a goal on the navmesh, its
// corners, and the input that walks to the next one (a view yaw, forward, a jump), as a human's keys would. Built to be
// reused: today the debug autopilot vr_nav_follow (a player walked after another player); later FrikBot's long-range
// navigation (NAVMESH.md 22, the plan).
//
// vr_nav_follow <name | slot | next | 0>, typed in a player's own console (a listen host's or a remote client's): the
// server plans his path to that player (its navmesh: vr_nav.cpp's followPath) every vr_nav_follow_repath seconds and
// sends him its corners (QVR_SVC_NAVFOLLOW) when they change; his client steers along them in the move it sends
// (VR_AdjustMove: the view turned to the next corner, forward pressed), so the walk goes through the moves a human's do
// (prediction, the server's moves, lag compensation unchanged). Any move, look or jump of his own hands control back at
// once; the follow resumes after vr_nav_follow_idle seconds without one. 0 turns it off.

#pragma once

#include "vr_engine.hpp"
#include "vr_nav.hpp"
#include "Zancle/Base/IntTypes.hpp"

namespace qvr::navfollow
{

// ---- The follower (both sides: the plan where the navmesh is, the steering where the moves are made) ----------------

constexpr int maxCorners = 32;

// A follower's plan (server side: the navmesh is the server's): the path's corners to its goal, found again every
// vr_nav_follow_repath seconds. state: what the last plan found (State).
enum State : int
{
    stateOff = 0,      // not following
    stateOn = 1,       // a path (perhaps partial: no way all the way)
    stateBuilding = 2, // no navmesh for a player's size yet (its build on the pool)
    stateNoPath = 3,   // his feet or the goal off the mesh
};
struct Plan
{
    nav::PathCorner corners[maxCorners];
    int count = 0;
    bool partial = false;
    int state = stateOff;
    double repathAt = 0.0;
};

// The plan of `self` (a walking edict: a player, a bot) to `goal`'s feet, found again when due (now: the server's time).
// -> true when it changed enough to pass on (its state, partial, or any corner but the first, where it stands, by over 4
// units).
bool replan(Plan& p, const edict_t* self, const edict_t* goal, double now);

// The steering: along the corners (the first where the plan found it), from its feet now.
struct Steer
{
    za::U32 serial = 0;   // the corners' it steers along (a new plan starts it again)
    int next = 1;         // the corner it walks to
    glm::vec3 stuckAt{0.f};
    double stuckSince = -1.0;
    bool arrived = false;
};
// What to do this frame: the yaw to face (world degrees), whether to walk forward, jump (stuck half a second walking:
// a human's hop over a lip), arrived (within `stop` units of the end along the path: stands facing it).
struct Input
{
    float yaw = 0.f;
    bool move = false, jump = false, arrived = false, standing = false;
};
[[nodiscard]] Input steer(Steer& s, const nav::PathCorner* corners, int count, za::U32 serial, const glm::vec3& feet,
                          double now, float stop);

// ---- Bots: FrikBot's walk on the navmesh (vr_bot_navmesh; NAVMESH.md 23) --------------------------------------------

// The builtin navbot(bot, goal, what) (QC frikbot/bot_nav.qc). what 0: `bot`'s plan to `goal` (kept per bot, by its client
// slot: found again every vr_nav_follow_repath of server time, at once for a new goal) and its steering -> the corner to
// walk to (its feet; the bot's own feet when it has no path). 1: that step -> (its plan's State, its BotFlags, the path
// left in units). 2: its plan dropped. 3: a one-off probe from its feet to the goal's -> (1 a whole path, 0 none or
// partial, -1 no navmesh for its size yet; its length; its corners).
enum BotFlags : int
{
    botWalk = 1,    // walk toward the corner
    botJump = 2,    // the follower's hop (stuck half a second walking)
    botArrived = 4, // within its stop of the goal along the path (an item: on it; a player: 48 units)
    botPartial = 8, // the path stops short of the goal
};
[[nodiscard]] glm::vec3 botStep(const edict_t* bot, const edict_t* goal, int what);

// ---- vr_nav_follow ---------------------------------------------------------------------------------------------------

void init(); // the commands

// Server: each server frame (VR_PhysicsFrameEnd): each following player's plan, its corners sent him when they change.
void serverFrame();

// Client: QVR_SVC_NAVFOLLOW's body.
void parse();
// Client: the move about to be sent (VR_AdjustMove, keys and sticks already in): following and not handed back, the view
// turned toward the next corner and the move replaced by the follower's -> true. `stick`: a stick asks to move.
bool clientMove(float* forwardmove, float* sidemove, float* upmove, bool stick);
// Client: the move's jump bit asked by the follower this frame (VR_MoveJump).
[[nodiscard]] bool jumpNow();
// Client: QVR_SVC_BOTPATH's body (a bot's navmesh path, for the debug draw).
void parseBotPath();
// The view's debug draws (vr_nav_debug on): the follower's path (yellow, a teleporter's cyan) and its next corner; each
// bot's navmesh path (vr_bot_navmesh: magenta, its next corner a white post), on the host and on a remote client alike
// (asked of the server while vr_nav_debug is on: "vr_nav_botpaths 1"; vr_nav_follow_status's navbots line counts them).
void debugDraw();

} // namespace qvr::navfollow
