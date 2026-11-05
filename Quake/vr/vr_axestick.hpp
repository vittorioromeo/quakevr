// vr_axestick.hpp -- thrown axes stick (ROUND21.md, "Thrown axes stick", "Thrown axes: the blade decides"): a thrown
// Embedded blades (vr_embed; ROUND23.md, "Embedded blades"): weapons.txt's `embed` says which weapons stick and how: by the
// edge as below (the axes), or by the point (swords: vr_axestick.cpp stickByPoint), also in bodies lying down.
// axe whose blade goes into something first -- its edge, its corners or the blade just behind them
// (vr_axestick_leniency: how much of it), not the handle or the head's middle, and not its side first
// (vr_axestick_angle) nor glancing (vr_axestick_incidence), fast enough (vr_axestick_speed) -- sticks in it with that part
// in (vr_axestick_depth), at the angle it came in at: the level and brush entities (doors, lifts), the props (Box3D's: it
// rides with the prop) and the monsters (their models as drawn, vr_hitmodel.cpp: it rides with the triangle it went
// into). Anything else, a flat or handle-first throw, bounces off as before (Box3D), and an axe never sticks after its
// first contact. QC (QC/vr_axestick.qc) does the rest: the blow, the bleeding, the fall when the monster dies or the
// prop goes, the pull by a hand or a force grab.
//
// Which models have blades, and where, is a table here (the axe: progs/v_axe.mdl, a double-bitted head).

#pragma once

#include "vr_engine.hpp"

#include <glm/glm.hpp>

namespace qvr::axestick
{

// Box3D's step, for each awake prop before it (vr_box3d.cpp's beforeStep; `com` its centre of mass, `vel` its velocity
// in units/s, `spin` its angular velocity in rad/s, world axes, `dt` the step): a thrown axe whose blade meets something
// this step as above is stuck in it (its pose, the .vr_stick_* fields, QC's VR_AxeStick_Stuck). True if it stuck: it
// is no longer a prop (QC made it MOVETYPE_NONE).
bool beforeStep(edict_t* ent, const glm::vec3& com, const glm::vec3& vel, const glm::vec3& spin, float dt);

// Once a server frame, after everything moved (VR_ServerFrameEnd, after the monsters' drawn poses are kept): each stuck
// axe placed where what it is in has taken it (its frame times the axe's pose in it), rocked by a tug
// (.vr_stick_wiggle), its blade's edge and facing written for QC (.vr_stick_at, .vr_stick_dir).
void serverFrame();

// Embedded Blades: Why (vr_debug_embed): each blade drawn, and the panel of reasons.
void debugDraw(const glm::vec3& eye, const glm::vec3& headAngles);

// A line on its panel, for a few seconds (vr_debug_embed on).
void say(const char* fmt, ...);

// Embedded blades, stage 2 (QC/vr_embedhold.qc): the blade `ent` (a thrown weapon just made, at rest) a hand's blow has just
// put into the body `host` at `at`, going at `vel` (units/s). Set where the hand's weapon is drawn (`hand`; a player not the
// local one: where QC put it), its checks made (an axe's edge first, not its side; a plate: its spark): stuck, by its point
// or its edge, with QC's VR_AxeStick_Stuck called. False: no, QC removes it and the hand keeps its weapon.
[[nodiscard]] int stickHeld(edict_t* ent, edict_t* host, const glm::vec3& at, const glm::vec3& vel, int hand);

// Server: the player's hands' blades as stats (STAT_QVR_EMBEDOFF, STAT_QVR_EMBEDMAIN).
void calcStats(edict_t* player, int* statsi);

} // namespace qvr::axestick
